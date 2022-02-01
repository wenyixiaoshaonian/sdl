#pragma once
#include <iostream>
#include <SDL2/SDL.h>
#include "SDL2/SDL_image.h"
#include <cmath>

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>

#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale_internal.h>
#include <libswscale/swscale.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/time.h>

#include <libavdevice/avdevice.h>
}
#define MAX_AUDIO_FRAME_SIZE 192000 //channels(2) * data_size(2) * sample_rate(48000)
#define VIDEO_PICTURE_QUEUE_SIZE 1
#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0

// 解复用后音视频packet保存队列
typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

// 解码后视频帧
typedef struct VideoPicture {
    AVFrame *frame;
    int width, height;
    double pts; // 音视频同步后视频帧应该播放的时间
} VideoPicture;

class cmedia_player {
private:
    AVFormatContext *ifmt_ctx;          //媒体文件的抽象
    const char *infile;                 //输入媒体文件的路径
    AVCodecContext *pCodecCtx;          //解码上下文缓冲区

    //音频相关
    AVStream *audio_st; // 音频流
    AVCodecContext *pCodecCtx_a; // 音频解码上下文
    uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2]; // 音频缓存
    unsigned int audio_buf_size;
    unsigned int audio_buf_index;
    AVFrame *audio_frame; // 音频帧
    AVPacket *audio_pkt; // 音频包
    uint8_t *audio_pkt_data;
    int audio_pkt_size;
    struct SwrContext *audio_swr_ctx; // 音频重采样

    //视频信息
    int width, height;                  //video的分辨率
    enum AVPixelFormat pix_fmt;         //raw数据格式(YUV、RGB等)

    // 解码后视频帧数组
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE]; 
    int pictq_size, pictq_rindex, pictq_windex;

    //同步机制
    SDL_mutex *pictq_mutex;
    SDL_cond *pictq_cond;

    pthread_t parse_tid;                // 解复用线程
    pthread_t video_tid;                // 视频解码线程

    //SDL
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    SDL_Event event;                    //SDL事件
    bool quit;                          //SDL事件退出标志
    int thread_pause;
public:
    //// 同步相关
    double audio_clock;         //视频播放到当前帧时的已播放的时间长度
    double video_clock;         //视频播放到当前帧时的已播放的时间长度
    double frame_timer;         //当前视频帧的播放时间
    double frame_last_pts;      //上一帧视频帧的播放时间
    double frame_last_delay;    //上一次的视频帧延时时间

    //解码线程需要用到的资源
    AVCodecContext *pCodecCtx_v;        // 视频流解码上下文     
    PacketQueue videoq;                 // 视频流队列
    AVStream *video_st;                 // 视频流
    int stream_idx_v;                   //video数据流索引
    int stream_idx_a;                   //audio数据流索引
    PacketQueue audioq;                 // 音频队列

    cmedia_player();
    virtual ~cmedia_player() {printf("bye\n");};

    //ffmpeg
    int open_input_file(const char *filename);
    int open_codec_context(enum AVMediaType type);
    int alloc_image();
    
    //队列相关
    void packet_queue_init(PacketQueue *q);
    int packet_queue_put(PacketQueue *q, AVPacket *pkt);
    int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
    int queue_picture(AVFrame *pFrame, double pts);
    
    //SDL
    int init_sdl();  
    void recv_event();

    //同步
    double synchronize_video(AVFrame *src_frame, double pts);
    double get_audio_clock();


    // 音频帧解码
    int audio_component_open();
    int audio_decode_frame(uint8_t *audio_buf, int buf_size, double *pts_ptr);
    friend void audio_callback(void *userdata, Uint8 *stream, int len);

    int video_component_open();
    void schedule_refresh(int delay);
    void video_refresh_timer(void *userdata);
    void video_display();

    void creat_demux_thread();

    //相关回调
    friend void *cbx_parse_thread(void *arg);
    friend void *cbx_video_thread(void *arg);
    friend Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque);

};

