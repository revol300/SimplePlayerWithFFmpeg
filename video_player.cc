#include "video_player.h"

VideoPlayer::VideoPlayer(int video_index, AVCodec* video_codec, AVCodecContext* video_codec_context) : video_index_(video_index),
             video_codec_(video_codec),
             video_codec_context_(video_codec_context),
             pFrame(nullptr),
             pFrameYUV(nullptr) {
  
} 

int VideoPlayer::initRenderer() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    cout << "Could not initialize SDL - " << SDL_GetError() <<endl;
    return -1;
  }
// Make a screen to put our video
  screen_ = SDL_CreateWindow(
      "FFmpegSimplePlayer",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      video_codec_context_->width,
      video_codec_context_->height,
      SDL_WINDOW_RESIZABLE
      );

  if (!screen_) {
    fprintf(stderr, "SDL: could not create window - exiting\n");
    return -2;
  }

  renderer_ = SDL_CreateRenderer(screen_, -1, 0);
  if (!renderer_) {
    fprintf(stderr, "SDL: could not create renderer_ - exiting\n");
    return -3;
  }

  // Allocate a place to put our YUV image on that screen
  texture_ = SDL_CreateTexture(
      renderer_,
      SDL_PIXELFORMAT_YV12,
      SDL_TEXTUREACCESS_STREAMING,
      video_codec_context_->width,
      video_codec_context_->height
      );
  if (!texture_) {
    fprintf(stderr, "SDL: could not create texture - exiting\n");
    return -4;
  }

  // initialize SWS context for software scaling
  sws_ctx_ = sws_getContext(video_codec_context_->width, video_codec_context_->height,
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

  pFrame = av_frame_alloc();
  /* https://medium.com/@moony211/av-frame-free에-대한-고찰-8c0b416e2082 */
  // Allocate an AVFrame structure
  pFrameYUV=av_frame_alloc();
  if(pFrameYUV==nullptr)
    return -1;
  int numBytes = av_image_alloc(pFrameYUV->data,
      pFrameYUV->linesize,
      video_codec_context_->width,
      video_codec_context_->height, AV_PIX_FMT_YUV420P,
      32
      );
  AVPicture pict;
  pFrameYUV->data[0] = yPlane;
  pFrameYUV->data[1] = uPlane;
  pFrameYUV->data[2] = vPlane;
  pFrameYUV->linesize[0] = video_codec_context_->width;
  pFrameYUV->linesize[1] = uvPitch;
  pFrameYUV->linesize[2] = uvPitch;
  return 0;
}

void VideoPlayer::quit() {
  SDL_DestroyTexture(texture_);
  SDL_DestroyRenderer(renderer_);
  SDL_DestroyWindow(screen_);
}

void VideoPlayer::render() {
  std::lock_guard<std::mutex> lock_guard(queue_lock_);
  if(packet_queue_.size() <=0)
    return;
  AVPacket* packet = packet_queue_.front();

  cout << "render video!!" << endl;
  bool frameFinished = false;
  // Decode video frame
  auto used = avcodec_send_packet(video_codec_context_, packet);
  if (!(used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF)) {
    if (used >= 0)
      packet->size = 0;
    used = avcodec_receive_frame(video_codec_context_, pFrame);
    if (used >= 0)
      frameFinished = true;
  }
  // Did we get a video frame?
  if (frameFinished) {
    cout << "find frame!!" << endl;
    // Convert the image into YUV format that SDL uses
    sws_scale(sws_ctx_, (uint8_t const * const *)pFrame->data,
        pFrame->linesize, 0, video_codec_context_->height,
        pFrameYUV->data, pFrameYUV->linesize);
    SDL_UpdateYUVTexture(
        texture_,
        NULL,
        yPlane,
        video_codec_context_->width,
        uPlane,
        uvPitch,
        vPlane,
        uvPitch
        );
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, NULL, NULL);
    SDL_RenderPresent(renderer_);
  }
  av_packet_unref(packet);
  packet_queue_.pop();
  free(packet);
}

void VideoPlayer::addPacket(AVPacket& packet) {
   std::lock_guard<std::mutex> lock_guard(queue_lock_);
   AVPacket *copy = (AVPacket*)malloc(sizeof(struct AVPacket));
   av_init_packet(copy);
   if (av_packet_ref(copy, &packet) < 0)
     exit(-1);
   packet_queue_.push(copy);
}
