#ifndef CONVERTER_AUDIO_STRUCT_H_
#define CONVERTER_AUDIO_STRUCT_H_

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
  uint8_t *data;
  unsigned int size;
  int64_t pts;
} AudioFrame;

#endif // CONVERTER_AUDIO_STRUCT_H_
