#ifndef VIDEO_CONVERTER
#define VIDEO_CONVERTER

extern "C" {
  #include <libavformat/avformat.h>
  #include <libswscale/swscale.h>
  #include <libavutil/imgutils.h>
}

#include <memory>

using SwsContext = struct SwsContext;

class VideoConverter {
public:
  VideoConverter(std::shared_ptr<AVCodecContext> &video_codec_context);
  ~VideoConverter();
  bool init();
  int getFrame(std::shared_ptr<AVFrame>& in, std::shared_ptr<AVFrame>& out);
private:
  std::shared_ptr<AVCodecContext> video_codec_context_;
  SwsContext* sws_ctx_;
};

#endif //VIDEO_CONVERTER
