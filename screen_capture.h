#ifndef SCREEN_CAPTURE_H_ 
#define SCREEN_CAPTURE_H_ 

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#include <string>
#include <iostream>
#include <memory>

using namespace std;

class ScreenCapture {
public:
  ScreenCapture();
  ~ScreenCapture();

  void setURL(std::string const& filename);
  void release();
  void captureFrames(int frame_count);
  int init();
private:
  std::string file_path_;
  int video_index_;
  int audio_index_;
  int subtitle_index_;
  AVFormatContext* fmt_ctx_; // this must be media descriptor
  AVCodec* video_codec_;
  AVCodec* audio_codec_;
  AVCodecContext *video_codec_context_;
  AVCodecContext *audio_codec_context_;

};

#endif //DEMUXER
