#include "video_player.h"

static SDL_AudioDeviceID audio_dev;

//@NOTE move to class
static void audio_callback(void *userdata, Uint8 * stream, int len);

//@NOTE move to class

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

typedef struct PacketQueue {
  AVPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int size;
  SDL_mutex *mutex;
  SDL_cond *cond;
} PacketQueue;

typedef struct AudioParams {
  int freq;
  int channels;
  int64_t channel_layout;
  enum AVSampleFormat fmt;
  int frame_size;
  int bytes_per_sec;
} AudioParams;

PacketQueue audioq;
SDL_AudioSpec spec;
SwrContext* swr_ctx;
int quit = 0;
AudioParams audio_hw_params_tgt;
AudioParams audio_hw_params_src;

void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

int packet_queue_put_private(PacketQueue *q, AVPacket *pkt) {
    AVPacketList *pkt1;

    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    
    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    /* XXX: should duplicate packet data in DV case */
    SDL_CondSignal(q->cond);
    return 0;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;
SDL_LockMutex(q->mutex);
    ret = packet_queue_put_private(q, pkt);
    SDL_UnlockMutex(q->mutex);
    return ret;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);
    for (;;) {
        pkt1 = q->first_pkt;
        if (pkt1) {
            printf("get packet %d\n", __LINE__);
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            /* cout << "get queue " << __LINE__ << endl; */
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

double get_audio_clock(VideoState *is) {
  double pts;
  int hw_buf_size, bytes_per_sec, n;

  pts = is->audio_clock; /* maintained in the audio thread */
  hw_buf_size = is->audio_buf_size - is->audio_buf_index;
  bytes_per_sec = 0;
  n = is->audio_ctx->channels * 2;
  if(is->audio_st) {
    bytes_per_sec = is->audio_ctx->sample_rate * n;
  }
  if(bytes_per_sec) {
    pts -= (double)hw_buf_size / bytes_per_sec;
  }
  return pts;
}

double get_video_clock(VideoState *is) {
  double delta;

  delta = (av_gettime() - is->video_current_pts_time) / 1000000.0;
  return is->video_current_pts + delta;
}
double get_external_clock(VideoState *is) {
  return av_gettime() / 1000000.0;
}

double get_master_clock(VideoState *is) {
  if(is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
    return get_video_clock(is);
  } else if(is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
    return get_audio_clock(is);
  } else {
    return get_external_clock(is);
  }
}

/* Add or subtract samples to get a better sync, return new
   audio buffer size */
int synchronize_audio(VideoState *is, short *samples, int samples_size, double pts) {
  int n;
  double ref_clock;

  n = 2 * is->audio_ctx->channels;

  if(is->av_sync_type != AV_SYNC_AUDIO_MASTER) {
    double diff, avg_diff;
    int wanted_size, min_size, max_size /*, nb_samples */;

    ref_clock = get_master_clock(is);
    diff = get_audio_clock(is) - ref_clock;

    if(diff < AV_NOSYNC_THRESHOLD) {
      // accumulate the diffs
      is->audio_diff_cum = diff + is->audio_diff_avg_coef
        * is->audio_diff_cum;
      if(is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
        is->audio_diff_avg_count++;
      } else {
        avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);
        if(fabs(avg_diff) >= is->audio_diff_threshold) {
          wanted_size = samples_size + ((int)(diff * is->audio_ctx->sample_rate) * n);
          min_size = samples_size * ((100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100);
          max_size = samples_size * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100);
          if(wanted_size < min_size) {
            wanted_size = min_size;
          } else if (wanted_size > max_size) {
            wanted_size = max_size;
          }
          if(wanted_size < samples_size) {
            /* remove samples */
            samples_size = wanted_size;
          } else if(wanted_size > samples_size) {
            uint8_t *samples_end, *q;
            int nb;

            /* add samples by copying final sample*/
            nb = (samples_size - wanted_size);
            samples_end = (uint8_t *)samples + samples_size - n;
            q = samples_end + n;
            while(nb > 0) {
              memcpy(q, samples_end, n);
              q += n;
              nb -= n;
            }
            samples_size = wanted_size;
          }
        }
      }
    } else {
      /* difference is TOO big; reset diff stuff */
      is->audio_diff_avg_count = 0;
      is->audio_diff_cum = 0;
    }
  }
  return samples_size;
}

//Resampling
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

        /* swr_ctx = swr_alloc_set_opts(NULL, audio_hw_params_tgt.channel_layout,
         *         audio_hw_params_tgt.fmt, audio_hw_params_tgt.freq,
         *         dec_channel_layout, (AVSampleFormat)af->format, af->sample_rate, 0, NULL); */
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

VideoPlayer::VideoPlayer() : fmt_ctx_(NULL),
                             video_index_(-1),
                             audio_index_(-1),
                             video_codec_(nullptr),
                             audio_codec_(nullptr),
                             video_codec_context_(nullptr),
                             audio_codec_context_(nullptr),
                             sws_ctx(nullptr),
                             event(),
                             screen(nullptr),
                             renderer(nullptr),
                             texture(nullptr),
                             yPlane(nullptr),
                             uPlane(nullptr),
                             vPlane(nullptr),
                             yPlaneSz(0),
                             uvPlaneSz(0),
                             uvPitch(0) {
                               av_register_all(); //@NOTE: For FFmpeg version < 4.0
                             }

VideoPlayer::~VideoPlayer() {
  if(yPlane)
    free(yPlane);
  if(uPlane)
    free(uPlane);
  if(vPlane)
    free(vPlane);
  if(fmt_ctx_)
    avformat_close_input(&fmt_ctx_);
  if(video_codec_context_)
    avcodec_free_context(&video_codec_context_);
}

void VideoPlayer::setURL(string const& filename) {
  file_path_ = filename;
}

int VideoPlayer::openFile() {
  //this function only looks at the header
  int ret = avformat_open_input(&fmt_ctx_, file_path_.c_str(), NULL, NULL);
  if(ret < 0) {
    char error_msg[256];
    av_make_error_string(error_msg, 256, ret);
    cout << "Could not open input file " <<   file_path_ << " error_msg : "<< error_msg << endl;
    return -1;
  }
  ret = avformat_find_stream_info(fmt_ctx_, NULL);
  if(ret < 0) {
    cout << "Failed to retrieve input stream information : " << endl;
    return -2;
  }

  video_index_ = av_find_best_stream	(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if(video_index_ == AVERROR_STREAM_NOT_FOUND)
    video_index_ = -1;

  audio_index_ = av_find_best_stream	(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if(audio_index_ == AVERROR_STREAM_NOT_FOUND)
    audio_index_ = -1;

  if(video_index_ < 0 && audio_index_ < 0) {
    cout << "Failed to retrieve input stream information" << endl;
    return -3;
  }

  video_codec_ = avcodec_find_decoder(fmt_ctx_->streams[video_index_]->codecpar->codec_id);
  audio_codec_ = avcodec_find_decoder(fmt_ctx_->streams[audio_index_]->codecpar->codec_id);

  if(video_codec_ == NULL || audio_codec_ == NULL){
    cout << "Failed to find codec" << endl;
  }

  // Get the codec context
  video_codec_context_ = avcodec_alloc_context3(video_codec_);
  if (!video_codec_context_){
    cout << "Out of memory" << endl;
    return -4;
  }

  // Set the parameters of the codec context from the stream
  int result = avcodec_parameters_to_context(
      video_codec_context_,
      fmt_ctx_->streams[video_index_]->codecpar);
  if(result < 0){
    return -5;
  }

  if(avcodec_open2(video_codec_context_, video_codec_, NULL)<0)
    return -1; // Could not open codec

  // Get the codec context
  audio_codec_context_ = avcodec_alloc_context3(audio_codec_);
  if (!audio_codec_context_){
    cout << "Out of memory" << endl;
    return -4;
  }

  // Set the parameters of the codec context from the stream
  result = avcodec_parameters_to_context(
      audio_codec_context_,
      fmt_ctx_->streams[audio_index_]->codecpar);
  if(result < 0){
    return -5;
  }

  if(avcodec_open2(audio_codec_context_, audio_codec_, NULL)<0)
    return -1; // Could not open codec

  return 0;

}

int VideoPlayer::openWindow() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    cout << "Could not initialize SDL - " << SDL_GetError() <<endl;
    return -1;
  }
  // Make a screen to put our video
  screen = SDL_CreateWindow(
      "FFmpeg Tutorial",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      video_codec_context_->width,
      video_codec_context_->height,
      SDL_WINDOW_RESIZABLE
      );

  if (!screen) {
    fprintf(stderr, "SDL: could not create window - exiting\n");
    return -2;
  }

  renderer = SDL_CreateRenderer(screen, -1, 0);
  if (!renderer) {
    fprintf(stderr, "SDL: could not create renderer - exiting\n");
    return -3;
  }

  // Allocate a place to put our YUV image on that screen
  texture = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_YV12,
      SDL_TEXTUREACCESS_STREAMING,
      video_codec_context_->width,
      video_codec_context_->height
      );
  if (!texture) {
    fprintf(stderr, "SDL: could not create texture - exiting\n");
    return -4;
  }

  // initialize SWS context for software scaling
  sws_ctx = sws_getContext(video_codec_context_->width, video_codec_context_->height,
      video_codec_context_->pix_fmt, video_codec_context_->width, video_codec_context_->height,
      AV_PIX_FMT_YUV420P,
      SWS_BILINEAR,
      NULL,
      NULL,
      NULL);

  // set up YV12 pixel array (12 bits per pixel)
  yPlaneSz = video_codec_context_->width * video_codec_context_->height;
  uvPlaneSz = video_codec_context_->width * video_codec_context_->height / 4;
  yPlane = (Uint8*)malloc(yPlaneSz);
  uPlane = (Uint8*)malloc(uvPlaneSz);
  vPlane = (Uint8*)malloc(uvPlaneSz);
  if (!yPlane || !uPlane || !vPlane) {
    fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
    exit(1);
  }
  uvPitch = video_codec_context_->width / 2;

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
  audio_wanted_spec_.callback = audio_callback;
  audio_wanted_spec_.userdata = audio_codec_context_;

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
  packet_queue_init(&audioq);

  audio_hw_params_tgt.fmt = AV_SAMPLE_FMT_S16;
  audio_hw_params_tgt.freq = spec.freq;
  /* audio_hw_params_tgt.channel_layout = channel_layout; */
  audio_hw_params_tgt.channels = spec.channels;
  audio_hw_params_tgt.frame_size = av_samples_get_buffer_size(NULL,
      audio_hw_params_tgt.channels, 1, audio_hw_params_tgt.fmt, 1);
  audio_hw_params_tgt.bytes_per_sec = av_samples_get_buffer_size(NULL,
      audio_hw_params_tgt.channels, audio_hw_params_tgt.freq,
      audio_hw_params_tgt.fmt, 1);
  if (audio_hw_params_tgt.bytes_per_sec <= 0|| audio_hw_params_tgt.frame_size <= 0)
  {
    printf("size error\n");
    return -1;
  }
  audio_hw_params_src = audio_hw_params_tgt;

  return 0;
}

