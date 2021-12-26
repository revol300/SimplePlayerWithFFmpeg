#include "decoder/decoder.h"

#include <iostream>

using std::cout;
using std::endl;

Decoder::Decoder(int stream_index, std::shared_ptr<AVFormatContext> &fmt_ctx)
    : stream_index_(stream_index), fmt_ctx_(fmt_ctx), codec_context_() {}

Decoder::~Decoder() {}

void Decoder::init() {
  // Do not have to free codec
  auto codec = avcodec_find_decoder(
      fmt_ctx_->streams[stream_index_]->codecpar->codec_id);
  if (codec == NULL) {
    cout << "Failed to find video codec" << endl;
  }

  // Get the codec context
  codec_context_ = std::shared_ptr<AVCodecContext>(
      avcodec_alloc_context3(codec),
      [](AVCodecContext *codec_context) { avcodec_close(codec_context); });
  if (!codec_context_.get()) {
    cout << "Out of memory" << endl;
  }

  // Set the parameters of the codec context from the stream
  int result = avcodec_parameters_to_context(
      codec_context_.get(), fmt_ctx_->streams[stream_index_]->codecpar);
  if (result < 0) {
    cout << "error : avcodec_paramters_to_context" << endl;
  }

  if (avcodec_open2(codec_context_.get(), codec, NULL) < 0) {
    cout << "error : could not open codec" << endl;
  }
}

bool Decoder::getFrame(std::shared_ptr<AVPacket> &packet,
                       std::shared_ptr<AVFrame> &frame) {
  frame = std::shared_ptr<AVFrame>(nullptr);
  auto used = avcodec_send_packet(codec_context_.get(), packet.get());
  if (!(used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF)) {
    AVFrame *pFrame = av_frame_alloc();
    used = avcodec_receive_frame(codec_context_.get(), pFrame);
    if (used >= 0) {
      frame = std::shared_ptr<AVFrame>(pFrame, [](AVFrame *pFrame) {
        std::cout << "av_frame_free : " << pFrame << std::endl;
        av_frame_free(&pFrame);
      });
    } else {
      av_frame_free(&pFrame);
    }
  }
  return true;
}

bool Decoder::getFrame(std::shared_ptr<AVPacket> &packet,
                       std::vector<std::shared_ptr<AVFrame>> &frame_list) {
  frame_list.clear();
  auto used = avcodec_send_packet(codec_context_.get(), packet.get());
  if (!(used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF)) {
    while (true) {
      AVFrame *pFrame = av_frame_alloc();
      used = avcodec_receive_frame(codec_context_.get(), pFrame);
      if (used >= 0) {
        frame_list.push_back(
            std::shared_ptr<AVFrame>(pFrame, [](AVFrame *pFrame) {
              std::cout << "av_frame_free : " << pFrame << std::endl;
              av_frame_free(&pFrame);
            }));
      } else {
        av_frame_free(&pFrame);
        break;
      }
    }
  }
  return true;
}

void Decoder::flush() { avcodec_flush_buffers(codec_context_.get()); }
