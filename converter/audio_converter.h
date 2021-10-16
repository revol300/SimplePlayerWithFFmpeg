#ifndef AUDIO_CONVERTER 
#define AUDIO_CONVERTER 

#include "audio_struct.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <SDL.h>
}
#include <string>
#include <memory>

class AudioConverter {
public:
  AudioConverter(std::shared_ptr<AVCodecContext> &audio_codec_context);
  ~AudioConverter();
  bool init();
  int getFrame(AVFrame *af, std::shared_ptr<AudioFrame>& audio_buf);
private:
  AudioParams audio_hw_params_;
  std::shared_ptr<AVCodecContext> audio_codec_context_;
  std::shared_ptr<SwrContext> swr_ctx_;
};

#endif // AUDIO_CONVERTER
