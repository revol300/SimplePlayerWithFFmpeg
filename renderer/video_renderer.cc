#include "video_renderer.h"

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../timer.h"

using std::cout;
using std::endl;

VideoRenderer::VideoRenderer(std::shared_ptr<AVCodecContext>& video_codec_context, AVRational time_base) : video_codec_context_(video_codec_context), time_base_(time_base) {}

VideoRenderer::~VideoRenderer() {}

bool VideoRenderer::init() {
  if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    cout << "Could not initialize SDL - " << SDL_GetError() <<endl;
    return false;
  }

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
    return false;
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
  handle = std::async(std::launch::async, &VideoRenderer::runLoop, this);
}

void VideoRenderer::runLoop() {
  running = true;
  while(running) {
#ifdef _WIN32
    Sleep(1);
#else
    usleep(1000);
#endif
    mutex_.lock();
    if(frame_queue_.size() > 0) {
      std::shared_ptr<AVFrame> in = frame_queue_.front();
      //@NOTE : time unit ms
      if((getFrameTime(in.get()) + 200)< Timer::getInstance()->getAudioTime()) {
        frame_queue_.pop();
      } else if (getFrameTime(in.get())  < Timer::getInstance()->getAudioTime()) {
        SDL_UpdateYUVTexture(
            texture_,
            NULL,
            in->data[0],
            in->linesize[0],
            in->data[1],
            in->linesize[1],
            in->data[2],
            in->linesize[2]
            );
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, NULL, NULL);
        SDL_RenderPresent(renderer_);
        frame_queue_.pop();
      }
    }
    mutex_.unlock();
  }
}

int VideoRenderer::getFrame(std::shared_ptr<AVFrame>& in) {
  mutex_.lock();
  frame_queue_.push(in);
  mutex_.unlock();
  return 0;
}

void VideoRenderer::stop() {
  running = false;
  handle.get();
}

void VideoRenderer::start() {
  running = true;
  handle = std::async(std::launch::async, &VideoRenderer::runLoop, this);
}

void VideoRenderer::flush() {
  stop();
  mutex_.lock();
  frame_queue_ = std::queue<std::shared_ptr<AVFrame> > ();
  mutex_.unlock();
  start();
}

void VideoRenderer::quit() {
  running = false;
  handle.get();
  SDL_DestroyTexture(texture_);
  SDL_DestroyRenderer(renderer_);
  SDL_DestroyWindow(screen_);
}

uint64_t VideoRenderer::getFrameTime(AVFrame* frame) {
  return av_q2d(time_base_)*frame->pts*1000;
}
