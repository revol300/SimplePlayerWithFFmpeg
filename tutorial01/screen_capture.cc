#include "screen_capture.h"

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  
  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
  
  // Close file
  fclose(pFile);
}

ScreenCapture::ScreenCapture() : fmt_ctx_(NULL),
                                 video_index_(-1),
                                 audio_index_(-1),
                                 subtitle_index_(-1),
                                 video_codec_(nullptr),
                                 audio_codec_(nullptr),
                                 video_codec_context_(nullptr) {}

ScreenCapture::~ScreenCapture() {
  if(fmt_ctx_)
    avformat_close_input(&fmt_ctx_);
  if(video_codec_context_)
    avcodec_free_context(&video_codec_context_);
}

void ScreenCapture::setURL(std::string const& filename) {
  file_path_ = filename;
}

void ScreenCapture::captureFrames(int frame_count) {
  AVPacket pkt;
  AVFrame* pFrame=av_frame_alloc();
  int frameFinished;
  for(int i=0; i<frame_count; i++) {
    int ret = av_read_frame(fmt_ctx_, &pkt);
    if(ret == AVERROR_EOF) {
      // No more packets to read.
      printf("End of frame\n");
      break;
    }
    if(pkt.stream_index == video_index_) {
      /* // FIXME: this function can crash with bad packets
       * auto used = avcodec_decode_video2(video_codec_context_, pFrame, &frameFinished, &pkt); */
      auto used = avcodec_send_packet(video_codec_context_, &pkt);
      frameFinished = 0;
      if (!(used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF)) {
        if (used >= 0)
          pkt.size = 0;
        used = avcodec_receive_frame(video_codec_context_, pFrame);
        if (used >= 0)
          frameFinished = 1;
      }

      if(frameFinished) {
        /* https://medium.com/@moony211/av-frame-free에-대한-고찰-8c0b416e2082 */
        // Allocate an AVFrame structure
        AVFrame* pFrameRGB = nullptr;
        pFrameRGB=av_frame_alloc();
        if(pFrameRGB==nullptr)
          return;

        int numBytes = av_image_alloc(pFrameRGB->data,
            pFrameRGB->linesize,
            video_codec_context_->width,
            video_codec_context_->height,
            AV_PIX_FMT_RGB24,
            32
            );
        struct SwsContext *sws_ctx = NULL;
        sws_ctx = sws_getContext(video_codec_context_->width,
            video_codec_context_->height,
            video_codec_context_->pix_fmt,
            video_codec_context_->width,
            video_codec_context_->height,
            AV_PIX_FMT_RGB24,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
            );
        // Convert the image from its native format to RGB
        sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
            pFrame->linesize, 0, video_codec_context_->height,
            pFrameRGB->data, pFrameRGB->linesize);
        SaveFrame(pFrameRGB, video_codec_context_->width, video_codec_context_->height, i);
        av_freep(&pFrameRGB->data[0]);
        av_frame_free(&pFrameRGB);
        printf("Video packet\n");
      }
    } else if(pkt.stream_index == audio_index_) {
      auto used = avcodec_send_packet(audio_codec_context_, &pkt);
      if (!(used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF)) {
        if (used >= 0)
          pkt.size = 0;
        used = avcodec_receive_frame(audio_codec_context_, pFrame);
        if (used >= 0)
          frameFinished = 1;
      }
      printf("Audio packet\n");
    }
    av_packet_unref(&pkt);
  }
  av_frame_free(&pFrame);
}

int ScreenCapture::init() {

  //this function only looks at the header
  if(avformat_open_input(&fmt_ctx_, file_path_.c_str(), NULL, NULL) < 0) {
    cout << "Could not open input file " <<   file_path_ << endl;
    return -1;
  }

  if(avformat_find_stream_info(fmt_ctx_, NULL) < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -2;
  }

  video_index_ = av_find_best_stream	(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if(video_index_ == AVERROR_STREAM_NOT_FOUND)
    video_index_ = -1;

  audio_index_ = av_find_best_stream	(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if(audio_index_ == AVERROR_STREAM_NOT_FOUND)
    audio_index_ = -1;

  if(video_index_ < 0 && audio_index_ < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }

  video_codec_ = avcodec_find_decoder(fmt_ctx_->streams[video_index_]->codecpar->codec_id);
  audio_codec_ = avcodec_find_decoder(fmt_ctx_->streams[audio_index_]->codecpar->codec_id);

  if(video_codec_ == NULL || audio_codec_ == NULL){
    cout << "Failed to find codec" << endl;
  }

  // Get the codec context
  video_codec_context_ = avcodec_alloc_context3(video_codec_);
  if (!video_codec_context_){
    cout << "Out of memory" << endl;
    return -4;
  }

  // Set the parameters of the codec context from the stream
  int result = avcodec_parameters_to_context(
      video_codec_context_,
      fmt_ctx_->streams[video_index_]->codecpar);
  if(result < 0){
    return -5;
  }

  if(avcodec_open2(video_codec_context_, video_codec_, NULL)<0)
    return -1; // Could not open codec

  // Get the codec context
  audio_codec_context_ = avcodec_alloc_context3(audio_codec_);
  if (!audio_codec_context_){
    cout << "Out of memory" << endl;
    return -4;
  }

  // Set the parameters of the codec context from the stream
  result = avcodec_parameters_to_context(
      audio_codec_context_,
      fmt_ctx_->streams[audio_index_]->codecpar);
  if(result < 0){
    return -5;
  }

  if(avcodec_open2(audio_codec_context_, audio_codec_, NULL)<0)
    return -1; // Could not open codec

  return 0;
}

int main(int argc, char* argv[]) {
  int ret;

  av_log_set_level(AV_LOG_DEBUG);

  if(argc < 2) {
    printf("usage : %s <input>\n", argv[0]);
    return 0;
  }

  shared_ptr<ScreenCapture> capture_tool = make_shared<ScreenCapture>();
  capture_tool->setURL(argv[1]);

  if(capture_tool->init() < 0) {
    cout << "Failed to init capture tool" << endl;
    return -1;
  }

  // AVPacket is used to store packed stream data.
  capture_tool->captureFrames(500);
  
  return 0;

}
