#ifndef MULTIMEDIA_PLAYER_H
#define MULTIMEDIA_PLAYER_H

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
#include <mutex>

#include "video_player.h"
#include "audio_player.h"
using namespace std;

class MultimediaPlayer {
public:
  MultimediaPlayer();

  int openFile(string const& filename);
  int openWindow();
  int play();
  int getVideoFrame();
private:
  int initAudioPlayer();
  int initVideoPlayer();

  string file_path_;

  AVFormatContext* fmt_ctx_;

  SDL_Event event;
  VideoPlayer* video_player_;
  AudioPlayer* audio_player_;
};

#endif //MULTIMEDIA_PLAYER_H
