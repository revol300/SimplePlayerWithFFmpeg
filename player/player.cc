#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>

#define MAX_SIZE 40
#define MIN_SIZE 20

extern "C" {
#include <SDL.h>
}
#include <atomic>
#include <future>

#include "converter/audio_converter.h"
#include "converter/video_converter.h"
#include "decoder/decoder.h"
#include "demuxer/demuxer.h"
#include "renderer/audio_renderer.h"
#include "renderer/video_renderer.h"
#include "timer/timer.h"

#include "player/safe_queue.h"

using std::cout;
using std::endl;

SafeQueue<AVPacket> video_decoder_queue;
SafeQueue<AVPacket> audio_decoder_queue;

SafeQueue<AVFrame> video_converter_queue;
SafeQueue<AVFrame> audio_converter_queue;

SafeQueue<AVFrame> video_renderer_queue;
SafeQueue<AudioFrame> audio_renderer_queue;

Uint32 SDL_DEMUX, SDL_VIDEO_DECODE, SDL_AUDIO_DECODE, SDL_VIDEO_CONVERT,
    SDL_AUDIO_CONVERT, SDL_VIDEO_RENDER, SDL_AUDIO_RENDER, SDL_DEMUX_END;

std::mutex demux_lock, v_decode_lock, a_decode_lock, v_convert_lock,
    a_convert_lock, v_render_lock, a_render_lock;

std::atomic<int> demuxer_job_count(0);

void flushQueue() {
  video_decoder_queue.flush();
  audio_decoder_queue.flush();
  video_converter_queue.flush();
  audio_converter_queue.flush();
  video_renderer_queue.flush();
  audio_renderer_queue.flush();
}

void demuxing(std::shared_ptr<Demuxer> demuxer) {
  std::lock_guard<std::mutex> lock_guard(demux_lock);
  std::shared_ptr<AVPacket> packet;
  auto result = demuxer->getPacket(packet);
  if (packet.get()) {
    SDL_Event decode_event;
    if (packet->stream_index == demuxer->getVideoIndex()) {
      video_decoder_queue.push(packet);
      decode_event.type = SDL_VIDEO_DECODE;
    } else if (packet->stream_index == demuxer->getAudioIndex()) {
      audio_decoder_queue.push(packet);
      decode_event.type = SDL_AUDIO_DECODE;
    }
    SDL_PushEvent(&decode_event);
  }

  if (!result) {
    SDL_Event end_event;
    end_event.type = SDL_DEMUX_END;
    SDL_PushEvent(&end_event);
  }
  demuxer_job_count--;
}

void video_decoding(std::shared_ptr<Decoder> decoder) {
  std::lock_guard<std::mutex> lock_guard(v_decode_lock);
  std::shared_ptr<AVPacket> packet = video_decoder_queue.front();
  if (packet.get()) {
    std::vector<std::shared_ptr<AVFrame>> frame_list;
    decoder->getFrame(packet, frame_list);
    while (frame_list.size() > 0) {
      std::shared_ptr<AVFrame> frame = frame_list.front();
      video_converter_queue.push(frame);
      frame_list.erase(frame_list.begin());
      SDL_Event convert_event;
      convert_event.type = SDL_VIDEO_CONVERT;
      SDL_PushEvent(&convert_event);
    }
  }
}

void audio_decoding(std::shared_ptr<Decoder> decoder) {
  std::lock_guard<std::mutex> lock_guard(a_decode_lock);
  std::shared_ptr<AVPacket> packet = audio_decoder_queue.front();
  if (packet.get()) {
    std::vector<std::shared_ptr<AVFrame>> frame_list;
    decoder->getFrame(packet, frame_list);
    while (frame_list.size() > 0) {
      std::shared_ptr<AVFrame> frame = frame_list.front();
      audio_converter_queue.push(frame);
      frame_list.erase(frame_list.begin());
      SDL_Event convert_event;
      convert_event.type = SDL_AUDIO_CONVERT;
      SDL_PushEvent(&convert_event);
    }
  }
}

void video_converting(std::shared_ptr<VideoConverter> video_converter) {
  std::lock_guard<std::mutex> lock_guard(v_convert_lock);
  std::shared_ptr<AVFrame> frame = video_converter_queue.front();
  if (frame.get()) {
    std::shared_ptr<AVFrame> converted_frame;
    video_converter->getFrame(frame, converted_frame);
    video_renderer_queue.push(converted_frame);
    SDL_Event render_event;
    render_event.type = SDL_VIDEO_RENDER;
    SDL_PushEvent(&render_event);
  }
}

void audio_converting(std::shared_ptr<AudioConverter> audio_converter) {
  std::lock_guard<std::mutex> lock_guard(a_convert_lock);
  std::shared_ptr<AVFrame> frame = audio_converter_queue.front();
  if (frame.get()) {
    std::shared_ptr<AudioFrame> converted_frame;
    audio_converter->getFrame(frame.get(), converted_frame);
    audio_renderer_queue.push(converted_frame);
    SDL_Event render_event;
    render_event.type = SDL_AUDIO_RENDER;
    SDL_PushEvent(&render_event);
  }
}

