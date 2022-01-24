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
#define MAX_AUDIO_FRAME_SIZE 192000 //channels(2) * data_size(2) * sample_rate(48000)
#define VIDEO_PICTURE_QUEUE_SIZE 1

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
    char *infile;                       //输入媒体文件的路径
    int stream_idx_v;                   //video数据流索引
    int stream_idx_a;                   //audio数据流索引
    AVCodecContext *pCodecCtx;          //解码上下文缓冲区

    //// 同步相关
    double audio_clock;
    double frame_timer;
    double frame_last_pts;
    double frame_last_delay;

    double video_clock; 
    double video_current_pts; 
    int64_t video_current_pts_time;  

    //音频相关
    AVStream *audio_st; // 音频流
    AVCodecContext *pCodecCtx_a; // 音频解码上下文
    PacketQueue audioq; // 音频队列
    uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2]; // 音频缓存
    unsigned int audio_buf_size;
    unsigned int audio_buf_index;
    AVFrame audio_frame; // 音频帧
    AVPacket audio_pkt; // 音频包
    uint8_t *audio_pkt_data;
    int audio_pkt_size;
    struct SwrContext *audio_swr_ctx; // 音频重采样


    //video
    AVStream *video_st; // 视频流
    AVCodecContext *pCodecCtx_v; // 视频流解码上下文
    PacketQueue videoq; // 视频流队列
    int width, height;                  //video的分辨率
    enum AVPixelFormat pix_fmt;         //raw数据格式(YUV、RGB等)

    //存放解码数据，格式转换后缓冲区的相关信息
    int video_dst_linesize[4];
    int video_dst_bufsize;
    uint8_t *video_dst_data[4];

    // 解码后视频帧数组
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE]; 
    int pictq_size, pictq_rindex, pictq_windex;
    SDL_mutex *pictq_mutex;
    SDL_cond *pictq_cond;

    pthread_t *parse_tid; // 解复用线程
    pthread_t *video_tid;// 视频解码线程

    //SDL
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    SDL_Event event;                    //SDL事件
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
//    int decode_video();
    int decode_audio();
//    int decode_func();
    
    //SDL
    int init_sdl();
    void packet_queue_init(PacketQueue *q);
    void recv_event();
    int packet_queue_put(PacketQueue *q, AVPacket *pkt);
    int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
    int queue_picture(AVFrame *pFrame, double pts);
    double synchronize_video(AVFrame *src_frame, double pts);
    int audio_component_open();
    void schedule_refresh(int delay);
    void video_refresh_timer(void *userdata);
    double get_audio_clock();
    void video_display();
    // 音频帧解码
    void audio_decode_frame(uint8_t *audio_buf, int buf_size, double *pts_ptr);

    friend void cbx_refresh_thread(void *opaque);
    friend void cbx_parse_thread(void *opaque);
    friend void cbx_video_thread(void *opaque);
    friend void audio_callback(void *userdata, Uint8 *stream, int len);
    friend Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque);

    int quit; // 退出标记位
} VideoState;

