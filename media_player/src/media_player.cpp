#include"media_player.h"

#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

int video_count = 0;
int audio_count = 0;

cmedia_player::cmedia_player()
{
    //编解码
    ifmt_ctx = NULL;
    pCodecCtx= NULL;
    pCodecCtx_v= NULL;
    pCodecCtx_a= NULL;

    //编码信息
    stream_idx_v = -1;
    stream_idx_a = -1;
    width = -1;
    height = -1;;
    pix_fmt = AV_PIX_FMT_NONE;
    pictq_windex = 0;
    pictq_rindex = 0;
    pictq_size = 0;   
   
    audio_buf_size = 0;
    audio_buf_index = 0;
    audio_frame = NULL;
    audio_pkt = NULL;
    audio_pkt_data = NULL;
    audio_pkt_size = 0;
    
    //同步 
    video_clock = 0;
    audio_clock = 0;
    frame_timer = 0;
    frame_last_pts = 0;
    frame_last_delay = 0;  

    //系统
    quit = false; 
    thread_pause=0;
    parse_tid = NULL;
    video_tid = NULL;

    //SDL
    win = NULL;
    ren = NULL;
    tex = NULL;



}

// 初始化队列
void cmedia_player::packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int cmedia_player::open_input_file(const char *filename)
{
    int ret = 0;

    infile = filename;

    avformat_network_init();
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        printf("Cannot open input file\n");
        return ret;
    }
 
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        printf("Cannot find stream information\n");
        return ret;
    }
    av_dump_format(ifmt_ctx, 0, filename, 0);
    return ret;

}

int cmedia_player::open_codec_context(enum AVMediaType type)
{
    int ret = 0, stream_index;
    AVStream *st = NULL;
    const AVCodec *dec = NULL;
    AVDictionary *opts = NULL;


    ret = av_find_best_stream(ifmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        printf("Could not find %s stream in input file\n"),
                av_get_media_type_string(type);
        return ret;
    } else {
        stream_index = ret;
        st = ifmt_ctx->streams[stream_index];
        
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            printf("Failed to find %s codec\n"),
                    av_get_media_type_string(type);
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        pCodecCtx = avcodec_alloc_context3(dec);
        if (!pCodecCtx) {
            printf("Failed to allocate the %s codec context\n"),
                    av_get_media_type_string(type);
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(pCodecCtx, st->codecpar)) < 0) {
            printf("Failed to copy %s codec parameters to decoder context\n"),
                    av_get_media_type_string(type);
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(pCodecCtx, dec, &opts)) < 0) {
            printf("Failed to open %s codec\n"),
                    av_get_media_type_string(type);
            return ret;
        }
        if(type == AVMEDIA_TYPE_VIDEO) {
            stream_idx_v = stream_index;
            width = pCodecCtx->width;
            height = pCodecCtx->height;
            pix_fmt = pCodecCtx->pix_fmt;
            pCodecCtx_v = pCodecCtx;
            video_st = st;
            
        }
        else if(type == AVMEDIA_TYPE_AUDIO) {
            stream_idx_a = stream_index;
            pCodecCtx_a = pCodecCtx;
            audio_st = st;

        }

        pCodecCtx = NULL;
    }

    return 0;
}

int cmedia_player::alloc_image()
{
    audio_pkt = av_packet_alloc();
    if (!audio_pkt)
        return -1;

    audio_frame = av_frame_alloc();
    if (!audio_frame) {
        printf("Could not allocate video frame\n");
        return -1;
    }
    return 0;
}

int cmedia_player::packet_queue_put(PacketQueue *q, AVPacket *pkt) {

    AVPacketList *pkt1;
    if (av_packet_make_refcounted(pkt) < 0) {
        return -1;
    }
    pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}
