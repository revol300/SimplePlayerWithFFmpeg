#ifndef CONVERTER_VIDEO_CONVERTER_H_
#define CONVERTER_VIDEO_CONVERTER_H_

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <memory>

class VideoConverter {
public:
  explicit VideoConverter(std::shared_ptr<AVCodecContext> &video_codec_context);
  ~VideoConverter();
  bool init();
  int getFrame(std::shared_ptr<AVFrame> &in, std::shared_ptr<AVFrame> &out);

private:
  std::shared_ptr<AVCodecContext> video_codec_context_;
  SwsContext *sws_ctx_;
};

#endif // CONVERTER_VIDEO_CONVERTER_H_
