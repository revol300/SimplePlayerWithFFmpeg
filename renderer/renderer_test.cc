#include <iostream>
#include <memory>

extern "C" {
#include <SDL.h>
}

#include "converter/audio_converter.h"
#include "converter/video_converter.h"
#include "decoder/decoder.h"
#include "demuxer/demuxer.h"
#include "renderer/audio_renderer.h"
#include "renderer/video_renderer.h"

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
  demuxer->init();
  auto video_index = demuxer->getVideoIndex();
  auto audio_index = demuxer->getAudioIndex();
  auto fmt_ctx = demuxer->getFormatContext();
  std::shared_ptr<Decoder> video_decoder =
      make_shared<Decoder>(video_index, fmt_ctx);
  std::shared_ptr<Decoder> audio_decoder =
      make_shared<Decoder>(audio_index, fmt_ctx);
  video_decoder->init();
  audio_decoder->init();

  std::shared_ptr<AVCodecContext> audio_codec_context;
  audio_decoder->getCodecContext(audio_codec_context);
  std::shared_ptr<AVCodecContext> video_codec_context;
  video_decoder->getCodecContext(video_codec_context);

  std::shared_ptr<AudioConverter> audio_converter =
      make_shared<AudioConverter>(audio_codec_context);
  audio_converter->init();
  std::shared_ptr<VideoConverter> video_converter =
      make_shared<VideoConverter>(video_codec_context);
  video_converter->init();

  std::shared_ptr<VideoRenderer> video_renderer = make_shared<VideoRenderer>(
      video_codec_context, fmt_ctx->streams[video_index]->time_base);
  video_renderer->init();

  std::shared_ptr<AudioRenderer> audio_renderer = make_shared<AudioRenderer>(
      audio_codec_context, fmt_ctx->streams[audio_index]->time_base);
  audio_renderer->init();

  SDL_Event event;
  while (true) {
    SDL_PollEvent(&event);
    if (event.type == SDL_QUIT) {
      video_renderer->quit();
      std::cout << "SDL_QUIT" << std::endl;
      SDL_Quit();
      break;
    }
    std::shared_ptr<AVPacket> packet;
    demuxer->getPacket(packet);
    if (packet.get()) {
      if (packet->stream_index == video_decoder->getIndex()) {
        std::vector<std::shared_ptr<AVFrame>> frame_list;
        video_decoder->getFrame(packet, frame_list);
        while (frame_list.size() > 0) {
          cout << "Got Video Frame! " << endl;
          std::shared_ptr<AVFrame> converted_frame;
          std::shared_ptr<AVFrame> original_frame = frame_list.front();
          video_converter->getFrame(original_frame, converted_frame);
          video_renderer->getFrame(converted_frame);
          frame_list.erase(frame_list.begin());
        }
      } else if (packet->stream_index == audio_decoder->getIndex()) {
        std::vector<std::shared_ptr<AVFrame>> frame_list;
        audio_decoder->getFrame(packet, frame_list);
        while (frame_list.size() > 0) {
          cout << "Got Audio Frame! " << endl;
          std::shared_ptr<AudioFrame> temp_buf;
          std::shared_ptr<AVFrame> original_frame = frame_list.front();
          audio_converter->getFrame(original_frame.get(), temp_buf);
          audio_renderer->getFrame(temp_buf);
          frame_list.erase(frame_list.begin());
        }
      }
    }
  }

  return 0;
}
