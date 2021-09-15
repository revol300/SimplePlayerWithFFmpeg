#ifndef DECODER 
#define DECODER 

extern "C" {
#include <libavformat/avformat.h>
}
#include <string>

class Decoder{
public:
  Decoder(int stream_index, std::shared_ptr<AVFormatContext>& fmt_ctx);
  ~Decoder();
  void init();
  bool getFrame(std::shared_ptr<AVPacket>& packet, std::shared_ptr<AVFrame>& frame);
  int getIndex() {return stream_index_;}
  void getCodecContext(std::shared_ptr<AVCodecContext> &codec_context) {codec_context = codec_context_;};
private:
  int stream_index_;
  std::shared_ptr<AVCodecContext> codec_context_;
  std::shared_ptr<AVFrame> pFrame;
  std::shared_ptr<AVFormatContext> fmt_ctx_;
};

#endif // DECODER
