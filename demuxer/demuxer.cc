#include "demuxer.h"

#include <iostream>
#include <memory>

using std::cout;
using std::endl;
using std::shared_ptr;

Demuxer::Demuxer(const std::string& file_path)
  : file_path_(file_path)
  , video_index_(-1)
  , audio_index_(-1) {}

Demuxer::~Demuxer() {}

int Demuxer::init() {
  AVFormatContext* fmt_ctx = nullptr;
  int ret = avformat_open_input(&fmt_ctx, file_path_.c_str(), NULL, NULL);
  if(ret < 0) {
    char error_msg[256];
    av_make_error_string(error_msg, 256, ret);
    cout << "Could not open input file " <<   file_path_ << " error_msg : "<< error_msg << endl;
    //@NOTE error_code needed
    return -1;
  }
  fmt_ctx_ = std::shared_ptr<AVFormatContext>(fmt_ctx);
  ret = avformat_find_stream_info(fmt_ctx_.get(), NULL);
  if(ret < 0) {
    cout << "Failed to retrieve input stream information : " << endl;
    return -2;
  }
  video_index_ = av_find_best_stream(fmt_ctx_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if(video_index_ == AVERROR_STREAM_NOT_FOUND)
    video_index_ = -1;
  if(video_index_ < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }

  audio_index_ = av_find_best_stream(fmt_ctx_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if(audio_index_ == AVERROR_STREAM_NOT_FOUND)
    audio_index_ = -1;

  if(audio_index_ < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }
}

bool Demuxer::getPacket(std::shared_ptr<AVPacket>& packet) {
  AVPacket* media_packet = av_packet_alloc();
  if (av_read_frame(fmt_ctx_.get(), media_packet) >= 0) {
    packet = std::shared_ptr<AVPacket>(media_packet, [](AVPacket* packet) {
        av_packet_free(&packet);
        cout << "packet free" << endl;
     });
     return true;
  } else {
      cout << "Invalid Packet" << endl;
    av_packet_free(&media_packet);
    return false;
  }
}
