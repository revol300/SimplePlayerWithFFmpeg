#ifndef VIDEO_PLAYER_H_
#define VIDEO_PLAYER_H_
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <SDL.h>
#include <SDL_thread.h>
#include <cassert>
}

#include <iostream>
#include <memory>
#include <queue>
using namespace std;

class VideoPlayer {
public:
  VideoPlayer(int video_index, AVCodec* video_codec, AVCodecContext* video_codec_context);
  int initRenderer();
  int getVideoIndex() const {return video_index_;};
  void addPacket(AVPacket& packet);
  void render();
  void quit();
private:
  int video_index_;
  AVCodec* video_codec_;
  AVCodecContext* video_codec_context_;
  SDL_Texture *texture_;
  struct SwsContext *sws_ctx_;
  Uint8 *yPlane, *uPlane, *vPlane;
  size_t yPlaneSz, uvPlaneSz;
  int uvPitch;
  AVFrame *pFrame;
  AVFrame* pFrameYUV;
  SDL_Renderer *renderer_;
  SDL_Window *screen_;
  queue<AVPacket*> packet_queue_;
  mutex queue_lock_;
};
#endif //VIDEO_PLAYER_H_
