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
#include <pthread.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale_internal.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>


#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>

}

class media_player
{
private:
    //ffmpeg
    AVFormatContext *ifmt_ctx;          //媒体文件的抽象
    const char *infile;                 //输入媒体文件的路径
    int stream_idx_v;                   //video数据流索引
    int stream_idx_a;                   //audio数据流索引
    AVPacket *pkt;                      //存放解码前数据
    AVFrame *frame;                     //存放解码后数据
    int width, height;                  //video的分辨率
    enum AVPixelFormat pix_fmt;         //raw数据格式(YUV、RGB等)
    AVCodecContext *pCodecCtx_v;
    AVCodecContext *pCodecCtx_a;
    AVCodecContext *pCodecCtx;          //缓冲区
    //存放解码数据，格式转换后缓冲区的相关信息
    int video_dst_linesize[4];
    int video_dst_bufsize;
    uint8_t *video_dst_data[4];

    int audio_frame_count;
    //filter
    const char *filter_descr;
    const AVFilter *buffersrc;
    const AVFilter *buffersink;
    AVFilterInOut *outputs;
    AVFilterInOut *inputs;
    AVFilterGraph *filter_graph;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFrame *frame_ft;                     //存放filter处理后的数据
    //SDL
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    SDL_Event event;                    //SDL事件
    pthread_t video_tid;              //SDL视频刷新线程
    bool quit;                          //SDL事件退出标志
    int thread_exit;
    int thread_pause;

public:
    media_player();
    virtual ~media_player();

    //ffmpeg
    int open_input_file(const char *filename);
    int open_codec_context(enum AVMediaType type);
    int alloc_image();
    int decode_video();
    int decode_audio();
    int decode_func();
    
    //SDL
    int init_sdl();
    void recv_event();
    int init_filters();
    friend void *sfp_refresh_thread(void *opaque);

};