int cmedia_player::packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {

        if (quit) {
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
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}
//解码后视频帧保存
int cmedia_player::queue_picture(AVFrame *pFrame, double pts) {

    VideoPicture *vp;

    /* wait until we have space for a new pic */
    SDL_LockMutex(pictq_mutex);
    while (pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !quit) {
        //等待pictq_cond信号量，同时暂时释放pictq_mutex，让其他线程获得，被唤醒时再度获取这个锁
        SDL_CondWait(pictq_cond, pictq_mutex);
    }
    SDL_UnlockMutex(pictq_mutex);

    if (quit) {
        return -1;
    }

    // windex is set to 0 initially
    vp = &pictq[pictq_windex];


//    /* allocate or resize the buffer! */
    if (!vp->frame ||
        vp->width != pCodecCtx_v->width ||
        vp->height != pCodecCtx_v->height) {

        vp->frame = av_frame_alloc();
        if (quit) {
            return -1;
        }
    }

    /* We have a place to put our picture on the queue */
    if (vp->frame) {
        vp->pts = pts;

        vp->frame = pFrame;
        /* now we inform our display thread that we have a pic ready */
        if (++pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
            pictq_windex = 0;
        }

        SDL_LockMutex(pictq_mutex);
        pictq_size++;
        SDL_UnlockMutex(pictq_mutex);
    }
    return 0;
}

//  视频同步，获取正确的视频PTS
double cmedia_player::synchronize_video(AVFrame *src_frame, double pts) {

    double frame_delay;

    if (pts != 0) {
        video_clock = pts;
    } else {
        //出错时使用上一帧的时间
        pts = video_clock;
    }
    /* 根据时间基，计算每一帧的间隔时间 */
    frame_delay = av_q2d(pCodecCtx_v->time_base);
    /* if we are repeating a frame, adjust clock accordingly */
    //计算需每帧图片要延迟的时间
    //extra_delay = repeat_pict / (2*fps)  ->  fps=1/time_base  ->  
    //frame_delay = time_base的换算后时间  -> extra_delay = repeat_pict * frame_delay /2
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    video_clock += frame_delay;
    return pts;
}

//视频解码线程
void *cbx_video_thread(void *arg)
{
    cmedia_player* tmp_serial=(cmedia_player*)arg;
    AVPacket pkt1, *packet = &pkt1;
    AVFrame *pFrame;
    double pts;
    int iout,iout_2;

    pFrame = av_frame_alloc(); 
    
    for (;;) {
        // 从视频队列中取出packet
        if (tmp_serial->packet_queue_get(&tmp_serial->videoq, packet, 1) < 0) {
            break;
        }
        // 解码
        avcodec_send_packet(tmp_serial->pCodecCtx_v, packet);
        while (avcodec_receive_frame(tmp_serial->pCodecCtx_v, pFrame) >= 0) {
            if ((pts = pFrame->best_effort_timestamp) != AV_NOPTS_VALUE) {
            } else {
                pts = 0;
            }
            pts *= av_q2d(tmp_serial->video_st->time_base);

            // 同步
            pts = tmp_serial->synchronize_video(pFrame, pts);
            if (tmp_serial->queue_picture(pFrame, pts) < 0) {
                break;
            }
            
            av_packet_unref(packet);
        }
    }
    av_frame_free(&pFrame);    
}
double cmedia_player::get_audio_clock() {
    double pts;
    int hw_buf_size, bytes_per_sec, n;

    //上一步获取的PTS
    pts = audio_clock;
    // 音频缓冲区还没有播放的数据
    hw_buf_size = audio_buf_size - audio_buf_index;
    // 每秒钟音频播放的字节数
    bytes_per_sec = 0;
    n = pCodecCtx_a->channels * 2;
    if (audio_st) {
        bytes_per_sec = pCodecCtx_a->sample_rate * n;
    }
    if (bytes_per_sec) {
        pts -= (double) hw_buf_size / bytes_per_sec;
    }
    return pts;
}


// 音频帧解码
int cmedia_player::audio_decode_frame(uint8_t *audio_buf, int buf_size, double *pts_ptr) {

    int len1, data_size = 0;
    double pts;
    int n;
    AVFrame *frame;
    
    for (;;) {
        while (audio_pkt_size > 0) {
            avcodec_send_packet(pCodecCtx_a, audio_pkt);
            while (avcodec_receive_frame(pCodecCtx_a, audio_frame) >= 0) {
                len1 = audio_frame->pkt_size;

                if (len1 < 0) {
                    /* if error, skip frame */
                    audio_pkt_size = 0;
                    break;
                }

                data_size = 2 * audio_frame->nb_samples * 2;
//                assert(data_size <= buf_size);

                swr_convert(audio_swr_ctx,
                            &audio_buf,
                            MAX_AUDIO_FRAME_SIZE * 3 / 2,
                            (const uint8_t **) audio_frame->data,
                            audio_frame->nb_samples);

            }
            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            if (data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            pts = audio_clock;
            *pts_ptr = pts;
            n = 2 * pCodecCtx_a->channels;
            //计算得到已播放的时长,每播放一帧都会增加
            audio_clock += (double) data_size /
                               (double) (n * pCodecCtx_a->sample_rate);
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if (audio_pkt->data)
            av_packet_unref(audio_pkt);

        if (quit) {
            return -1;
        }
        /* next packet */
        if (packet_queue_get(&audioq, audio_pkt, 1) < 0) {
            return -1;
        }
        audio_pkt_data = audio_pkt->data;
        audio_pkt_size = audio_pkt->size;
        printf("audio count = %d  pkt->size = %d  \n",audio_count++,audio_pkt->size);
        /* if update, update the audio clock w/pts */
        if (audio_pkt->pts != AV_NOPTS_VALUE) {
            audio_clock = av_q2d(audio_st->time_base) * audio_pkt->pts;
        }
    }
}

// 音频设备回调
void audio_callback(void *userdata, Uint8 *stream, int len) {

    cmedia_player* tmp_serial=(cmedia_player*)userdata;
    int len1, audio_size;
    double pts;

    SDL_memset(stream, 0, len);

    while (len > 0 && !tmp_serial->thread_pause) {
        //判断本次解码的数据是否全部播放完，全部播放完后解码下一个pkt
        if (tmp_serial->audio_buf_index >= tmp_serial->audio_buf_size) {
            // 音频解码
            audio_size = tmp_serial->audio_decode_frame(tmp_serial->audio_buf, sizeof(tmp_serial->audio_buf), &pts);
            if (audio_size < 0) {
                // 音频解码错误，播放静音
                printf("audio_decode_frame error!! play mute....  audio_size = %d\n",audio_size);
                tmp_serial->audio_buf_size = 1024 * 2 * 2;
                memset(tmp_serial->audio_buf, 0, tmp_serial->audio_buf_size);
            } else {
                tmp_serial->audio_buf_size = audio_size;
            }
            tmp_serial->audio_buf_index = 0;
        }
        len1 = tmp_serial->audio_buf_size - tmp_serial->audio_buf_index;
        if (len1 > len)
            len1 = len;
        SDL_MixAudio(stream, (uint8_t *) tmp_serial->audio_buf + tmp_serial->audio_buf_index, len1, SDL_MIX_MAXVOLUME);
        len -= len1;
        stream += len1;
        tmp_serial->audio_buf_index += len1;
    }
}
int cmedia_player::video_component_open() {
//    frame_timer = (double) av_gettime() / 1000000.0;
    frame_last_delay = 40e-3;
    packet_queue_init(&videoq);

    pthread_create(&video_tid, 0, cbx_video_thread, this);

}
int cmedia_player::audio_component_open() {

    SDL_AudioSpec wanted_spec;

    // Set audio settings from codec info
    wanted_spec.freq = pCodecCtx_a->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;//codecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = this;

    fprintf(stderr, "wanted spec: channels:%d, sample_fmt:%d, sample_rate:%d \n",
            2, AUDIO_S16SYS, pCodecCtx_a->sample_rate);

    // 打开音频设备
    if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
        fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
        return -1;
    }

    audio_buf_size = 0;
    audio_buf_index = 0;
    
    memset(audio_pkt, 0, sizeof(*audio_pkt));
    packet_queue_init(&audioq);

    //Out Audio Param
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;

    int out_nb_samples = pCodecCtx_a->frame_size;

    int out_sample_rate = pCodecCtx_a->sample_rate;
    int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);


    int64_t in_channel_layout = av_get_default_channel_layout(pCodecCtx_a->channels);

    // 音频重采样
    struct SwrContext *audio_convert_ctx;
    audio_convert_ctx = swr_alloc();
    swr_alloc_set_opts(audio_convert_ctx,
                       out_channel_layout,
                       AV_SAMPLE_FMT_S16,
                       out_sample_rate,
                       in_channel_layout,
                       pCodecCtx_a->sample_fmt,
                       pCodecCtx_a->sample_rate,
                       0,
                       NULL);

    swr_init(audio_convert_ctx);
    audio_swr_ctx = audio_convert_ctx;

    // 开始播放音频，audio_callback回调
    SDL_PauseAudio(0);

}

//分解线程
void *cbx_parse_thread(void *arg)
{
    cmedia_player* tmp_serial=(cmedia_player*)arg;
    int ret;
    AVCodecContext *pCodecCtx;
    AVPacket  *pkt;
    int count;
    
    pkt = av_packet_alloc();
    //初始化视频相关组件、解码线程
    tmp_serial->video_component_open();

    //初始化音频相关组件、解码线程
    tmp_serial->audio_component_open();


    while(1) {
        ret = av_read_frame(tmp_serial->ifmt_ctx,pkt);
        if (ret < 0)
            break;
        if(pkt->stream_index == tmp_serial->stream_idx_v) {
            tmp_serial->packet_queue_put(&tmp_serial->videoq, pkt);
        }
        else if(pkt->stream_index == tmp_serial->stream_idx_a) {
            tmp_serial->packet_queue_put(&tmp_serial->audioq, pkt);
        }
    }
    av_packet_free(&pkt);
}


// 定时器回调函数，发送FF_REFRESH_EVENT事件，更新显示视频帧
Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) {

    cmedia_player* tmp_serial=(cmedia_player*)opaque;
    SDL_Event event;
	if (!tmp_serial->quit) {
    	if(!tmp_serial->thread_pause){  
            event.type = SFM_REFRESH_EVENT;
            event.user.data1 = opaque;
            SDL_PushEvent(&event);
        }
	}
	tmp_serial->quit=0;

    return 0;
}


