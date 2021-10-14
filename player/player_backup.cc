#include <iostream>
#include <memory>

#define MAX_SIZE 40 
#define MIN_SIZE 20 

extern "C" {
  #include <SDL.h>
}
#include <atomic>
#include <future>

#include "../renderer/video_renderer.h"
#include "../renderer/audio_renderer.h"
#include "../converter/audio_converter.h"
#include "../converter/video_converter.h"
#include "../decoder/decoder.h"
#include "../demuxer/demuxer.h"

#include "safe_queue.h"

using std::cout;
using std::endl;

SafeQueue<AVPacket> video_decoder_queue;
SafeQueue<AVPacket> audio_decoder_queue;

SafeQueue<AVFrame> video_converter_queue;
SafeQueue<AVFrame> audio_converter_queue;

SafeQueue<AVFrame> video_renderer_queue;
SafeQueue<AudioFrame> audio_renderer_queue;

std::atomic<bool> running;
std::atomic<bool> do_video_decode;
std::atomic<bool> do_audio_decode;
std::atomic<bool> do_video_convert;
std::atomic<bool> do_audio_convert;



void demuxing(std::shared_ptr<Demuxer> demuxer) {
  std::shared_ptr<AVPacket> packet;
  while(running) {
    if(video_decoder_queue.size() + audio_decoder_queue.size() < MAX_SIZE * 2){
      demuxer->getPacket(packet);
      if(packet.get() ) {
        if(packet->stream_index == demuxer->getVideoIndex()) {
          video_decoder_queue.push(packet);
        } else if(packet->stream_index == demuxer->getAudioIndex()) {
          audio_decoder_queue.push(packet);
        }
      } else {
        break;
      }
    }
  }
}

void video_decoding(std::shared_ptr<Decoder> decoder) {
  while(running) {
    if(video_converter_queue.size() < MAX_SIZE || do_video_decode) {
      std::shared_ptr<AVPacket> packet = video_decoder_queue.front();
      if(packet.get()) {
        std::shared_ptr<AVFrame> frame;
        decoder->getFrame(packet, frame);
        if(frame.get()) {
          video_converter_queue.push(frame);
        }
      }
    }
  }
}

void audio_decoding(std::shared_ptr<Decoder> decoder) {
  while(running) {
    if(audio_converter_queue.size() < MAX_SIZE || do_audio_decode) {
      std::shared_ptr<AVPacket> packet = audio_decoder_queue.front();
      if(packet.get()) {
        std::shared_ptr<AVFrame> frame;
        decoder->getFrame(packet, frame);
        if(frame.get()) {
          audio_converter_queue.push(frame);
        }
      }
    }
  }
}

void video_converting(std::shared_ptr<VideoConverter> video_converter) {
  while(running) {
    if(video_renderer_queue.size() < MAX_SIZE * 5 || do_video_convert) {
      std::shared_ptr<AVFrame> frame = video_converter_queue.front();
      if(frame.get()) {
        std::shared_ptr<AVFrame> converted_frame;
        video_converter->getFrame(frame, converted_frame);
        video_renderer_queue.push(converted_frame);
      }
    }
    if(video_converter_queue.size() < MIN_SIZE) {
      do_video_decode = true;
    } else {
      do_video_decode = false;
    }
  }
}

void audio_converting(std::shared_ptr<AudioConverter> audio_converter) {
  while(running) {
    if(audio_renderer_queue.size() < MAX_SIZE * 5 || do_audio_convert) {
      std::shared_ptr<AVFrame> frame = audio_converter_queue.front();
      if(frame.get()) {
        std::shared_ptr<AudioFrame> converted_frame;
        audio_converter->getFrame(frame.get(), converted_frame);
        audio_renderer_queue.push(converted_frame);
      }
    }
    if(audio_converter_queue.size() < MIN_SIZE) {
      do_audio_decode = true;
    } else {
      do_audio_decode = false;
    }
  }
}

void video_rendering(std::shared_ptr<VideoRenderer> renderer) {
  while(running) {
    std::shared_ptr<AVFrame> frame = video_renderer_queue.front();
    if(frame.get()) {
      renderer->getFrame(frame);
    }
    if(video_renderer_queue.size() < MIN_SIZE) {
      do_video_convert = true;
    } else {
      do_video_convert = false;
    }
  }
}

void audio_rendering(std::shared_ptr<AudioRenderer> renderer) {
  while(running) {
    std::shared_ptr<AudioFrame> frame = audio_renderer_queue.front();
    if(frame.get()) {
      renderer->getFrame(frame);
    }
    if(audio_renderer_queue.size() < MIN_SIZE) {
      do_audio_convert = true;
    } else {
      do_audio_convert = false;
    }
  }
}

int main(int argc, char* argv[]) {
  do_video_decode = false;
  do_audio_decode = false;
  do_video_convert = false;
  do_audio_convert = false;
  
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

  std::shared_ptr<AVCodecContext> audio_codec_context;
  audio_decoder->getCodecContext(audio_codec_context);
  std::shared_ptr<AVCodecContext> video_codec_context;
  video_decoder->getCodecContext(video_codec_context);

  std::shared_ptr<AudioConverter> audio_converter = make_shared<AudioConverter>(audio_codec_context);
  audio_converter->init();
  std::shared_ptr<VideoConverter> video_converter = make_shared<VideoConverter>(video_codec_context);
  video_converter->init();

  std::shared_ptr<VideoRenderer> video_renderer = make_shared<VideoRenderer>(video_codec_context, fmt_ctx->streams[video_index]->time_base);
  video_renderer->init();

  std::shared_ptr<AudioRenderer> audio_renderer = make_shared<AudioRenderer>(audio_codec_context, fmt_ctx->streams[audio_index]->time_base);
  audio_renderer->init();

  SDL_Event event;
  running = true;

  auto demuxer_handle = std::async(std::launch::async, demuxing, demuxer);
  auto video_decoding_handle = std::async(std::launch::async, video_decoding, video_decoder);
  auto audio_decoding_handle = std::async(std::launch::async, audio_decoding, audio_decoder);
  auto video_converting_handle = std::async(std::launch::async, video_converting, video_converter);
  auto audio_converting_handle = std::async(std::launch::async, audio_converting, audio_converter);
  auto video_rendering_handle = std::async(std::launch::async, video_rendering, video_renderer);
  auto audio_rendering_handle = std::async(std::launch::async, audio_rendering, audio_renderer);

  while(true) {
    SDL_PollEvent(&event);
    if (event.type == SDL_QUIT) {
      video_renderer->quit();
      std::cout << "SDL_QUIT" << std::endl;
      SDL_Quit();
      break;
    }
  }

  running = false;
  demuxer_handle.get();
  video_decoding_handle.get();
  audio_decoding_handle.get();
  video_converting_handle.get();
  audio_converting_handle.get(); 
  video_rendering_handle.get();
  audio_rendering_handle.get();
  return 0;
}
