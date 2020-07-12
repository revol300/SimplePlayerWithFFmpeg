// tutorial03.c
// A pedagogical video player that will stream through every video frame as fast as it can
// and play audio (out of sync).
//
// Code based on FFplay, Copyright (c) 2003 Fabrice Bellard, and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1 With updates from https://github.com/chelyaev/ffmpeg-tutorial
// Updates tested on:
// LAVC 54.59.100, LAVF 54.29.104, LSWS 2.1.101, SDL 1.2.15
// on GCC 4.7.2 in Debian February 2015
//
// Use
//
// gcc -o tutorial03 tutorial03.c -lavformat -lavcodec -lswscale -lz -lm `sdl-config --cflags --libs`
// to build (assuming libavformat and libavcodec are correctly installed, 
// and assuming you have sdl-config. Please refer to SDL docs for your installation.)
//
// Run using
// tutorial03 myvideofile.mpg
//
// to play the stream on your screen.

#include "tutorial03.h"

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <assert.h>

using namespace std;
// compatibility with newer API
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

typedef struct PacketQueue {
  AVPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int size;
  SDL_mutex *mutex;
  SDL_cond *cond;
} PacketQueue;

static AVCodecContext  *aCodecCtx;
static PacketQueue audioq;
static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
static unsigned int audio_buf_size;
static unsigned int audio_buf_index;
static int quit;

void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
  /* cout << "put queue" << endl; */
  AVPacketList *pkt1;
  pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
  if (!pkt1)
    return -1;
  av_init_packet(&pkt1->pkt);
  av_packet_ref(&pkt1->pkt, pkt);
  /* memcpy((void*)&pkt1->pkt, (void*)pkt, sizeof(AVPacket)); */
  pkt1->next = NULL;
  SDL_LockMutex(q->mutex);
  if (!q->last_pkt) {
    q->first_pkt = pkt1;
  } else {
    q->last_pkt->next = pkt1;
  }
  q->last_pkt = pkt1;
  q->nb_packets++;
  q->size += pkt1->pkt.size;
  SDL_CondSignal(q->cond);
  SDL_UnlockMutex(q->mutex);
  return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{

  /* cout << "packet queue get : " << __LINE__ << endl; */
  AVPacketList *pkt1;
  int ret;
  
  SDL_LockMutex(q->mutex);
  for(;;) {
    if(quit) {
      ret = -1;
      break;
    }
    pkt1 = q->first_pkt;
    if (pkt1) {
      q->first_pkt = pkt1->next;
      if (!q->first_pkt)
        q->last_pkt = NULL;
      q->nb_packets--;
      q->size -= pkt1->pkt.size;
      *pkt = pkt1->pkt;
      av_free(pkt1);
      ret = 1;
      break;
    } else if (!block) {
      ret = 0;
      break;
    } else {
      SDL_CondWait(q->cond, q->mutex);
      break;
    }
  }
  SDL_UnlockMutex(q->mutex);
  return ret;
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size) {

  /* cout << "audio decode frame : " << __LINE__ << endl; */
  static AVPacket pkt;
  static uint8_t *audio_pkt_data = NULL;
  static int audio_pkt_size = 0;
  static AVFrame frame;

  int len1, data_size = 0;

  for(;;) {
    while(audio_pkt_size > 0) {
      int got_frame = 0;
      len1 = avcodec_send_packet(aCodecCtx, &pkt);
      bool frameFinished = 0;
      if (!(len1 < 0 && len1 != AVERROR(EAGAIN) && len1 != AVERROR_EOF)) {
        if (len1 >= 0)
          pkt.size = 0;
        len1 = avcodec_receive_frame(aCodecCtx, &frame);
        if (len1 >= 0) {
          cout << "got frame !!!" << endl;
          frameFinished = 1;
        }
      }

      if(frameFinished)
        continue;
      audio_pkt_data += len1;
      audio_pkt_size -= len1;
      data_size = 0;
      if(got_frame) {
        data_size = av_samples_get_buffer_size(NULL,
            aCodecCtx->channels,
            frame.nb_samples,
            aCodecCtx->sample_fmt,
            1);
        assert(data_size <= buf_size);
        memcpy(audio_buf, frame.data[0], data_size);
      }
      if(data_size <= 0) {
        /* No data yet, get more frames */
        continue;
      }
      /* We have data, return it and come back for more later */
      return data_size;
    }
    if(pkt.data)
      av_packet_unref(&pkt);

    if(quit) {
      return -1;
    }

    if(packet_queue_get(&audioq, &pkt, 1) < 0) {
      exit(-1);
      return -1;
    }

    audio_pkt_data = pkt.data;
    audio_pkt_size = pkt.size;
  }
}

