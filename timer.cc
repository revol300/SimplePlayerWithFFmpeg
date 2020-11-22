#include "timer.h"

Timer::Timer() : current_time_(0) {}

Timer* Timer::getInstance() {
  if(!instance_) {
    cout << "create timer" << endl;
    instance_ = new Timer();
  }
  return instance_;
}

int64_t Timer::getTime() {return current_time_;}
void Timer::setTime(int64_t time) {
  lock_guard<mutex> lock_guard(lock_);
  current_time_ = time;
}//unit : microsecond

Timer* Timer::instance_ = nullptr;
