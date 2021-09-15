#include <iostream>

#include "decoder.h"
#include "../demuxer/demuxer.h"

using std::cout;
using std::endl;

int main(int argc, char* argv[]) {
  av_register_all(); //@NOTE: For FFmpeg version < 4.0
  if(argc < 2) {
    printf("usage : %s <input>\n", argv[0]);
    return 0;
  }
  std::string file_path = argv[1];
  std::shared_ptr<Demuxer> demuxer = make_shared<Demuxer>(file_path);
  demuxer->init();
  auto video_index = demuxer->getVideoIndex();
  auto audio_index = demuxer->getAudioIndex();
  auto fmt_ctx = demuxer->getFormatContext();
  std::shared_ptr<Decoder> video_decoder = make_shared<Decoder>(video_index, fmt_ctx);
  std::shared_ptr<Decoder> audio_decoder = make_shared<Decoder>(audio_index, fmt_ctx);
  video_decoder->init();
  audio_decoder->init();
  while(true) {
    std::shared_ptr<AVPacket> packet;
    demuxer->getPacket(packet);
    if(packet.get() ) {
      if(packet->stream_index == video_decoder->getIndex()) {
        std::shared_ptr<AVFrame> frame;
        video_decoder->getFrame(packet, frame);
        if(frame.get()) {
          cout << "Got Video Frame! " << endl;
        }
      } else if(packet->stream_index == audio_decoder->getIndex()) {
        std::shared_ptr<AVFrame> frame;
        audio_decoder->getFrame(packet, frame);
        if(frame.get()) {
          cout << "Got Audio Frame! " << endl;
        }
      }
    } else {
      break;
    }
  }
  return 0;
}