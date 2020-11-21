#include "audio_player.h"

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

int resample(AVCodecContext *aCodecCtx, AVFrame *af, uint8_t** audio_buf, int *audio_buf_size) {
    int data_size = 0;
    int resampled_data_size = 0;
    int64_t dec_channel_layout;
    data_size = av_samples_get_buffer_size(NULL,
            aCodecCtx->channels,
            af->nb_samples,
            aCodecCtx->sample_fmt, 1);
    dec_channel_layout =(af->channel_layout&&
            av_frame_get_channels(af)== av_get_channel_layout_nb_channels(
                                    af->channel_layout)) ?
                    af->channel_layout :
                    av_get_default_channel_layout(av_frame_get_channels(af));
    if (af->format != audio_hw_params_src.fmt
            || af->sample_rate != audio_hw_params_src.freq
            /* || dec_channel_layout != audio_hw_params_src.channel_layout */
            || !swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = swr_alloc_set_opts(NULL,
            aCodecCtx->channel_layout, (AVSampleFormat)AV_SAMPLE_FMT_S16, spec.freq,
            dec_channel_layout,        (AVSampleFormat)af->format,     af->sample_rate,
            0, NULL);

        if (!swr_ctx || swr_init(swr_ctx) < 0)
        {
            swr_free(&swr_ctx);
            return -1;
        }
        printf("swr_init\n");
        audio_hw_params_src.channels = av_frame_get_channels(af);
        audio_hw_params_src.fmt = (AVSampleFormat)af->format;
        audio_hw_params_src.freq = af->sample_rate;
    }

    if (swr_ctx) {
        const uint8_t **in = (const uint8_t **) af->extended_data;
        uint8_t **out = audio_buf;
        int out_count = (int64_t) af->nb_samples * audio_hw_params_tgt.freq
                / af->sample_rate + 256;
        int out_size = av_samples_get_buffer_size(NULL,
                audio_hw_params_tgt.channels, out_count,
                audio_hw_params_tgt.fmt, 0);
        int len2;
        if (out_size < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        *audio_buf = (uint8_t *)av_malloc(out_size);
        /* av_fast_malloc(&audio_buf, audio_buf_size, out_size); */
        if (!*audio_buf)
            return AVERROR(ENOMEM);
        len2 = swr_convert(swr_ctx, out, out_count, in, af->nb_samples);
        if (len2 < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count)
        {
            av_log(NULL, AV_LOG_WARNING,
                    "audio buffer is probably too small\n");
            if (swr_init(swr_ctx) < 0)
                swr_free(&swr_ctx);
        }
        resampled_data_size = len2 * audio_hw_params_tgt.channels
                * av_get_bytes_per_sample(audio_hw_params_tgt.fmt);
    } else {
        *audio_buf = af->data[0];
        resampled_data_size = data_size;
    }
    return resampled_data_size;
}



static void audio_callback(void *userdata, Uint8 * stream, int len) {
   cout << "audio_callback called !" << endl;
   AudioPlayer* audio_player = (AudioPlayer*) userdata;
   int len1,audio_size;
   static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE*3)/2];
   static unsigned int audio_buf_size = 0;
   static unsigned int audio_buf_index =0;
   while (len > 0) {
     if (audio_buf_index >= audio_buf_size) {
       audio_size = audio_player->getFrame(audio_buf, sizeof(audio_buf));
       if (audio_size < 0) {
         cout << "audio size is minus" << endl;
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
     SDL_MixAudioFormat(stream, (uint8_t *)audio_buf + audio_buf_index, AUDIO_S16SYS, len1, 64);
     len -= len1;
     stream += len1;
     audio_buf_index += len1;
   }
}

AudioPlayer::AudioPlayer(int audio_index, AVCodec* audio_codec, AVCodecContext* audio_codec_context) : audio_index_(audio_index),
             audio_codec_(audio_codec),
             audio_codec_context_(audio_codec_context) {
  pkt = av_packet_alloc();
}

void AudioPlayer::addPacket(AVPacket& packet) {
   std::lock_guard<std::mutex> lock_guard(queue_lock_);
   AVPacket *copy = (AVPacket*)malloc(sizeof(struct AVPacket));
   av_init_packet(copy);
   if (av_packet_ref(copy, &packet) < 0)
     exit(-1);
   packet_queue_.push(copy);
}

int AudioPlayer::initRenderer() {
  cout << "initRender!" << endl;
  // Set audio settings from codec info
  SDL_memset(&audio_wanted_spec_, 0, sizeof(audio_wanted_spec_)); /* or SDL_zero(want) */
  audio_wanted_spec_.freq = audio_codec_context_->sample_rate;
  audio_wanted_spec_.format = AUDIO_S16SYS;
  /* audio_wanted_spec.format = AUDIO_F32SYS; */
  /* av_get_channel_layout_nb_channels(audio_codec_context_->channels); */
  audio_wanted_spec_.channels = audio_codec_context_->channels;
  cout << "audio_codec_context_ channel : " << audio_codec_context_->channels << endl;
  audio_wanted_spec_.silence = 0;
  audio_wanted_spec_.samples = SDL_AUDIO_BUFFER_SIZE;
  // SDL에서 해당 callback은 별도의 thread로 동작한다
  audio_wanted_spec_.callback = audio_callback;
  audio_wanted_spec_.userdata = this;

  dev = SDL_OpenAudioDevice(NULL, 0, &audio_wanted_spec_, &spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
  if (dev == 0) {
    exit(-1);
  } else {
    if (audio_wanted_spec_.format != spec.format) {
      /* we let this one thing change. */
      cout << "format is different" << endl;
    }
  }
  SDL_PauseAudioDevice(dev, 1);
  cout << "wanted spec channel : " <<  audio_wanted_spec_.channels << endl;
  cout << "actual spec channel : " <<  spec.channels << endl;

  audio_hw_params_.fmt = AV_SAMPLE_FMT_S16;
  audio_hw_params_.freq = spec.freq;
  audio_hw_params_.channels = spec.channels;
  audio_hw_params_.frame_size = av_samples_get_buffer_size(NULL,
      audio_hw_params_.channels, 1, audio_hw_params_.fmt, 1);
  audio_hw_params_.bytes_per_sec = av_samples_get_buffer_size(NULL,
      audio_hw_params_.channels, audio_hw_params_.freq,
      audio_hw_params_.fmt, 1);
  if (audio_hw_params_.bytes_per_sec <= 0|| audio_hw_params_.frame_size <= 0)
  {
    printf("size error\n");
    return -1;
  }
  return 0;
}

void AudioPlayer::start() {
  SDL_PauseAudioDevice(dev, 0);
}

int AudioPlayer::getFrame(uint8_t *audio_buf, int buf_size) {

  cout << "audio get frame : " << __LINE__ << endl;
  uint8_t *audio_pkt_data = NULL;
  int audio_pkt_size = 0;
  AVFrame* frame = av_frame_alloc();

  int len1, data_size = 0;
  for(;;) {
    while(audio_pkt_size > 0) {
      len1 = avcodec_send_packet(audio_codec_context_, pkt);
      bool frameFinished = 0;
      if (!(len1 < 0 && len1 != AVERROR(EAGAIN) && len1 != AVERROR_EOF)) {
        if (len1 >= 0)
          pkt->size = 0;
        cout << "receive packet !! " << endl;
        len1 = avcodec_receive_frame(audio_codec_context_, frame);
        if (len1 >= 0) {
          frameFinished = 1;
          cout << "got frame !!" << endl;
        }
      }

      data_size = 0;
      if(frameFinished) {
        uint8_t* temp_buf;
        data_size = resample(audio_codec_context_, frame, &temp_buf, &buf_size);
        cout << "data size : " << data_size << endl;
        assert(data_size <= buf_size);
        memcpy(audio_buf, temp_buf, data_size);
      }

      audio_pkt_data += data_size;
      audio_pkt_size -= data_size;
      if(data_size <= 0) {
        // No data yet, get more frames
        continue;
      }
      // We have data, return it and come back for more later
      // av_frame_free(&frame);
      return data_size;
    }
    if(pkt->data)
      av_packet_unref(pkt);

    /* if(quit) {
     *   av_frame_free(&frame);
     *   return -1;
     * } */

    if(getPacket() < 0) {
      av_frame_free(&frame);
      exit(-1);
      return -1;
    }
    audio_pkt_data = pkt->data;
    audio_pkt_size = pkt->size;
  }
  av_frame_free(&frame);
  return 0;
}

int AudioPlayer::getPacket() {
  std::lock_guard<std::mutex> lock_guard(queue_lock_);
  if(packet_queue_.size() <=0)
    return -1;
  pkt = packet_queue_.front();
  return 0;
}

