#include "timer.h"

Timer::Timer() : current_time_(0), origin_time_(av_gettime()/1000), audio_time_(0) {}

Timer* Timer::getInstance() {
  if(!instance_) {
    cout << "create timer" << endl;
    instance_ = new Timer();
  }
  return instance_;
}

int64_t Timer::getAudioTime() {
  return audio_time_;
}

int64_t Timer::setAudioTime(int64_t time) {
  audio_time_ = time;
  return time;
}

int64_t Timer::getTime() {return av_gettime()/1000;}
int64_t Timer::getRelativeTime() {return av_gettime()/1000-origin_time_;}
void Timer::setRelativeTime(int64_t time) {
  lock_guard<mutex> lock_guard(lock_);
  origin_time_ = av_gettime()/1000 - time;
}//unit : microsecond

Timer* Timer::instance_ = nullptr;
