#ifndef AUDIO_PLAYER_H_
#define AUDIO_PLAYER_H_
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
#include <mutex>
using namespace std;

typedef struct AudioParams {
  int freq;
  int channels;
  int64_t channel_layout;
  enum AVSampleFormat fmt;
  int frame_size;
  int bytes_per_sec;
} AudioParams;

class AudioPlayer {
public:
  AudioPlayer(int video_index, AVCodec* video_codec, AVCodecContext* video_codec_context);
  int initRenderer();
  int getAudioIndex() const {return audio_index_;};
  void addPacket(AVPacket& packet);
  void render();
  void quit();
  int getFrame(uint8_t *audio_buf, int buf_size);
  int getPacket();
  int resample(AVFrame *af, uint8_t** audio_buf, int *audio_buf_size);
  void start();
private:
  int audio_index_;
  int dev;
  AVCodec* audio_codec_;
  SwrContext* swr_ctx;
  AVCodecContext* audio_codec_context_;
  SDL_AudioSpec audio_wanted_spec_;
  SDL_AudioSpec spec;
  AudioParams audio_hw_params_;

  AVPacket* pkt;

  queue<AVPacket*> packet_queue_;
  mutex queue_lock_;
};
#endif //AUDIO_PLAYER_H_
