#include <iostream>
#include <SDL2/SDL.h>
#include "SDL2/SDL_image.h"

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale_internal.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

static Uint8 *pAudio_chunk;
static Uint32 audio_len;
static Uint8 *pAudio_pos;
static int audio_frame_count = 0;
char *pcm_buffer;
int pcm_buffer_size;

#define AUDIO_INBUF_SIZE 20480

static int decode(AVCodecContext *dec_ctx, AVPacket *pkt, AVFrame *frame,FILE *outfile)
{
    int i, ch;
    int ret, data_size;

    /* send the packet with the compressed data to the decoder */
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting the packet to the decoder\n");
        return -1;
    }

    /* read all the output frames (in general there may be any number of them */
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return -1;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return -1;
        }
        data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
        if (data_size < 0) {
            /* This should not occur, checking just for paranoia */
            fprintf(stderr, "Failed to calculate data size\n");
            return -1;
        }
//        printf("audio_frame n:%d nb_samples:%d pts:%s\n",
//           audio_frame_count++, frame->nb_samples,
//           av_ts2timestr(frame->pts, &dec_ctx->time_base));
        printf(">>>===111 size = %d\n",av_get_bytes_per_sample(dec_ctx->sample_fmt) * dec_ctx->channels * frame->nb_samples);
        for (i = 0; i < frame->nb_samples; i++)
            for (ch = 0; ch < dec_ctx->channels; ch++) {
                fwrite(frame->data[ch] + data_size*i, 1, data_size, outfile);
                memcpy(pcm_buffer + data_size*(2*i + ch),frame->data[ch] + data_size*i, data_size); 
            }
            
        pcm_buffer_size = data_size * dec_ctx->channels * frame->nb_samples;   
    }
    return 0;
}

static void fill_audio_buffer(void *userdata, Uint8 * stream, int len)
{
	SDL_memset(stream, 0, len);
	// ???????????????????????????
	if (audio_len == 0)
		return;

	len = (len > audio_len ? audio_len : len);
	SDL_MixAudio(stream, pAudio_pos, len, SDL_MIX_MAXVOLUME);
	pAudio_pos += len;
	audio_len -= len;
}


int main_audio_aac(const char *src,const char *des)
{
    SDL_AudioSpec audioSpec;

    /*----------------------------------ffmpeg--------------------------------*/
    const char *outfilename, *filename;
    const AVCodec *codec;
    AVCodecContext *c= NULL;
    AVCodecParserContext *parser = NULL;
    int ret;
    FILE *f, *outfile;
    uint8_t *data;
    size_t   data_size;
    AVPacket *pkt;
    AVFrame *decoded_frame = NULL;
    enum AVSampleFormat sfmt;
    int n_channels = 0;
    const char *fmt;
    const AVOutputFormat* input_fmt = NULL;
    uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];


    /*----------------------------------ffmpeg--------------------------------*/
    //?????????SDL
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    {
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    //??????????????????????????????
    // ???????????????????????????????????????48000???44100???
    audioSpec.freq = 48000; 
    // ?????????????????????
    audioSpec.format = AUDIO_F32SYS;
    // ????????????????????????????????????1?????????????????????2
    audioSpec.channels = 2;
    // ??????????????????
    audioSpec.silence = 0;

    // ???????????????????????????????????????????????????2???n??????
    audioSpec.samples = 1024;

    // ????????????????????????????????????
    audioSpec.callback = fill_audio_buffer;

    // ??????????????????
    if (SDL_OpenAudio(&audioSpec, NULL) < 0)
    {
        printf("Can not open audio!");
        return -1;
    }


    /*----------------------------------ffmpeg--------------------------------*/
    
    filename    = src;
    outfilename = des;
    
     /* find the MPEG audio decoder */
    input_fmt = av_guess_format(NULL, filename, NULL); 
    
    codec = avcodec_find_decoder(input_fmt->audio_codec);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }
    memset(inbuf + AUDIO_INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "Parser not found\n");
        return -1;
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return -1;
    }

    c->codec_id = input_fmt->video_codec;
    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return -1;
    }

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        return -1;
    }
    outfile = fopen(outfilename, "wb");
    if (!outfile) {
        av_free(c);
        return -1;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        printf(">>>====error av_packet_alloc \n");
        return -1;
    }

    decoded_frame = av_frame_alloc();
    if (!decoded_frame) {
        printf(">>>====error av_frame_alloc \n");
        return -1;
    }

    pcm_buffer_size = audioSpec.samples * audioSpec.channels * 4;      //?????????????????????
    printf(">>>===222 size = %d\n",pcm_buffer_size);
    pcm_buffer_size = 8192;
    pcm_buffer = (char *)malloc(pcm_buffer_size);
    

    SDL_PauseAudio(0);
    while (!feof(f)) {
        data_size = fread(inbuf, 1, AUDIO_INBUF_SIZE, f);
        if (!data_size)
        {
            fprintf(stderr, "Could not read audio frame\n");
            fclose(f);
            return -1;
        }
        data = inbuf;
        while (data_size > 0) {
            ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                                   data, data_size,
                                   AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                return -1;
            }
            data      += ret;
            data_size -= ret;

            if (pkt->size) {
                decode(c, pkt, decoded_frame,outfile);
                pAudio_chunk = (Uint8 *)pcm_buffer;
                audio_len = pcm_buffer_size;
                pAudio_pos = pAudio_chunk;
                
                while (audio_len > 0)
                    SDL_Delay(1);
            }
        }
    }
    /* flush the decoder */
    decode(c, NULL, decoded_frame,outfile);
    
    /*----------------------------------ffmpeg--------------------------------*/
#if 0

    FILE *pAudioFile = fopen("test.pcm", "rb+");
    if (pAudioFile == NULL)
    {
        printf("Can not open audio file!");
        return -1;
    }

    int pcm_buffer_size = 4096;
    pcm_buffer = (char *)malloc(pcm_buffer_size);
    int data_count = 0;

    SDL_PauseAudio(0);

    for (;;)
    {
        // ????????????
        if (fread(pcm_buffer, 1, pcm_buffer_size, pAudioFile) != pcm_buffer_size)
        {
            fseek(pAudioFile, 0, SEEK_SET);
            fread(pcm_buffer, 1, pcm_buffer_size, pAudioFile);
            data_count = 0;
        }
        printf("Playing %10d Bytes data.\n", data_count);
        data_count += pcm_buffer_size;

        pAudio_chunk = (Uint8 *)pcm_buffer;
        audio_len = pcm_buffer_size;
        pAudio_pos = pAudio_chunk;
        
        while (audio_len > 0)
            SDL_Delay(1);
    }
#endif
    free(pcm_buffer);
    SDL_Quit();
    fclose(f);

    avcodec_free_context(&c);
    av_parser_close(parser);
    av_frame_free(&decoded_frame);
    av_packet_free(&pkt);
    return 0;
}

