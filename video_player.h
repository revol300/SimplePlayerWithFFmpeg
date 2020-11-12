#ifndef VIDEO_PLAYER_H_
#define VIDEO_PLAYER_H_
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <SDL.h>
#include <SDL_thread.h>
#include <cassert>
}

#include <iostream>
#include <memory>
using namespace std;

class VideoPlayer {
public:
  VideoPlayer();
  ~VideoPlayer();

  void setURL(string const& filename);
  int init();
  void play();

private:
  int openFile();
  int openWindow();
  string file_path_;
  int video_index_, audio_index_;
  AVFormatContext* fmt_ctx_;

  AVCodec* video_codec_;
  AVCodec* audio_codec_;
  AVCodecContext* video_codec_context_;
  AVCodecContext* audio_codec_context_;
  struct SwsContext *sws_ctx;
  SDL_Event event;
  SDL_Window *screen;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  SDL_AudioSpec audio_wanted_spec_;
  Uint8 *yPlane, *uPlane, *vPlane;
  size_t yPlaneSz, uvPlaneSz;
  int uvPitch;

  //NOTE : 임시로 입력
  int dev;

};
#endif //VIDEO_PLAYER_H_
