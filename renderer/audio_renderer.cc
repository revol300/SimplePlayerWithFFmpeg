#include "renderer/audio_renderer.h"
#include "timer/timer.h"

#include <iostream>

using std::cerr;
using std::cout;
using std::endl;

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

int64_t audio_time = 0;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
  Timer::getInstance()->setAudioTime(audio_time);
  /* cout << "need size : " << len << endl; */
  AudioRenderer *audio_renderer = reinterpret_cast<AudioRenderer *>(userdata);
  int len1, audio_size;
  std::shared_ptr<AudioFrame> frame;
  static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
  static unsigned int audio_buf_size = 0;
  static unsigned int audio_buf_index = 0;

  int audio_bytes_len = len;
  while (len > 0) {
    if (audio_buf_index >= audio_buf_size) {
      audio_size = audio_renderer->getAudioData(audio_buf, sizeof(audio_buf));
      if (audio_size < 0) {
        // cout << "audio size is minus" << endl;
        /* if error, just output silence */
        audio_buf_size = 1024;
      } else {
        //@NOTE: sync를 맞추기 위해 사용 우선 기준이 되는 clock을 audio로 사용
        /* audio_size = synchronize_audio(is, (int16_t *)is->audio_buf,
         *     audio_size, pts); */
        audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }
    len1 = audio_buf_size - audio_buf_index;
    if (len1 > len)
      len1 = len;
    memset(stream, 0, len1);
    SDL_MixAudioFormat(stream,
                       reinterpret_cast<uint8_t *>(audio_buf) + audio_buf_index,
                       AUDIO_S16SYS, len1, 64);

    len -= len1;
    stream += len1;
    audio_buf_index += len1;
  }

  audio_time = audio_renderer->getAudioTime() -
               audio_renderer->bytesToMilisecond(audio_bytes_len) -
               2 * audio_renderer->bytesToMilisecond(audio_buf_size);
  // cerr << "audio_time_diff : " << Timer::getInstance()->getRelativeTime()<<
  // endl;
  // /* cerr << "audio_buf size time : " <<
  // audio_player->bytesToMilisecond(SDL_AUDIO_BUFFER_SIZE) << endl; */
  // /* Timer::getInstance()->setRelativeTime(audio_time+3*32); */
  // /* cerr << "audio_callback time : " <<
  // Timer::getInstance()->getRelativeTime() << endl; */ cerr << "audio_time : "
  // << Timer::getInstance()->getAudioTime() << endl; cerr << "audio_time pts: "
  // << audio_player->getAudioTime() << endl;

  // /* Let's assume the audio driver that is used by SDL has two periods. */
}

AudioRenderer::AudioRenderer(
    std::shared_ptr<AVCodecContext> &audio_codec_context, AVRational time_base)
    : audio_codec_context_(audio_codec_context), time_base_(time_base) {}

AudioRenderer::~AudioRenderer() {}

bool AudioRenderer::init() {
  if (SDL_Init(SDL_INIT_AUDIO)) {
    cout << "Could not initialize SDL - " << SDL_GetError() << endl;
    return -1;
  }
  SDL_AudioSpec audio_wanted_spec_;
  cout << "initRender!" << endl;
  // Set audio settings from codec info
  SDL_memset(&audio_wanted_spec_, 0,
             sizeof(audio_wanted_spec_)); /* or SDL_zero(want) */
  audio_wanted_spec_.freq = audio_codec_context_->sample_rate;
  audio_wanted_spec_.format = AUDIO_S16SYS;
  audio_wanted_spec_.channels = audio_codec_context_->channels;
  audio_wanted_spec_.silence = 0;
  audio_wanted_spec_.samples = SDL_AUDIO_BUFFER_SIZE;

  cout << "audio_codec_context_ channel : " << audio_codec_context_->channels
       << endl;
  // SDL에서 해당 callback은 별도의 thread로 동작한다
  audio_wanted_spec_.callback = audio_callback;
  audio_wanted_spec_.userdata = this;

  SDL_AudioSpec hw_spec;
  dev_ = SDL_OpenAudioDevice(NULL, 0, &audio_wanted_spec_, &hw_spec, 0);
  if (dev_ == 0) {
    cerr << "open audio device failed!!" << endl;
    cerr << "Error : " << SDL_GetError() << endl;
    exit(-1);
  } else {
    if (audio_wanted_spec_.format != hw_spec.format) {
      /* we let this one thing change. */
      cout << "format is different" << endl;
    }
  }
  SDL_PauseAudioDevice(dev_, 1);
  cout << "wanted spec channel : " << audio_wanted_spec_.channels << endl;
  cout << "actual spec channel : " << hw_spec.channels << endl;

  audio_hw_params_.fmt = AV_SAMPLE_FMT_S16;
  audio_hw_params_.freq = hw_spec.freq;
  audio_hw_params_.channels = hw_spec.channels;
  audio_hw_params_.frame_size = av_samples_get_buffer_size(
      NULL, audio_hw_params_.channels, 1, audio_hw_params_.fmt, 1);
  audio_hw_params_.bytes_per_sec = av_samples_get_buffer_size(
      NULL, audio_hw_params_.channels, audio_hw_params_.freq,
      audio_hw_params_.fmt, 1);
  if (audio_hw_params_.bytes_per_sec <= 0 || audio_hw_params_.frame_size <= 0) {
    printf("size error\n");
    return false;
  }
  // @NOTE : start playing audio
  SDL_PauseAudioDevice(dev_, 0);
  return true;
}

int AudioRenderer::getAudioData(uint8_t *audio_buf, int buf_size) {
  if (frame_queue_.size() > 0) {
    std::cout << "get Audio Data !!!!" << std::endl;
    lk_.lock();
    auto frame = frame_queue_.front();
    int min = buf_size > frame->size ? frame->size : buf_size;
    uint8_t *data = frame->data;
    memcpy(audio_buf, data, min);
    frame_queue_.pop();
    audio_clock_ = av_q2d(time_base_) * frame->pts * 1000;
    lk_.unlock();
    std::cout << "get Audio Data unlock!!!!" << std::endl;
    return min;
  }
  return -1;
}

int AudioRenderer::getFrame(std::shared_ptr<AudioFrame> &in) {
  lk_.lock();
  frame_queue_.push(in);
  lk_.unlock();
  return 0;
}
