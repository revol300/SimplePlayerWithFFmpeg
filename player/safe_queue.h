#ifndef PLAYER_SAFE_QUEUE_H_
#define PLAYER_SAFE_QUEUE_H_

#include <memory>
#include <queue>

template <typename T> class SafeQueue {
public:
  void push(std::shared_ptr<T> &in) {
    mutex_.lock();
    queue_.push(in);
    mutex_.unlock();
  }

  std::shared_ptr<T> front() {
    std::shared_ptr<T> temp;
    mutex_.lock();
    if (queue_.size() > 0) {
      temp = queue_.front();
      queue_.pop();
    } else {
      temp = nullptr;
    }
    mutex_.unlock();
    return temp;
  }

  int size() { return queue_.size(); }

  void flush() {
    mutex_.lock();
    queue_ = std::queue<std::shared_ptr<T>>();
    mutex_.unlock();
  }

private:
  std::queue<std::shared_ptr<T>> queue_;
  std::mutex mutex_;
};

#endif // PLAYER_SAFE_QUEUE_H_