// 设置定时器
void cmedia_player::schedule_refresh(int delay) {
    SDL_AddTimer(delay, sdl_refresh_timer_cb, this);
}

void cmedia_player::creat_demux_thread()
{
    //创建复分解线程
    pthread_create(&parse_tid, 0, cbx_parse_thread, this);

}

int cmedia_player::init_sdl()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_AUDIO) != 0)
        {
            printf("SDL_Init error!\n",SDL_GetError());
            return -1;
        }
    
        //创建窗口
        win = NULL;
        win = SDL_CreateWindow("Media player",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        if (win == NULL)
        {
            printf("SDL_CreateWindow error! error : %s\n",SDL_GetError());
            return -1;
        }
    
        //创建渲染器

        ren = SDL_CreateRenderer(win, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == NULL)
        {
            printf("SDL_CreateRenderer error! error : %s\n",SDL_GetError());
            return -1;
        }

        //创建一个锁和信号量，主要用于存放解码后视频帧个数的变量pictq_size会被两个线程操作
        pictq_mutex = SDL_CreateMutex();
        pictq_cond = SDL_CreateCond();

        //创建纹理 需要修改  
        tex = SDL_CreateTexture(ren,
                                    SDL_PIXELFORMAT_IYUV,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    width,
                                    height);

        // 定时刷新器，主要用来控制视频的刷新
        schedule_refresh(50);

        return 0;
}
// 视频播放
void cmedia_player::video_display() {

    SDL_Rect rect;
    VideoPicture *vp;

    vp = &pictq[pictq_rindex];

    // 渲染播放
    if (vp->frame) {
        SDL_UpdateYUVTexture(tex, NULL,
                             vp->frame->data[0], vp->frame->linesize[0],
                             vp->frame->data[1], vp->frame->linesize[1],
                             vp->frame->data[2], vp->frame->linesize[2]);
#if 0        
        SDL_UpdateTexture(tex, NULL,vp->frame->data[0], vp->frame->linesize[0]);
#endif
        rect.x = 0;
        rect.y = 0;
        rect.w = pCodecCtx_v->width;
        rect.h = pCodecCtx_v->height;
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, &rect);
        SDL_RenderPresent(ren);
    }
}

