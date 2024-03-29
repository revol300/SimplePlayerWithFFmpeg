#ifndef RENDERER_VIDEO_RENDERER_H_
#define RENDERER_VIDEO_RENDERER_H_

extern "C" {
#include <SDL.h>
#include <libavformat/avformat.h>
}

#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>

class VideoRenderer {
public:
  VideoRenderer(std::shared_ptr<AVCodecContext> &video_codec_context,
                AVRational time_base);
  ~VideoRenderer();
  bool init();
  int getFrame(std::shared_ptr<AVFrame> &in);
  void quit();
  void runLoop();
  void stop();
  void start();
  void flush();
  uint64_t getFrameTime(AVFrame *frame);

private:
  std::queue<std::shared_ptr<AVFrame>> frame_queue_;
  std::atomic<bool> running;
  std::future<void> handle;
  std::mutex mutex_;
  std::condition_variable cv_;
  AVRational time_base_;
  std::shared_ptr<AVCodecContext> video_codec_context_;
  SDL_Renderer *renderer_;
  SDL_Window *screen_;
  SDL_Texture *texture_;
};

#endif // RENDERER_VIDEO_RENDERER_H_
