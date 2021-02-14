#include "screen_capture.h"

int Demuxer::init(string file_path) {

  //this function only looks at the header
  AVFormatContext* fmt_ctx = nullptr;
  if (avformat_open_input(&fmt_ctx, file_path.c_str(), nullptr, nullptr) != 0) {
    cout << "Could not open input file " <<   file_path << endl;
    return -1;
  }

  if(avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -2;
  }

  fmt_ctx_ = std::shared_ptr<AVFormatContext>(
      fmt_ctx,
      [](AVFormatContext* p){
        if (p) avformat_close_input(&p);
      }
  );

  
  video_index_ = av_find_best_stream	(fmt_ctx_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if(video_index_ == AVERROR_STREAM_NOT_FOUND)
    video_index_ = -1;

  audio_index_ = av_find_best_stream	(fmt_ctx_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if(audio_index_ == AVERROR_STREAM_NOT_FOUND)
    audio_index_ = -1;

  if(video_index_ < 0 && audio_index_ < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }

  auto video_codec_ = avcodec_find_decoder(fmt_ctx_->streams[video_index_]->codecpar->codec_id);
  auto audio_codec_ = avcodec_find_decoder(fmt_ctx_->streams[audio_index_]->codecpar->codec_id);

  if(video_codec_ == NULL || audio_codec_ == NULL){
    cout << "Failed to find codec" << endl;
  }

  /* // Get the codec context
   * video_codec_context_ = avcodec_alloc_context3(video_codec_);
   * if (!video_codec_context_){
   *   cout << "Out of memory" << endl;
   *   return -4;
   * } */

/*   // Set the parameters of the codec context from the stream
 *   int result = avcodec_parameters_to_context(
 *       video_codec_context_,
 *       fmt_ctx_->streams[video_index_]->codecpar);
 *   if(result < 0){
 *     return -5;
 *   }
 *
 *   if(avcodec_open2(video_codec_context_, video_codec_, NULL)<0)
 *     return -1; // Could not open codec
 *
 *   // Get the codec context
 *   audio_codec_context_ = avcodec_alloc_context3(audio_codec_);
 *   if (!audio_codec_context_){
 *     cout << "Out of memory" << endl;
 *     return -4;
 *   }
 *
 *   // Set the parameters of the codec context from the stream
 *   result = avcodec_parameters_to_context(
 *       audio_codec_context_,
 *       fmt_ctx_->streams[audio_index_]->codecpar);
 *   if(result < 0){
 *     return -5;
 *   }
 *
 *   if(avcodec_open2(audio_codec_context_, audio_codec_, NULL)<0)
 *     return -1; // Could not open codec */

  return 0;
}

