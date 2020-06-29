#include "video_player.h"

VideoPlayer::VideoPlayer() : fmt_ctx_(NULL),
                             video_index_(-1),
                             video_codec_(nullptr),
                             video_codec_context_(nullptr),
                             sws_ctx(nullptr),
                             event(),
                             screen(nullptr),
                             renderer(nullptr),
                             texture(nullptr),
                             yPlane(nullptr),
                             uPlane(nullptr),
                             vPlane(nullptr),
                             yPlaneSz(0),
                             uvPlaneSz(0),
                             uvPitch(0)
{}

VideoPlayer::~VideoPlayer() {
  if(yPlane)
    free(yPlane);
  if(uPlane)
    free(uPlane);
  if(vPlane)
    free(vPlane);
  if(fmt_ctx_)
    avformat_close_input(&fmt_ctx_);
  if(video_codec_context_)
    avcodec_free_context(&video_codec_context_);
}

void VideoPlayer::setURL(string const& filename) {
  file_path_ = filename;
}

int VideoPlayer::openFile() {
  //this function only looks at the header
  if(avformat_open_input(&fmt_ctx_, file_path_.c_str(), NULL, NULL) < 0) {
    cout << "Could not open input file " <<   file_path_ << endl;
    return -1;
  }

  if(avformat_find_stream_info(fmt_ctx_, NULL) < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -2;
  }

  video_index_ = av_find_best_stream	(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if(video_index_ == AVERROR_STREAM_NOT_FOUND)
    video_index_ = -1;

  if(video_index_ < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }

  video_codec_ = avcodec_find_decoder(fmt_ctx_->streams[video_index_]->codecpar->codec_id);

  if(video_codec_ == NULL){
    cout << "Failed to find codec" << endl;
  }

  // Get the codec context
  video_codec_context_ = avcodec_alloc_context3(video_codec_);
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
  return 0;
}

int VideoPlayer::openWindow(){
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    cout << "Could not initialize SDL - " << SDL_GetError() <<endl;
    return -1;
  }
  // Make a screen to put our video
  screen = SDL_CreateWindow(
      "FFmpeg Tutorial",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      video_codec_context_->width,
      video_codec_context_->height,
      SDL_WINDOW_RESIZABLE
      );

  if (!screen) {
    fprintf(stderr, "SDL: could not create window - exiting\n");
    return -2;
  }

  renderer = SDL_CreateRenderer(screen, -1, 0);
  if (!renderer) {
    fprintf(stderr, "SDL: could not create renderer - exiting\n");
    return -3;
  }

  // Allocate a place to put our YUV image on that screen
  texture = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_YV12,
      SDL_TEXTUREACCESS_STREAMING,
      video_codec_context_->width,
      video_codec_context_->height
      );
  if (!texture) {
    fprintf(stderr, "SDL: could not create texture - exiting\n");
    return -4;
  }

  // initialize SWS context for software scaling
  sws_ctx = sws_getContext(video_codec_context_->width, video_codec_context_->height,
      video_codec_context_->pix_fmt, video_codec_context_->width, video_codec_context_->height,
      AV_PIX_FMT_YUV420P,
      SWS_BILINEAR,
      NULL,
      NULL,
      NULL);

  // set up YV12 pixel array (12 bits per pixel)
  yPlaneSz = video_codec_context_->width * video_codec_context_->height;
  uvPlaneSz = video_codec_context_->width * video_codec_context_->height / 4;
  yPlane = (Uint8*)malloc(yPlaneSz);
  uPlane = (Uint8*)malloc(uvPlaneSz);
  vPlane = (Uint8*)malloc(uvPlaneSz);
  if (!yPlane || !uPlane || !vPlane) {
    fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
    exit(1);
  }
  uvPitch = video_codec_context_->width / 2;
  return 0;
}

int VideoPlayer::init() {
  openFile();
  openWindow();
  return 0;
}

void VideoPlayer::play() {
  bool frameFinished = false;
  AVPacket packet;
  AVFrame *pFrame = NULL;
  pFrame = av_frame_alloc();
  /* https://medium.com/@moony211/av-frame-free에-대한-고찰-8c0b416e2082 */
  // Allocate an AVFrame structure
  AVFrame* pFrameYUV = nullptr;
  pFrameYUV=av_frame_alloc();
  if(pFrameYUV==nullptr)
    return;
  int numBytes = av_image_alloc(pFrameYUV->data,
      pFrameYUV->linesize,
      video_codec_context_->width,
      video_codec_context_->height,
      AV_PIX_FMT_YUV420P,
      32
      );
  AVPicture pict;
  pFrameYUV->data[0] = yPlane;
  pFrameYUV->data[1] = uPlane;
  pFrameYUV->data[2] = vPlane;
  pFrameYUV->linesize[0] = video_codec_context_->width;
  pFrameYUV->linesize[1] = uvPitch;
  pFrameYUV->linesize[2] = uvPitch;

  while (av_read_frame(fmt_ctx_, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == video_index_) {
      // Decode video frame
      auto used = avcodec_send_packet(video_codec_context_, &packet);
      if (!(used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF)) {
        if (used >= 0)
          packet.size = 0;
        used = avcodec_receive_frame(video_codec_context_, pFrame);
        if (used >= 0)
          frameFinished = true;
      }
      // Did we get a video frame?
      if (frameFinished) {
        // Convert the image into YUV format that SDL uses
        sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
            pFrame->linesize, 0, video_codec_context_->height,
            pFrameYUV->data, pFrameYUV->linesize);
        SDL_UpdateYUVTexture(
            texture,
            NULL,
            yPlane,
            video_codec_context_->width,
            uPlane,
            uvPitch,
            vPlane,
            uvPitch
            );
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
      }
    }
        // Free the packet that was allocated by av_read_frame
    av_packet_unref(&packet);
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(screen);
        SDL_Quit();
        exit(0);
        break;
      default:
        break;
    }
  }
  // Free the YUV frame
  av_freep(&pFrameYUV->data);
  av_frame_free(&pFrameYUV);
  av_frame_free(&pFrame);
}

int main(int argc, char* argv[]) {
  int ret;

  av_log_set_level(AV_LOG_DEBUG);

  if(argc < 2) {
    printf("usage : %s <input>\n", argv[0]);
    return 0;
  }

  shared_ptr<VideoPlayer> video_player = make_shared<VideoPlayer>();
  video_player->setURL(argv[1]);

  if(video_player->init() < 0) {
    cout << "Failed to init capture tool" << endl;
    return -1;
  }

  video_player->play();
  return 0;

}
