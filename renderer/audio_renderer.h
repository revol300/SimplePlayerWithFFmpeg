#ifndef AUDIO_RENDERER
#define AUDIO_RENDERER

extern "C" {
  #include <libavformat/avformat.h>
  #include <SDL.h>
}

#include <memory>
#include <mutex>
#include <queue>

#include "../converter/audio_struct.h"

static void audio_callback(void *userdata, Uint8 * stream, int len);

class AudioRenderer {
public:
  AudioRenderer(std::shared_ptr<AVCodecContext>& audio_codec_context, AVRational time_base);
  ~AudioRenderer();
  bool init();
  int getFrame(std::shared_ptr<AudioFrame>& in);
  int getAudioData(uint8_t *audio_buf, int buf_size);
  std::queue<std::shared_ptr<AudioFrame> > frame_queue_;
  uint64_t getAudioTime() {return audio_clock_;};
  uint64_t bytesToMilisecond(uint32_t len) {
    return (len*1000)/audio_hw_params_.bytes_per_sec;
  }
  void stop() {
    SDL_PauseAudioDevice(dev_, 1);
  }
  void start() {
    SDL_PauseAudioDevice(dev_, 0);
  }

  void flush() {
    stop();
    lk_.lock();
    frame_queue_ = std::queue<std::shared_ptr<AudioFrame> > ();
    lk_.unlock();
    start();
  }
private:
  int64_t audio_clock_;
  AVRational time_base_;
  std::shared_ptr<AVCodecContext> audio_codec_context_;
  std::mutex lk_;
  AudioParams audio_hw_params_;
  int dev_;
};

#endif //VIDEO_RENDERER
