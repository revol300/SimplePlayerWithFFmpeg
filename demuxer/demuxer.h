#ifndef DEMUXER_DEMUXER_H_
#define DEMUXER_DEMUXER_H_

extern "C" {
#include <libavformat/avformat.h>
}

#include <memory>
#include <string>

class Demuxer {
public:
  explicit Demuxer(const std::string &file_path);
  ~Demuxer();
  int init();
  bool getPacket(std::shared_ptr<AVPacket> &packet);
  int getVideoIndex() { return video_index_; }
  int getAudioIndex() { return audio_index_; }
  std::shared_ptr<AVFormatContext> &getFormatContext() { return fmt_ctx_; }
  // @NOTE: time_unit ==  microseconds
  void seek(int64_t time);

private:
  std::shared_ptr<AVFormatContext> fmt_ctx_;
  std::string file_path_;
  int video_index_;
  int audio_index_;
};

#endif // DEMUXER_DEMUXER_H_
