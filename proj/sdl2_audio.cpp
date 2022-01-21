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
char *pcm_buffer;
int pcm_buffer_size;

#define AUDIO_INBUF_SIZE 20480

static int decode(AVCodecContext *dec_ctx, AVPacket *pkt, AVFrame *frame,
                   FILE *outfile)
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
            return;
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
        printf("audio_frame n:%d nb_samples:%d pts:%s\n",
           audio_frame_count++, frame->nb_samples,
           av_ts2timestr(frame->pts, &dec_ctx->time_base));
        
        for (i = 0; i < frame->nb_samples; i++)
            for (ch = 0; ch < dec_ctx->channels; ch++)
                memcpy(pcm_buffer + data_size*i,frame->data[ch] + data_size*i, data_size);
//                fwrite(frame->data[ch] + data_size*i, 1, data_size, outfile);

        pcm_buffer_size = frame->nb_samples * data_size * dec_ctx->channels;    //保存这帧的长度
    }
    return 0;
}

void fill_audio_buffer(void *userdata, Uint8 * stream, int len)
{
	SDL_memset(stream, 0, len);
	// 判断是否有读到数据
	if (audio_len == 0)
		return;

	len = (len > audio_len ? audio_len : len);
	SDL_MixAudio(stream, pAudio_pos, len, SDL_MIX_MAXVOLUME);
	pAudio_pos += len;
	audio_len -= len;
}


int main_audio(const char *src,const char *des)
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
    AVOutputFormat* input_fmt;
    uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];


    /*----------------------------------ffmpeg--------------------------------*/
    //初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    //初始化音频设备结构体
    // 音频数据的采样率。常用的有48000，44100等
    audioSpec.freq = 44100; 
    
    // 音频数据的格式
    audioSpec.format = AUDIO_S16SYS;
    
    // 声道数。例如单声道取值为1，立体声取值为2
    audioSpec.channels = 2;
    
    // 设置静音的值
    audioSpec.silence = 0;

    // 音频缓冲区中的采样个数，要求必须是2的n次方
    audioSpec.samples = 1024;

    // 填充音频缓冲区的回调函数
    audioSpec.callback = fill_audio_buffer;

    // 打开音频设备
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

    pcm_buffer_size = 4096;
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
                decode(c, pkt, decoded_frame);
                pAudio_chunk = (Uint8 *)pcm_buffer;
                audio_len = pcm_buffer_size;
                pAudio_pos = pAudio_chunk;
                
                while (audio_len > 0)
                    SDL_Delay(1);
            }
        }
    }
    /* flush the decoder */
    decode(c, NULL, decoded_frame, outfile);
    
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
        // 循环播放
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

