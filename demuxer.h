#ifndef DEMUXER
#define DEMUXER

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <stdio.h>
}
#include <string>

class Demuxer{
public:
  Demuxer();
  ~Demuxer();

  void init();
private:
  std::string file_path_;
  AVCodecContext* fmt_ctx_; // this must be media descriptor

};

#endif //DEMUXER
