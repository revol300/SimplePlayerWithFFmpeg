#include "multimedia_player.h"
#include "timer.h"
#include <unistd.h> 
#define VIDEO_RENDER_EVENT (SDL_USEREVENT)

MultimediaPlayer::MultimediaPlayer() : video_player_(nullptr), audio_player_(nullptr), fmt_ctx_(NULL) {}

int MultimediaPlayer::openFile(string const& filename) {
  file_path_ = filename;
  //this function only looks at the header
  int ret = avformat_open_input(&fmt_ctx_, file_path_.c_str(), NULL, NULL);
  if(ret < 0) {
    char error_msg[256];
    av_make_error_string(error_msg, 256, ret);
    cout << "Could not open input file " <<   file_path_ << " error_msg : "<< error_msg << endl;
    return -1;
  }
  ret = avformat_find_stream_info(fmt_ctx_, NULL);
  if(ret < 0) {
    cout << "Failed to retrieve input stream information : " << endl;
    return -2;
  }

  initVideoPlayer();
  initAudioPlayer();
  return 0;
}

int MultimediaPlayer::initVideoPlayer() {
  int video_index_ = av_find_best_stream	(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if(video_index_ == AVERROR_STREAM_NOT_FOUND)
    video_index_ = -1;
  if(video_index_ < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }

  auto video_codec_ = avcodec_find_decoder(fmt_ctx_->streams[video_index_]->codecpar->codec_id);
  if(video_codec_ == NULL){
    cout << "Failed to find video codec" << endl;
  }

  // Get the codec context
  auto video_codec_context_ = avcodec_alloc_context3(video_codec_);
  if (!video_codec_context_){
    cout << "Out of memory" << endl;
    return -4;
  }

  // Set the parameters of the codec context from the stream
  int result = avcodec_parameters_to_context(
      video_codec_context_,
      fmt_ctx_->streams[video_index_]->codecpar);
  if(result < 0){
    return -5;
  }

  if(avcodec_open2(video_codec_context_, video_codec_, NULL)<0)
    return -1; // Could not open codec

  video_player_ = new VideoPlayer(video_index_, video_codec_, video_codec_context_, fmt_ctx_->streams[video_index_]->time_base);
  return 0;
}

int MultimediaPlayer::initAudioPlayer() {
  int audio_index = av_find_best_stream	(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if(audio_index == AVERROR_STREAM_NOT_FOUND)
    audio_index = -1;

  if(audio_index < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }
  auto audio_codec = avcodec_find_decoder(fmt_ctx_->streams[audio_index]->codecpar->codec_id);
  if(audio_codec == NULL){
    cout << "Failed to find audio codec" << endl;
  }

  // Get the codec context
  auto audio_codec_context = avcodec_alloc_context3(audio_codec);
  if (!audio_codec_context){
    cout << "Out of memory" << endl;
    return -4;
  }

  // Set the parameters of the codec context from the stream
  int result = avcodec_parameters_to_context(
      audio_codec_context,
      fmt_ctx_->streams[audio_index]->codecpar);
  if(result < 0){
    return -5;
  }

  if(avcodec_open2(audio_codec_context, audio_codec, NULL)<0)
    return -1; // Could not open codec

  audio_player_ = new AudioPlayer(audio_index, audio_codec, audio_codec_context);
  return 0;
}

int MultimediaPlayer::openWindow() {
  video_player_->initRenderer();
  audio_player_->initRenderer();
  return 0;
}

static Uint32 videoRenderCallback(Uint32 interval, void *userdata) {
  MultimediaPlayer* player = (MultimediaPlayer*) userdata;
  SDL_Event event;
  event.type = VIDEO_RENDER_EVENT;
  SDL_PushEvent(&event);
  int delay = player->getVideoFrame();
  if(delay <= 0){
    return 1;
  } else {
    cout << "delay : " << delay << endl;
    return delay;
  }
  return 1; /* 0 means stop timer */
}

int MultimediaPlayer::getVideoFrame() {
  AVFrame* frame = video_player_->getFrame();
  cout << "current time : " << Timer::getInstance()->getTime() << endl;
  if(frame) {
    int interval = video_player_->getFrameTime(frame) - Timer::getInstance()->getTime();
    cout << "pts time : " << video_player_->getFrameTime(frame) << endl;
    return interval;
  } else {
    return -1;
  }
}

int MultimediaPlayer::play() {
  AVPacket packet;
  bool audio_start = false;
  SDL_AddTimer(1, videoRenderCallback, this);
  /* SDL_CreateThread(renderVideo, "render video" ,video_player_); */
  while (true) {
    //@NOTE : demuxing과정을 class로 빼야할 필요 있음
    if (av_read_frame(fmt_ctx_, &packet) >= 0) {
      // Is this a packet from the video stream?
      if (packet.stream_index == video_player_->getVideoIndex()) {
        video_player_->addPacket(packet);
      } else if (packet.stream_index == audio_player_->getAudioIndex()) {
        audio_player_->addPacket(packet);
        if(!audio_start) {
          audio_player_->start();
          audio_start=true;
        }
        /* cout << "audio packet!!!" << endl; */
      }
    }
    av_packet_unref(&packet);
    //PollEvent를 해야 화면이 뜸:
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        video_player_->quit();
        SDL_Quit();
        exit(0);
        return 0;
      case VIDEO_RENDER_EVENT: {
        video_player_->render();
        break;
      }
      default:
        break;
    }
  }
  return 0;
}

int main(int argc, char* argv[]) {
  int ret;

  av_register_all(); //@NOTE: For FFmpeg version < 4.0
  /* av_log_set_level(AV_LOG_DEBUG); */

  if(argc < 2) {
    printf("usage : %s <input>\n", argv[0]);
    return 0;
  }

  shared_ptr<MultimediaPlayer> multimedia_player = make_shared<MultimediaPlayer>();
  multimedia_player->openFile(argv[1]);

  multimedia_player->openWindow();

  multimedia_player->play();

  return 0;
}
