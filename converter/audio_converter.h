#ifndef CONVERTER_AUDIO_CONVERTER_H_
#define CONVERTER_AUDIO_CONVERTER_H_

#include "converter/audio_struct.h"

extern "C" {
#include <SDL.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
#include <memory>
#include <string>

class AudioConverter {
public:
  explicit AudioConverter(std::shared_ptr<AVCodecContext> &audio_codec_context);
  ~AudioConverter();
  bool init();
  int getFrame(AVFrame *af, std::shared_ptr<AudioFrame> &audio_buf);

private:
  AudioParams audio_hw_params_;
  std::shared_ptr<AVCodecContext> audio_codec_context_;
  std::shared_ptr<SwrContext> swr_ctx_;
};

#endif // CONVERTER_AUDIO_CONVERTER_H_
