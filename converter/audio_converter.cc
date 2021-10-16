#include "audio_converter.h"
#include <iostream>

#define SDL_AUDIO_BUFFER_SIZE 1024

using std::cout;
using std::cerr;
using std::endl;

AudioConverter::AudioConverter(std::shared_ptr<AVCodecContext> &audio_codec_context) : audio_codec_context_(audio_codec_context) {}

AudioConverter::~AudioConverter() {}

bool AudioConverter::init() {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    cout << "Could not initialize SDL - " << SDL_GetError() <<endl;
    return -1;
  }
  int dev;
  SDL_AudioSpec audio_wanted_spec_;
  cout << "initRender!" << endl;
  // Set audio settings from codec info
  SDL_memset(&audio_wanted_spec_, 0, sizeof(audio_wanted_spec_)); /* or SDL_zero(want) */
  audio_wanted_spec_.freq = audio_codec_context_->sample_rate;
  audio_wanted_spec_.format = AUDIO_S16SYS;
  audio_wanted_spec_.channels = audio_codec_context_->channels;
  audio_wanted_spec_.silence = 0;
  audio_wanted_spec_.samples = SDL_AUDIO_BUFFER_SIZE;

  cout << "audio_codec_context_ channel : " << audio_codec_context_->channels << endl;
  // SDL에서 해당 callback은 별도의 thread로 동작한다
  audio_wanted_spec_.callback = NULL;
  audio_wanted_spec_.userdata = this;

  SDL_AudioSpec hw_spec;
  dev = SDL_OpenAudioDevice(NULL, 0, &audio_wanted_spec_, &hw_spec, 0);
  if (dev == 0) {
    cerr << "open audio device failed!!" << endl;
    cerr << "Error : " << SDL_GetError() << endl;
    exit(-1);
  } else {
    if (audio_wanted_spec_.format != hw_spec.format) {
      /* we let this one thing change. */
      cout << "format is different" << endl;
    }
  }
  SDL_PauseAudioDevice(dev, 1);
  cout << "wanted spec channel : " <<  audio_wanted_spec_.channels << endl;
  cout << "actual spec channel : " <<  hw_spec.channels << endl;

  audio_hw_params_.fmt = AV_SAMPLE_FMT_S16;
  audio_hw_params_.freq = hw_spec.freq;
  audio_hw_params_.channels = hw_spec.channels;
  audio_hw_params_.frame_size =
    av_samples_get_buffer_size(
      NULL,
      audio_hw_params_.channels,
      1,
      audio_hw_params_.fmt,
      1);
  audio_hw_params_.bytes_per_sec =
    av_samples_get_buffer_size(
      NULL,
      audio_hw_params_.channels,
      audio_hw_params_.freq,
      audio_hw_params_.fmt,
      1);
  if (audio_hw_params_.bytes_per_sec <= 0|| audio_hw_params_.frame_size <= 0) {
    printf("size error\n");
    return false;
  }
  SDL_CloseAudioDevice(dev);
  SDL_Quit();
  return true;
}

int AudioConverter::getFrame(AVFrame *af, std::shared_ptr<AudioFrame>& audio_buf) {
    int resampled_data_size = 0;
    int64_t dec_channel_layout;
    int data_size = av_samples_get_buffer_size(
        NULL,
        audio_codec_context_->channels,
        af->nb_samples,
        audio_codec_context_->sample_fmt,
        1);
    
    if (!swr_ctx_.get()) {
      SwrContext* swr_ctx = nullptr;
      dec_channel_layout =
        (af->channel_layout &&
         av_frame_get_channels(af) == av_get_channel_layout_nb_channels(af->channel_layout)) ?
           af->channel_layout :
           av_get_default_channel_layout(audio_hw_params_.channels);
      if (af->format != audio_hw_params_.fmt
          || af->sample_rate != audio_hw_params_.freq
          || dec_channel_layout != audio_hw_params_.channel_layout
          || !swr_ctx) {
        if(!swr_ctx)
          swr_free(&swr_ctx);
        swr_ctx = swr_alloc_set_opts(NULL,
            audio_codec_context_->channel_layout,
            (AVSampleFormat)AV_SAMPLE_FMT_S16, audio_hw_params_.freq,
            av_get_default_channel_layout(audio_hw_params_.channels),
            (AVSampleFormat)af->format,
            af->sample_rate,
            0, NULL);

        if (!swr_ctx || swr_init(swr_ctx) < 0) {
          swr_free(&swr_ctx);
          return -1;
        }

        swr_ctx_ = std::shared_ptr<SwrContext>(swr_ctx, [](SwrContext* swr_ctx) {swr_free(&swr_ctx);});

        printf("swr_init\n");
        audio_hw_params_.channels = av_frame_get_channels(af);
        /* audio_hw_params_.fmt = (AVSampleFormat)af->format; */
        audio_hw_params_.fmt = (AVSampleFormat)AV_SAMPLE_FMT_S16;
        audio_hw_params_.freq = af->sample_rate;
      }
    }

    if (swr_ctx_.get()) {
        const uint8_t **in = (const uint8_t **) af->extended_data;
        int out_count = (int64_t) af->nb_samples * audio_hw_params_.freq
                / af->sample_rate + 256;
        int out_size = av_samples_get_buffer_size(NULL,
                audio_hw_params_.channels, out_count,
                audio_hw_params_.fmt, 0);
        int len2;
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        // @NOTE : https://ffmpeg.org/doxygen/3.2/group__lswr.html#details 참고해서 수정
        uint8_t* memory = (uint8_t *)av_malloc(out_size);
        uint8_t **out = &memory;
        len2 = swr_convert(swr_ctx_.get(), out, out_count, in, af->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING,
                    "audio buffer is probably too small\n");
            if (swr_init(swr_ctx_.get()) < 0)
              swr_ctx_.reset();
        }
        /* audio_buf = std::shared_ptr<uint8_t *>(&memory, [](uint8_t ** p) {
         *     std::cout << "converted_buffer freed (Audio)" << std::endl;
         *     av_free((void*)*p);
         * }); */
        resampled_data_size = len2 * audio_hw_params_.channels
                * av_get_bytes_per_sample(audio_hw_params_.fmt);
        AudioFrame* audio_frame = new AudioFrame();
        audio_frame->data = memory;
        audio_frame->size = resampled_data_size;
        audio_frame->pts = af->pts;
        audio_buf = std::shared_ptr<AudioFrame>(audio_frame, [](AudioFrame* f) {
            // std::cout << "converted_buffer freed (Audio)" << std::endl;
            av_free((void*) f->data);
            delete f;
        });
    } else {
      //@TODO : FIXME
      /* *audio_buf = af->data[0];
       * resampled_data_size = data_size; */
    }
    return resampled_data_size;
}