// 视频刷新播放，并预测下一帧的播放时间，设置新的定时器
void cmedia_player::video_refresh_timer(void *userdata) {

    cmedia_player* tmp_serial=(cmedia_player*)userdata;
    VideoPicture *vp;
    double actual_delay, delay, sync_threshold, ref_clock, diff;

    if (tmp_serial->video_st) {
        if (tmp_serial->pictq_size == 0) {
            schedule_refresh(1);
        } else {
            // 从数组中取出一帧视频帧
            vp = &tmp_serial->pictq[tmp_serial->pictq_rindex];
            
            // 当前Frame时间减去上一帧的时间，获取两帧间的时差
            delay = vp->pts - tmp_serial->frame_last_pts;
            if (delay <= 0 || delay >= 1.0) {
                // 延时小于0或大于1秒（太长）都是错误的，将延时时间设置为上一次的延时时间
                delay = tmp_serial->frame_last_delay;
            }
            // 保存延时和PTS，等待下次使用
            tmp_serial->frame_last_delay = delay;
            tmp_serial->frame_last_pts = vp->pts;

            // 获取音频Audio_Clock
            ref_clock = get_audio_clock();
            // 得到当前PTS和Audio_Clock的差值
            diff = vp->pts - ref_clock;

            /* Skip or repeat the frame. Take delay into account
               FFPlay still doesn't "know if this is the best guess." */
            sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
            if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
                //音频快了 减少延时
                if (diff <= -sync_threshold) {
                    delay = 0;
                } 
                //视频快了 增大延时
                else if (diff >= sync_threshold) {
                    delay = delay*3;
                }
            }
            tmp_serial->frame_timer = delay;
            // 最终真正要延时的时间
            actual_delay = tmp_serial->frame_timer;
            if (actual_delay < 0.010) {
                // 延时时间过小就设置最小值
                actual_delay = 0.010;
            }
            printf("video count = %d  actual_delay = %lf\n",video_count++,actual_delay);
            // 根据延时时间重新设置定时器，刷新视频
            schedule_refresh((int) (actual_delay * 1000 + 0.5));

            // 视频帧显示
            video_display();

            // 更新视频帧数组下标
            if (++tmp_serial->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
                tmp_serial->pictq_rindex = 0;
            }
            SDL_LockMutex(tmp_serial->pictq_mutex);
            // 视频帧数组减一
            tmp_serial->pictq_size--;
            SDL_CondSignal(tmp_serial->pictq_cond);
            SDL_UnlockMutex(tmp_serial->pictq_mutex);
        }
    } else {
        schedule_refresh(100);
    }
}

void cmedia_player::recv_event()
{
    int ret;
    
    while(!quit)
        {
            SDL_WaitEvent(&event);
            switch (event.type)
            {
                case SDL_QUIT:                      //用户请求退出
                    printf("SDL_QUIT\n");
                    quit = true;
                    break;
                case SFM_REFRESH_EVENT:             //用户刷新事件
                    video_refresh_timer(event.user.data1);
                    break;
                case SDL_KEYDOWN:                   //用户键盘输入事件  ，暂停事件
                    if(event.key.keysym.sym==SDLK_SPACE) {
                        printf("SDLK_SPACE\n");
                        thread_pause=!thread_pause;
                        if(!thread_pause)
                            schedule_refresh(1);    //由暂停开始播放 再次发送一个刷新视频帧的事件
                    }
                    break;
                default:
                    break;
            }
        }
}

