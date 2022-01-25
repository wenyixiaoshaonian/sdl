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
int thread_exit=0;
int thread_pause=0;
static int video_dst_linesize[4];
static int video_dst_bufsize;
static uint8_t *video_dst_data[4] = {NULL};
static AVCodecContext *pCodecCtx_v= NULL;
static AVCodecContext *pCodecCtx_a= NULL;
static FILE *video_dst_file = NULL;

#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)


//视频刷新线程 40ms发送一个刷新事件
int sfp_refresh_thread(void *opaque){
	thread_exit=0;
	thread_pause=0;
    SDL_Event event;
    
	while (!thread_exit) {
		if(!thread_pause){
			event.type = SFM_REFRESH_EVENT;
			SDL_PushEvent(&event);
		}
		SDL_Delay(1000 / 60);
	}
	thread_exit=0;
	thread_pause=0;
	//Break
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);
 
	return 0;
}
static int decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt)
{
    char buf[1024];
    int ret;
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return ret;
        }

        fflush(stdout);
        
        printf(">>>==== frame: number : %d   width = %d, height = %d, format = %d\n",
                        dec_ctx->frame_number,frame->width, frame->height,
                        dec_ctx->pix_fmt);


        //如果原始格式为yuv，将解码帧复制到目标缓冲区后直接写入

        /* copy decoded frame to destination buffer:
         * this is required since rawvideo expects non aligned data */
        av_image_copy(video_dst_data, video_dst_linesize,
                      (const uint8_t **)(frame->data), frame->linesize,
                      dec_ctx->pix_fmt, dec_ctx->width, dec_ctx->height);

        /* write to rawvideo file */
        fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);

    }
    return ret;
}
                   

static int open_codec_context(AVFormatContext *ifmt,AVCodecContext **pCodecCtx,int *stream_idx,enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;
    AVDictionary *opts = NULL;


    ret = av_find_best_stream(ifmt, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file\n",
                av_get_media_type_string(type));
        return ret;
    } else {
        stream_index = ret;
        st = ifmt->streams[stream_index];
        
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *pCodecCtx = avcodec_alloc_context3(dec);
        if (!*pCodecCtx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*pCodecCtx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(*pCodecCtx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

static int open_input_file(const char *filename,AVFormatContext **ifmt)
{
    int ret;
    AVCodec *dec;
 
    if ((ret = avformat_open_input(ifmt, filename, NULL, NULL)) < 0) {
        printf( "Cannot open input file\n");
        return ret;
    }
 
    if ((ret = avformat_find_stream_info(*ifmt, NULL)) < 0) {
        printf( "Cannot find stream information\n");
        return ret;
    }
    av_dump_format(*ifmt, 0, filename, 0);
    return ret;
}

int main_hard(const char *src)
{
    //-----------ffmpeg------------
    AVFormatContext *ifmt_ctx = NULL;

    
    const char *infile,*out_videofile,*out_audiofile;
    AVStream *out_v_stream,*out_a_stream;
    int stream_idx_v,stream_idx_a;
    AVStream *out_stream_v,*out_stream_a;
    AVPacket *pkt;
    const AVOutputFormat *ofmt_v = NULL,*ofmt_a = NULL;
    int width, height;
    enum AVPixelFormat pix_fmt;
    AVFrame *frame;

    int ret;


    //-----------SDL---------------
    SDL_Event e;
    SDL_Thread *video_tid;
    bool quit = false;

    //----------------------------------------ffmpeg初始化--------------------------------------------
    infile        = src;
    
    //打开输入文件 获取其中的数据流信息
    open_input_file(infile,&ifmt_ctx);
       
    //获取编码器信息
    open_codec_context(ifmt_ctx,&pCodecCtx_v,&stream_idx_v,AVMEDIA_TYPE_VIDEO);
    open_codec_context(ifmt_ctx,&pCodecCtx_a,&stream_idx_a,AVMEDIA_TYPE_AUDIO);

    width = pCodecCtx_v->width;
    height = pCodecCtx_v->height;
    pix_fmt = pCodecCtx_v->pix_fmt;
    ret = av_image_alloc(video_dst_data, video_dst_linesize,
                         width, height, pix_fmt, 1);
    if (ret < 0) {
        std::cout << "Could not allocate raw video buffer\n";
        return 1;
    }
    video_dst_bufsize = ret;

    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    video_dst_file = fopen("test.yuv", "wb+");
    if (!video_dst_file) {
        fprintf(stderr, "Could not open %s\n", "test.yuv");
        exit(1);
    }

    //----------------------------------------SDL初始化--------------------------------------------
    //初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

    //创建窗口
    SDL_Window *win = NULL;
    win = SDL_CreateWindow("Hello SDL2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        960, 544, SDL_WINDOW_SHOWN);
    if (win == NULL)
    {
        std::cout << SDL_GetError() << std::endl;
        return 1;
    }

    //创建渲染器
    SDL_Renderer *ren = NULL;
    ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (ren == NULL)
    {
        std::cout << SDL_GetError() << std::endl;
        return 1;
    }
#if 0
    //创建表面
    SDL_Surface *bmp = NULL;
    bmp = IMG_Load("press.jpg");
    if (bmp == NULL) {
        std::cout << SDL_GetError() << std::endl;
        return 1;
    }
#endif
    //创建材质
    SDL_Texture *tex = NULL;
    
//    SDL_FreeSurface(bmp);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx_v->width, pCodecCtx_v->height);//创建对应纹理


    video_tid = SDL_CreateThread(sfp_refresh_thread,NULL,NULL);


    //--------------------------------------开始工作-----------------------------------------
    while(!quit)
    {
        SDL_WaitEvent(&e);
        switch (e.type)
        {
            case SDL_QUIT:                      //用户请求退出
                printf(">>>===SDL_QUIT\n");
                thread_exit=1;
                break;
            case SFM_REFRESH_EVENT:             //用户刷新事件
                //todo
                //----------------------------ffmpeg-----------------------------------------
                //读一帧数据
                ret = av_read_frame(ifmt_ctx,pkt);
                if (ret < 0)
                    break;
                if(pkt->stream_index == stream_idx_v) {
                    ret = decode(pCodecCtx_v, frame, pkt);
                    if(ret < 0) {
                        std::cout << "decode error!!!\n";
                        return 1;
                    }
                }
                else if(pkt->stream_index == stream_idx_a) {
                //todo
                ;
                }
                av_packet_unref(pkt);

                //-------------------------------sdl-----------------------------------------
                //填充材质
                SDL_UpdateTexture(tex,NULL,video_dst_data[0],video_dst_linesize[0]);
                //清空渲染器
                SDL_RenderClear(ren);
                //将材质复制到渲染器
                SDL_RenderCopy(ren, tex, NULL, NULL);
                //呈现渲染器
                SDL_RenderPresent(ren);
                break;
            case SDL_KEYDOWN:                   //用户键盘输入事件  ，暂停事件
                if(e.key.keysym.sym==SDLK_SPACE)
				    thread_pause=!thread_pause;
                break;
            case SFM_BREAK_EVENT:               //退出事件
                quit = true;
                break;
            default:
                printf(">>>===error event type...\n");
                break;
        }

    }
    //释放资源
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    //退出SDL
    SDL_Quit();
  }

