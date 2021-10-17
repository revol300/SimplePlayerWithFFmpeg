#include "converter/video_converter.h"

#include <iostream>

VideoConverter::VideoConverter(
    std::shared_ptr<AVCodecContext> &video_codec_context)
    : video_codec_context_(video_codec_context) {}

VideoConverter::~VideoConverter() {}

using SwsContext = struct SwsContext;
bool VideoConverter::init() {
  sws_ctx_ =
      sws_getContext(video_codec_context_->width, video_codec_context_->height,
                     video_codec_context_->pix_fmt, video_codec_context_->width,
                     video_codec_context_->height, AV_PIX_FMT_YUV420P,
                     SWS_BILINEAR, NULL, NULL, NULL);
  return true;
}

int VideoConverter::getFrame(std::shared_ptr<AVFrame> &in,
                             std::shared_ptr<AVFrame> &out) {
  AVFrame *pFrameYUV = av_frame_alloc();
  if (pFrameYUV == nullptr)
    return -1;
  int numBytes = av_image_alloc(
      pFrameYUV->data, pFrameYUV->linesize, video_codec_context_->width,
      video_codec_context_->height, AV_PIX_FMT_YUV420P, 32);
  pFrameYUV->pts = in->pts;
  sws_scale(sws_ctx_, in->data, in->linesize, 0, video_codec_context_->height,
            pFrameYUV->data, pFrameYUV->linesize);
  out = std::shared_ptr<AVFrame>(pFrameYUV, [](AVFrame *frame) {
    // std::cout << "converted_buffer freed (Video)" << std::endl;
    av_freep(&frame->data[0]);
    av_frame_free(&frame);
  });
  return 0;
}