void video_rendering(std::shared_ptr<VideoRenderer> renderer) {
  std::lock_guard<std::mutex> lock_guard(v_render_lock);
  std::shared_ptr<AVFrame> frame = video_renderer_queue.front();
  if (frame.get()) {
    renderer->getFrame(frame);
  }
}

void audio_rendering(std::shared_ptr<AudioRenderer> renderer) {
  std::lock_guard<std::mutex> lock_guard(a_render_lock);
  std::shared_ptr<AudioFrame> frame = audio_renderer_queue.front();
  if (frame.get()) {
    renderer->getFrame(frame);
  }
}

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

  std::vector<std::future<void>> job_queue;
  SDL_DEMUX = SDL_RegisterEvents(1);
  SDL_VIDEO_DECODE = SDL_RegisterEvents(2);
  SDL_AUDIO_DECODE = SDL_RegisterEvents(3);
  SDL_VIDEO_CONVERT = SDL_RegisterEvents(4);
  SDL_AUDIO_CONVERT = SDL_RegisterEvents(5);
  SDL_VIDEO_RENDER = SDL_RegisterEvents(6);
  SDL_AUDIO_RENDER = SDL_RegisterEvents(7);
  SDL_DEMUX_END = SDL_RegisterEvents(8);

  bool demux_done = false;
  bool running = true;
  while (true) {
    SDL_PollEvent(&event);

    if (event.type == SDL_KEYDOWN) {
      switch (event.key.keysym.sym) {
      case SDLK_p:
      case SDLK_SPACE:
        running = !running;
        if (running) {
          video_renderer->start();
          audio_renderer->start();
        } else {
          video_renderer->stop();
          audio_renderer->stop();
        }
        break;
      case SDLK_LEFT:
        flushQueue();
        for (int i = 0; i < job_queue.size(); i++) {
          job_queue[i].get();
        }
        job_queue.clear();
        video_renderer->flush();
        audio_renderer->flush();
        video_decoder->flush();
        audio_decoder->flush();
        SDL_PumpEvents();
        demuxer_job_count = 0;
        Timer::getInstance()->setAudioTime(
            Timer::getInstance()->getAudioTime() - (10 * 1000));
        demuxer->seek(Timer::getInstance()->getAudioTime());
        break;
      case SDLK_RIGHT:
        flushQueue();
        for (int i = 0; i < job_queue.size(); i++) {
          job_queue[i].get();
        }
        job_queue.clear();
        video_renderer->flush();
        audio_renderer->flush();
        video_decoder->flush();
        audio_decoder->flush();
        SDL_PumpEvents();
        demuxer_job_count = 0;
        Timer::getInstance()->setAudioTime(
            Timer::getInstance()->getAudioTime() + (10 * 1000));
        demuxer->seek(Timer::getInstance()->getAudioTime());
        break;
      }
    }

    if (event.type == SDL_QUIT) {
      video_renderer->quit();
      std::cout << "SDL_QUIT" << std::endl;
      SDL_Quit();
      break;
    } else if (event.type == SDL_DEMUX) {
      job_queue.push_back(std::async(std::launch::async, demuxing, demuxer));
    } else if (event.type == SDL_VIDEO_DECODE) {
      job_queue.push_back(
          std::async(std::launch::async, video_decoding, video_decoder));
    } else if (event.type == SDL_AUDIO_DECODE) {
      job_queue.push_back(
          std::async(std::launch::async, audio_decoding, audio_decoder));
    } else if (event.type == SDL_VIDEO_CONVERT) {
      job_queue.push_back(
          std::async(std::launch::async, video_converting, video_converter));
    } else if (event.type == SDL_AUDIO_CONVERT) {
      job_queue.push_back(
          std::async(std::launch::async, audio_converting, audio_converter));
    } else if (event.type == SDL_VIDEO_RENDER) {
      job_queue.push_back(
          std::async(std::launch::async, video_rendering, video_renderer));
    } else if (event.type == SDL_AUDIO_RENDER) {
      job_queue.push_back(
          std::async(std::launch::async, audio_rendering, audio_renderer));
    } else if (event.type == SDL_DEMUX_END) {
      demux_done = true;
    }

    std::vector<int> job_done;

    for (int i = 0; i < job_queue.size(); i++) {
      auto status = job_queue[i].wait_for(std::chrono::milliseconds(10));
      if (status == std::future_status::ready) {
        job_done.push_back(i);
      }
    }

    for (int j = job_done.size() - 1; j >= 0; j--) {
      job_queue.erase(job_queue.begin() + job_done[j]);
    }

    if (!demux_done && running) {
      if (video_renderer_queue.size() < MIN_SIZE ||
          audio_renderer_queue.size() < MIN_SIZE) {
        if (demuxer_job_count < 20) {
          SDL_Event demux_event;
          demux_event.type = SDL_DEMUX;
          demuxer_job_count++;
          SDL_PushEvent(&demux_event);
        }
      }
    }
  }

  for (int i = 0; i < job_queue.size(); i++) {
    job_queue[i].get();
  }

  return 0;
}
