#include <iostream>
#include <memory>

#include "demuxer/demuxer.h"

using std::cout;
using std::endl;
using std::make_shared;

int main(int argc, char *argv[]) {
  av_register_all(); //@NOTE: For FFmpeg version < 4.0
  if (argc < 2) {
    printf("usage : %s <input>\n", argv[0]);
    return 0;
  }
  std::string file_path = argv[1];
  std::shared_ptr<Demuxer> demuxer = make_shared<Demuxer>(file_path);
  if (
      demuxer->init() < 0) {
      cout << "failed to initialize demuxer" << endl;
      return 0;
  };
  std::shared_ptr<AVPacket> packet;
  demuxer->getPacket(packet);
  return 0;
}
