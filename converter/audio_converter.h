#ifndef AUDIO_CONVERTER 
#define AUDIO_CONVERTER 

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <SDL.h>
#include <SDL_thread.h>

}
#include <string>

typedef struct AudioParams {
  int freq;
  int channels;
  int64_t channel_layout;
  enum AVSampleFormat fmt;
  int frame_size;
  int bytes_per_sec;
} AudioParams;

class AudioConverter {
public:
  AudioConverter(std::shared_ptr<AVCodecContext> &audio_codec_context);
  ~AudioConverter();
  bool init();
  bool getFrame(std::shared_ptr<AVFrame>& original_frame, std::shared_ptr<AVFrame>& converted_frame);
  int resample(AVFrame *af, uint8_t** audio_buf);
private:
  SDL_AudioSpec spec;
  AudioParams audio_hw_params_;
  int stream_index_;
  std::shared_ptr<AVCodecContext> audio_codec_context_;
  std::shared_ptr<AVFrame> pFrame;
  std::shared_ptr<SwrContext> swr_ctx_;
};

#endif // AUDIO_CONVERTER
