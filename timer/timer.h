#ifndef TIMER_TIMER_H_
#define TIMER_TIMER_H_
extern "C" {
#include <SDL.h>
#include <SDL_thread.h>
#include <cassert>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <iostream>
#include <memory>
#include <mutex>

class Timer {
public:
  static Timer *getInstance();
  int64_t getTime();
  int64_t getAudioTime();
  int64_t setAudioTime(int64_t time);
  int64_t getRelativeTime();
  void setRelativeTime(int64_t time);

private:
  Timer();
  static Timer *instance_;
  std::mutex lock_;
  int64_t current_time_;
  int64_t origin_time_;
  int64_t audio_time_;
};

#endif // TIMER_TIMER_H_
