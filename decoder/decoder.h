#ifndef DECODER_DECODER_H_
#define DECODER_DECODER_H_

extern "C" {
#include <libavformat/avformat.h>
}
#include <memory>
#include <string>
#include <vector>

class Decoder {
public:
  Decoder(int stream_index, std::shared_ptr<AVFormatContext> &fmt_ctx);
  ~Decoder();
  void init();
  bool getFrame(std::shared_ptr<AVPacket> &packet,
                std::shared_ptr<AVFrame> &frame);
  bool getFrame(std::shared_ptr<AVPacket> &packet,
                std::vector<std::shared_ptr<AVFrame>> &frame_list);
  int getIndex() { return stream_index_; }
  void getCodecContext(std::shared_ptr<AVCodecContext> &codec_context) {
    codec_context = codec_context_;
  }
  void flush();

private:
  int stream_index_;
  std::shared_ptr<AVCodecContext> codec_context_;
  std::shared_ptr<AVFormatContext> fmt_ctx_;
};

#endif // DECODER_DECODER_H_
