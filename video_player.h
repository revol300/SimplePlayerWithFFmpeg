#ifndef VIDEO_PLAYER_H_
#define VIDEO_PLAYER_H_
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
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
  int video_index_;
  AVFormatContext* fmt_ctx_;

  AVCodec* video_codec_;
  AVCodecContext* video_codec_context_;
  struct SwsContext *sws_ctx;
  SDL_Event event;
  SDL_Window *screen;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  Uint8 *yPlane, *uPlane, *vPlane;
  size_t yPlaneSz, uvPlaneSz;
  int uvPitch;

};
#endif //VIDEO_PLAYER_H_
