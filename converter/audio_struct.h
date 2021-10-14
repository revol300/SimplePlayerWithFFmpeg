#ifndef AUDIO_STRUCT
#define AUDIO_STRUCT

extern "C" {
#include <libavformat/avformat.h>
}

typedef struct AudioParams {
  int freq;
  int channels;
  int64_t channel_layout;
  enum AVSampleFormat fmt;
  int frame_size;
  int bytes_per_sec;
} AudioParams;

typedef struct AudioFrame {
  uint8_t* data;
  unsigned int size;
  int64_t pts;
} AudioFrame;

#endif // AUDIO_STRUCT
