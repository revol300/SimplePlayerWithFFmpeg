#include "video_player.h"

static SDL_AudioDeviceID audio_dev;

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
VideoPlayer::VideoPlayer() : fmt_ctx_(NULL),
                             video_index_(-1),
                             audio_index_(-1),
                             video_codec_(nullptr),
                             audio_codec_(nullptr),
                             video_codec_context_(nullptr),
                             audio_codec_context_(nullptr),
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
                             uvPitch(0) {}

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

  audio_index_ = av_find_best_stream	(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if(audio_index_ == AVERROR_STREAM_NOT_FOUND)
    audio_index_ = -1;

  if(video_index_ < 0 && audio_index_ < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }

  video_codec_ = avcodec_find_decoder(fmt_ctx_->streams[video_index_]->codecpar->codec_id);
  audio_codec_ = avcodec_find_decoder(fmt_ctx_->streams[audio_index_]->codecpar->codec_id);

  if(video_codec_ == NULL || audio_codec_ == NULL){
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

  // Get the codec context
  audio_codec_context_ = avcodec_alloc_context3(audio_codec_);
  if (!audio_codec_context_){
    cout << "Out of memory" << endl;
    return -4;
  }

  // Set the parameters of the codec context from the stream
  result = avcodec_parameters_to_context(
      audio_codec_context_,
      fmt_ctx_->streams[audio_index_]->codecpar);
  if(result < 0){
    return -5;
  }

  if(avcodec_open2(audio_codec_context_, audio_codec_, NULL)<0)
    return -1; // Could not open codec

  return 0;

}

int VideoPlayer::openWindow() {
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

/* static void sdl_audio_callback(void *opaque, Uint8 *stream, int len) {
 *     VideoState *is = opaque;
 *     int audio_size, len1;
 *
 *     audio_callback_time = av_gettime_relative();
 *
 *     while (len > 0) {
 *         if (is->audio_buf_index >= is->audio_buf_size) {
 *            audio_size = audio_decode_frame(is);
 *            if (audio_size < 0) {
 *                 [> if error, just output silence <]
 *                is->audio_buf = NULL;
 *                is->audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
 *            } else {
 *                if (is->show_mode != SHOW_MODE_VIDEO)
 *                    update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
 *                is->audio_buf_size = audio_size;
 *            }
 *            is->audio_buf_index = 0;
 *         }
 *         len1 = is->audio_buf_size - is->audio_buf_index;
 *         if (len1 > len)
 *             len1 = len;
 *         if (!is->muted && is->audio_buf && is->audio_volume == SDL_MIX_MAXVOLUME)
 *             memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
 *         else {
 *             memset(stream, 0, len1);
 *             if (!is->muted && is->audio_buf)
 *                 SDL_MixAudioFormat(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, AUDIO_S16SYS, len1, is->audio_volume);
 *         }
 *         len -= len1;
 *         stream += len1;
 *         is->audio_buf_index += len1;
 *     }
 *     is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
 *     [> Let's assume the audio driver that is used by SDL has two periods. <]
 *     if (!isnan(is->audio_clock)) {
 *         set_clock_at(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
 *         sync_clock_to_slave(&is->extclk, &is->audclk);
 *     }
 * }
 *
 * static int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params) {
 *     SDL_AudioSpec wanted_spec, spec;
 *     const char *env;
 *     static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
 *     static const int next_sample_rates[] = {0, 44100, 48000, 96000, 192000};
 *     int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;
 *
 *     env = SDL_getenv("SDL_AUDIO_CHANNELS");
 *     if (env) {
 *         wanted_nb_channels = atoi(env);
 *         wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
 *     }
 *     if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
 *         wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
 *         wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
 *     }
 *     wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
 *     wanted_spec.channels = wanted_nb_channels;
 *     wanted_spec.freq = wanted_sample_rate;
 *     if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
 *         av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
 *         return -1;
 *     }
 *     while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
 *         next_sample_rate_idx--;
 *     wanted_spec.format = AUDIO_S16SYS;
 *     wanted_spec.silence = 0;
 *     wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
 *     wanted_spec.callback = sdl_audio_callback;
 *     wanted_spec.userdata = opaque;
 *     while (!(audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
 *         av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
 *                wanted_spec.channels, wanted_spec.freq, SDL_GetError());
 *         wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
 *         if (!wanted_spec.channels) {
 *             wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
 *             wanted_spec.channels = wanted_nb_channels;
 *             if (!wanted_spec.freq) {
 *                 av_log(NULL, AV_LOG_ERROR,
 *                        "No more combinations to try, audio open failed\n");
 *                 return -1;
 *             }
 *         }
 *         wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
 *     }
 *     if (spec.format != AUDIO_S16SYS) {
 *         av_log(NULL, AV_LOG_ERROR,
 *                "SDL advised audio format %d is not supported!\n", spec.format);
 *         return -1;
 *     }
 *     if (spec.channels != wanted_spec.channels) {
 *         wanted_channel_layout = av_get_default_channel_layout(spec.channels);
 *         if (!wanted_channel_layout) {
 *             av_log(NULL, AV_LOG_ERROR,
 *                    "SDL advised channel count %d is not supported!\n", spec.channels);
 *             return -1;
 *         }
 *     }
 *
 *     audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
 *     audio_hw_params->freq = spec.freq;
 *     audio_hw_params->channel_layout = wanted_channel_layout;
 *     audio_hw_params->channels =  spec.channels;
 *     audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
 *     audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
 *     if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
 *         av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
 *         return -1;
 *     }
 *     return spec.size;
 * } */


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
    } else if (packet.stream_index == audio_index_) {
      
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
