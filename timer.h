#ifndef TIMER_H
#define TIMER_H
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <SDL.h>
#include <SDL_thread.h>
#include <cassert>
}

#include <iostream>
#include <memory>
#include <mutex>

using namespace std;

class Timer {
  public:
    static Timer* getInstance();
    int64_t getTime();
    void setTime(int64_t time);
  private:
    Timer();
    static Timer* instance_;
    mutex lock_;
    int64_t current_time_;
};

#endif //TIMER_H
