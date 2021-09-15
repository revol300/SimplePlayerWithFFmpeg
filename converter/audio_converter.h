#ifndef AUDIO_CONVERTER 
#define AUDIO_CONVERTER 

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <SDL.h>
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
  int getFrame(AVFrame *af, std::shared_ptr<uint8_t*>& audio_buf);
private:
  AudioParams audio_hw_params_;
  std::shared_ptr<AVCodecContext> audio_codec_context_;
  std::shared_ptr<SwrContext> swr_ctx_;
};

#endif // AUDIO_CONVERTER