static void audio_callback(void *userdata, Uint8 *stream, int len) {

  cout << "audio callback called" << endl;

  AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
  int len1, audio_size;

  while(len > 0) {
    if(audio_buf_index >= audio_buf_size) {
      /* We have already sent all our data; get more */
      audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
      if(audio_size < 0) {
        cout << "error : " << endl;
        /* If error, output silence */
        audio_buf_size = 1024; // arbitrary?
        memset(audio_buf, 0, audio_buf_size);
      } else {
        audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }
    len1 = audio_buf_size - audio_buf_index;
    if(len1 > len)
      len1 = len;
    memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
    len -= len1;
    stream += len1;
    audio_buf_index += len1;
  }
}

int main(int argc, char *argv[]) {
    static AVCodecContext  *aCodecCtx = NULL;
    static PacketQueue audioq;
    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;
    static int quit = 0;

    AVFormatContext *pFormatCtx = NULL;
    int videoStream, audioStream;
    unsigned i;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVFrame *pFrame = NULL;
    AVPacket packet;
    int frameFinished;
    struct SwsContext *sws_ctx = NULL;
    SDL_Event event;
    SDL_Window *screen;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_AudioSpec   wanted_spec, spec;
    Uint8 *yPlane, *uPlane, *vPlane; size_t yPlaneSz, uvPlaneSz;
    int uvPitch;

    if (argc < 2) {
        fprintf(stderr, "Usage: test <file>\n");
        exit(1);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }

    // Open video file
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
        return -1; // Couldn't open file

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        return -1; // Couldn't find stream information

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    // Find the first video stream
    videoStream = -1;
    audioStream = -1;
    for(i=0; i<pFormatCtx->nb_streams; i++) {
      if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO &&
          videoStream < 0) {
        videoStream=i;
      }
      if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO &&
          audioStream < 0) {
        audioStream=i;
      }
    }
    if(videoStream==-1)
      return -1; // Didn't find a video stream
    if(audioStream==-1)
      return -1;
   
    auto aCodec = avcodec_find_decoder(pFormatCtx->streams[audioStream]->codecpar->codec_id);
    if(!aCodec) {
      fprintf(stderr, "Unsupported codec!\n");
      return -1;
    }

    // Copy context
    aCodecCtx = avcodec_alloc_context3(aCodec);
    auto result = avcodec_parameters_to_context(
        aCodecCtx,
        pFormatCtx->streams[audioStream]->codecpar);
    if(result < 0){
      return -5;
    }

  // Set audio settings from codec info
  SDL_memset(&wanted_spec, 0, sizeof(wanted_spec)); /* or SDL_zero(want) */
  wanted_spec.freq = aCodecCtx->sample_rate;
  wanted_spec.format = AUDIO_S16SYS;
  av_get_channel_layout_nb_channels(aCodecCtx->channels);
  wanted_spec.channels = aCodecCtx->channels;
  wanted_spec.silence = 0;
  wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
  wanted_spec.callback = audio_callback;
  wanted_spec.userdata = aCodecCtx;
  cout << "aCodecCtx : " << aCodecCtx;

  auto dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
  if (dev == 0) {
    cout << "open audio device failed" << endl;
    exit(-1);
  } else {
    if (wanted_spec.format != spec.format) { /* we let this one thing change. */
    }
  }
  avcodec_open2(aCodecCtx, aCodec, NULL);
  packet_queue_init(&audioq);
  bool audio_start = false;

  pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;
  
  auto pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
  if(pCodec==NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }

  // Copy context
  pCodecCtx = avcodec_alloc_context3(pCodec);

  // Open codec
  if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
    return -1; // Could not open codec
  
  SDL_PauseAudioDevice(dev, 1);
  /* start audio playing. */
  /* SDL_Delay(100000); [> let the audio callback play some sound for 100 seconds. <]
   * SDL_CloseAudioDevice(dev); */
    // Get a pointer to the codec context for the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if (pCodec == NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }

    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
        fprintf(stderr, "Couldn't copy codec context");
        return -1; // Error copying codec context
    }

    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -1; // Could not open codec

    // Allocate video frame
    pFrame = av_frame_alloc();

    // Make a screen to put our video
    screen = SDL_CreateWindow(
            "FFmpeg Tutorial",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            pCodecCtx->width,
            pCodecCtx->height,
            SDL_WINDOW_RESIZABLE
        );

    if (!screen) {
        fprintf(stderr, "SDL: could not create window - exiting\n");
        exit(1);
    }

    renderer = SDL_CreateRenderer(screen, -1, 0);
    if (!renderer) {
        fprintf(stderr, "SDL: could not create renderer - exiting\n");
        exit(1);
    }

    // Allocate a place to put our YUV image on that screen
    texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            pCodecCtx->width,
            pCodecCtx->height
        );
    if (!texture) {
        fprintf(stderr, "SDL: could not create texture - exiting\n");
        exit(1);
    }

    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
            pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL);

    // set up YV12 pixel array (12 bits per pixel)
    yPlaneSz = pCodecCtx->width * pCodecCtx->height;
    uvPlaneSz = pCodecCtx->width * pCodecCtx->height / 4;
    yPlane = (Uint8*)malloc(yPlaneSz);
    uPlane = (Uint8*)malloc(uvPlaneSz);
    vPlane = (Uint8*)malloc(uvPlaneSz);
    if (!yPlane || !uPlane || !vPlane) {
        fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
        exit(1);
    }

    uvPitch = pCodecCtx->width / 2;
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        // Is this a packet from the video stream?
        if (packet.stream_index == videoStream) {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // Did we get a video frame?
            if (frameFinished) {
                AVPicture pict;
                pict.data[0] = yPlane;
                pict.data[1] = uPlane;
                pict.data[2] = vPlane;
                pict.linesize[0] = pCodecCtx->width;
                pict.linesize[1] = uvPitch;
                pict.linesize[2] = uvPitch;

                // Convert the image into YUV format that SDL uses
                sws_scale(sws_ctx, (uint8_t const * const *) pFrame->data,
                        pFrame->linesize, 0, pCodecCtx->height, pict.data,
                        pict.linesize);

                SDL_UpdateYUVTexture(
                        texture,
                        NULL,
                        yPlane,
                        pCodecCtx->width,
                        uPlane,
                        uvPitch,
                        vPlane,
                        uvPitch
                    );

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

            }
        } else if(packet.stream_index==audioStream) {
          /* if(packet.stream_index==audioStream) {  */
          if(!audio_start) {
            cout << "audio start !!" << endl;
            SDL_PauseAudio(0);
            SDL_PauseAudioDevice(dev, 0);
            audio_start = true;
          }
          packet_queue_put(&audioq, &packet);
        } else {
          av_free_packet(&packet);
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
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
    av_frame_free(&pFrame);
    free(yPlane);
    free(uPlane);
    free(vPlane);

    // Close the codec
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}