int VideoPlayer::init() {
  openFile();
  openWindow();
  return 0;
}

void VideoPlayer::play() {
  bool frameFinished = false;
  bool audio_start = false;
  AVPacket packet;
  AVFrame *pFrame = NULL;
  pFrame = av_frame_alloc();
  /* https://medium.com/@moony211/av-frame-free에-대한-고찰-8c0b416e2082 */
  // Allocate an AVFrame structure
  AVFrame* pFrameYUV = nullptr;
  pFrameYUV=av_frame_alloc();
  if(pFrameYUV==nullptr)
    return;
  int numBytes = av_image_alloc(pFrameYUV->data,
      pFrameYUV->linesize,
      video_codec_context_->width,
      video_codec_context_->height,
      AV_PIX_FMT_YUV420P,
      32
      );
  AVPicture pict;
  pFrameYUV->data[0] = yPlane;
  pFrameYUV->data[1] = uPlane;
  pFrameYUV->data[2] = vPlane;
  pFrameYUV->linesize[0] = video_codec_context_->width;
  pFrameYUV->linesize[1] = uvPitch;
  pFrameYUV->linesize[2] = uvPitch;

  while (av_read_frame(fmt_ctx_, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == video_index_) {
      // Decode video frame
      auto used = avcodec_send_packet(video_codec_context_, &packet);
      if (!(used < 0 && used != AVERROR(EAGAIN) && used != AVERROR_EOF)) {
        if (used >= 0)
          packet.size = 0;
        used = avcodec_receive_frame(video_codec_context_, pFrame);
        if (used >= 0)
          frameFinished = true;
      }
      // Did we get a video frame?
      if (frameFinished) {
        // Convert the image into YUV format that SDL uses
        sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
            pFrame->linesize, 0, video_codec_context_->height,
            pFrameYUV->data, pFrameYUV->linesize);
        SDL_UpdateYUVTexture(
            texture,
            NULL,
            yPlane,
            video_codec_context_->width,
            uPlane,
            uvPitch,
            vPlane,
            uvPitch
            );
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
      }
    } else if (packet.stream_index == audio_index_) {
      if(!audio_start) {
        SDL_PauseAudio(0);
        SDL_PauseAudioDevice(dev, 0);
        audio_start = true;
      }
      AVPacket copy = { 0 };
      if (av_packet_ref(&copy, &packet) < 0)
        exit(-1);
      packet_queue_put(&audioq, &copy);
    }
        // Free the packet that was allocated by av_read_frame
    av_packet_unref(&packet);
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(screen);
        SDL_Quit();
        exit(0);
        break;
      default:
        break;
    }
  }
  // Free the YUV frame
  av_freep(&pFrameYUV->data);
  av_frame_free(&pFrameYUV);
  av_frame_free(&pFrame);
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size) {

  /* cout << "audio decode frame : " << __LINE__ << endl; */
  AVPacket* pkt = av_packet_alloc();
  uint8_t *audio_pkt_data = NULL;
  int audio_pkt_size = 0;
  AVFrame* frame = av_frame_alloc();

  int len1, data_size = 0;

  for(;;) {
    while(audio_pkt_size > 0) {
      len1 = avcodec_send_packet(aCodecCtx, pkt);
      bool frameFinished = 0;
      if (!(len1 < 0 && len1 != AVERROR(EAGAIN) && len1 != AVERROR_EOF)) {
        if (len1 >= 0)
          pkt->size = 0;
        cout << "receive packet !! " << endl;
        len1 = avcodec_receive_frame(aCodecCtx, frame);
        if (len1 >= 0) {
          frameFinished = 1;
          cout << "got frame !!" << endl;
        }
      }

      data_size = 0;
      if(frameFinished) {
        uint8_t* temp_buf;
        data_size = resample(aCodecCtx, frame, &temp_buf, &buf_size);
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

    if(quit) {
      av_frame_free(&frame);
      return -1;
    }

    if(packet_queue_get(&audioq, pkt, 1) < 0) {
      av_frame_free(&frame);
      exit(-1);
      return -1;
    }

    audio_pkt_data = pkt->data;
    audio_pkt_size = pkt->size;
  }
  av_frame_free(&frame);
}


static void audio_callback(void *userdata, Uint8 * stream, int len) {
    AVCodecContext *aCodecCtx = (AVCodecContext*)userdata;
    int len1,audio_size;
    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE*3)/2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index =0;
    while (len > 0) {
      if (audio_buf_index >= audio_buf_size) {
        audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
        if (audio_size < 0) {
          cout << "audio size is minux" << endl;
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


int main(int argc, char* argv[]) {
  int ret;

  av_log_set_level(AV_LOG_DEBUG);

  if(argc < 2) {
    printf("usage : %s <input>\n", argv[0]);
    return 0;
  }

  shared_ptr<VideoPlayer> video_player = make_shared<VideoPlayer>();
  video_player->setURL(argv[1]);

  if(video_player->init() < 0) {
    cout << "Failed to init capture tool" << endl;
    return -1;
  }

  video_player->play();
  return 0;

}
