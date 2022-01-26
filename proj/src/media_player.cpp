#include "media_player.h"


#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

media_player::media_player()
{
    //ffmpeg
    ifmt_ctx = NULL;
    stream_idx_v = -1;
    stream_idx_a = -1;
    pkt = NULL;
    frame = NULL;
    width = -1;
    height = -1;;
    pix_fmt = AV_PIX_FMT_NONE;
    pCodecCtx_v= NULL;
    pCodecCtx_a= NULL;
    video_dst_data[4] = {NULL};
    video_dst_linesize[4] = 0;
    video_dst_bufsize = -1;
    audio_frame_count = 0;
    //SDL
    video_tid= NULL;
    quit = false; 
    thread_exit=0;
    thread_pause=0;
}
media_player::~media_player()
{
#if 0
    //释放资源
    avcodec_free_context(&pCodecCtx_v);
    avcodec_free_context(&pCodecCtx_a);
    avformat_close_input(&ifmt_ctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
#endif
    //退出SDL
    SDL_Quit();
}
int media_player::open_input_file(const char *filename)
{
    int ret = 0;

    infile = filename;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
//        std::cout << "Cannot open input file\n";
        printf("Cannot open input file");
        return ret;
    }
 
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        printf("Cannot find stream information");
        return ret;
    }
//    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;

}

int media_player::open_codec_context(enum AVMediaType type)
{
    int ret = 0, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(ifmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        printf("Could not find %s stream in input file\n",
                av_get_media_type_string(type));
        return ret;
    } else {
        stream_index = ret;
        st = ifmt_ctx->streams[stream_index];
        
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            printf("Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        pCodecCtx = avcodec_alloc_context3(dec);
        if (!pCodecCtx) {
            printf("Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(pCodecCtx, st->codecpar)) < 0) {
            printf("Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(pCodecCtx, dec, &opts)) < 0) {
            printf("Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        if(type == AVMEDIA_TYPE_VIDEO) {
            stream_idx_v = stream_index;
            width = pCodecCtx->width;
            height = pCodecCtx->height;
            pix_fmt = pCodecCtx->pix_fmt;
            pCodecCtx_v = pCodecCtx;
        }
        else if(type == AVMEDIA_TYPE_AUDIO) {
            stream_idx_a = stream_index;
            pCodecCtx_a = pCodecCtx;
        }
        pCodecCtx = NULL;

    }

    return 0;
}

int media_player::alloc_image()
{
    int ret;
    ret = av_image_alloc(video_dst_data, video_dst_linesize,
                         width, height, pix_fmt, 1);
    if (ret < 0) {
        printf("Could not allocate raw video buffer\n");
        return ret;
    }
    video_dst_bufsize = ret;

    pkt = av_packet_alloc();
    if (!pkt)
        return -1;

    frame = av_frame_alloc();
    if (!frame) {
        printf("Could not allocate video frame\n");
        return -1;
    }

    frame_ft = av_frame_alloc();
    if (!frame_ft) {
        printf("Could not allocate video frame\n");
        return -1;
    }
    return 0;
}

int media_player::decode_video()
{
    char buf[1024];
    int ret = 0;
    ret = avcodec_send_packet(pCodecCtx_v, pkt);
    if (ret < 0) {
        printf("Error sending a packet for decoding pkt->size: %d\n",pkt->size);
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(pCodecCtx_v, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            printf("EOF~~~\n");
            return 0;
        }
        else if (ret < 0) {
            printf("Error during decoding\n");
            return ret;
        }

        printf("saving frame %3d\n", pCodecCtx_v->frame_number);
        fflush(stdout);
        
//        printf(">>>==== frame: number : %d   width = %d, height = %d, format = %d\n",
//                        pCodecCtx_v->frame_number,frame->width, frame->height,
//                        pCodecCtx_v->pix_fmt);
#if 1        
        //--------------filter--------------------------------------------------------------
        frame->pts = frame->best_effort_timestamp;
        /* push the decoded frame into the filtergraph */
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0) 
            {
            printf("Error while feeding the filtergraph  ret = %d\n",ret);
            break;
        }        
        /* pull filtered frames from the filtergraph */
        while (1) {
            ret = av_buffersink_get_frame(buffersink_ctx, frame_ft);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                break;
            }
            printf(">>>==== 2222 frame: number : %d   width = %d, height = %d, format = %d\n",
                            pCodecCtx_v->frame_number,frame_ft->width, frame_ft->height,
                            pCodecCtx_v->pix_fmt);

//            write_frame(filt_frame,outfilename);
//            av_frame_unref(frame_ft);
            av_image_copy(video_dst_data, video_dst_linesize,
                          (const uint8_t **)(frame_ft->data), frame_ft->linesize,
                          pCodecCtx_v->pix_fmt, pCodecCtx_v->width, pCodecCtx_v->height);

        }
        //--------------filter--------------------------------------------------------------

#else

        //如果原始格式为yuv，将解码帧复制到目标缓冲区后直接写入

        /* copy decoded frame to destination buffer:
         * this is required since rawvideo expects non aligned data */
        av_image_copy(video_dst_data, video_dst_linesize,
                      (const uint8_t **)(frame->data), frame->linesize,
                      pCodecCtx_v->pix_fmt, pCodecCtx_v->width, pCodecCtx_v->height);
#endif
        /* write to rawvideo file */
 //       fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);

    }
    av_packet_unref(pkt);
    return 0;
}

int media_player::decode_audio()
{
    int ret, data_size;

    /* send the packet with the compressed data to the decoder */
    ret = avcodec_send_packet(pCodecCtx_a, pkt);
    if (ret < 0) {
        printf("Error submitting the packet to the decoder\n");
        return -1;
    }

    /* read all the output frames (in general there may be any number of them */
    while (ret >= 0) {
        ret = avcodec_receive_frame(pCodecCtx_a, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return -1;
        else if (ret < 0) {
            printf("Error during decoding\n");
            return -1;
        }
        data_size = av_get_bytes_per_sample(pCodecCtx_a->sample_fmt);
        if (data_size < 0) {
            /* This should not occur, checking just for paranoia */
            printf("Failed to calculate data size\n");
            return -1;
        }
 //       std::cout << "audio_frame n:%d nb_samples:%d pts:%s\n",
 //          audio_frame_count++, frame->nb_samples,
 //          av_ts2timestr(frame->pts, &pCodecCtx_a->time_base);
    }
    av_packet_unref(pkt);
    return 0;
}

int media_player::decode_func()
{
    int ret;
    AVCodecContext *pCodecCtx;

    ret = av_read_frame(ifmt_ctx,pkt);
    if (ret < 0){
        printf("av_read_frame error!!!\n");
        return ret;
    }
//    printf(">>>>2222 pkt->size = %d \n",pkt->size);
    if(pkt->stream_index == stream_idx_v) {
        ret = decode_video();
        if(ret < 0) {
            printf("decode_video error!!!\n");
            return ret;
        }
    }
    else if(pkt->stream_index == stream_idx_a) {
            return 0;       //audio在其他地方处理
    }
    return 0;
}
int media_player::init_filters()
{
    int ret;
    char args[512];
    enum AVPixelFormat pix_fmts[] = { pix_fmt, AV_PIX_FMT_NONE };
    AVRational time_base = ifmt_ctx->streams[stream_idx_v]->time_base;
//    filter_descr = "movie=demo.jpg[wm];[in][wm]overlay=55:55[out]";
//    filter_descr = "movie=demo.jpg,colorkey=black:1.0:1.0[wm];[in][wm]overlay=55:55[out]";
//    filter_descr = "movie=timg.jpeg,scale=iw/2:ih/2[wm];[in][wm]overlay=55:55[out]";   //控制大小
    filter_descr = "drawtext=fontsize=100:fontfile=FreeSerif.ttf:text='hello world':x=20:y=20:fontcolor=red";
    frame_ft = av_frame_alloc();
    if (!frame_ft) {
        printf("Could not allocate video frame\n");
        return -1;
    }

    //创建过滤器
    buffersrc = avfilter_get_by_name("buffer");
    buffersink = avfilter_get_by_name("buffersink");
    //创建inout
    outputs = avfilter_inout_alloc();
    inputs  = avfilter_inout_alloc();
    //创建graph
    filter_graph = avfilter_graph_alloc();

    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            width, height, pix_fmt,
            time_base.num, time_base.den,
            pCodecCtx_v->sample_aspect_ratio.num, pCodecCtx_v->sample_aspect_ratio.den);

    //在graph中创建过滤器实例
    //与avfilter_graph_alloc_filter一样，只是再用args 和 opaque初始化这个实例
    avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",args, NULL, filter_graph);
    avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",NULL, NULL, filter_graph);
    
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
#if 1
    //填充inout
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    //使用filter_descr中的指令填充graph
    //内部还是使用avfilter_link链接过滤器
    avfilter_graph_parse_ptr(filter_graph, filter_descr,&inputs, &outputs, NULL);
#else    
    //或者直接使用这种方式连接 通常用来重采样等格式转换
    avfilter_link(buffersrc_ctx, 0, buffersink_ctx, 0);
#endif
    //检查过滤器的完整性
    avfilter_graph_config(filter_graph, NULL);
    return 0;
}

void *sfp_refresh_thread(void* arg){

    media_player* tmp_serial=(media_player*)arg;

	tmp_serial->thread_exit=0;
	tmp_serial->thread_pause=0;
    
	while (!tmp_serial->thread_exit) {
		if(!tmp_serial->thread_pause){
			tmp_serial->event.type = SFM_REFRESH_EVENT;
			SDL_PushEvent(&tmp_serial->event);
		}
		SDL_Delay(1000 / 60);
	}
	tmp_serial->thread_exit=0;
	tmp_serial->thread_pause=0;
	//Break
	tmp_serial->event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&tmp_serial->event);

}

int media_player::init_sdl()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        {
            printf("SDL_Init Error: %s\n",SDL_GetError());
            return -1;
        }
//        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
        //创建窗口
        win = NULL;
        win = SDL_CreateWindow("Hello SDL2",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        if (win == NULL)
        {
            printf("SDL_CreateWindow Error: %s\n",SDL_GetError());
            return -1;
        }
    
        //创建渲染器
        ren = NULL;
        ren = SDL_CreateRenderer(win, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == NULL)
        {
            printf("SDL_CreateRenderer Error: %s\n",SDL_GetError());
            return -1;
        }

        //创建纹理
        tex = NULL;   
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);//创建对应纹理
//        video_tid = SDL_CreateThread(sfp_refresh_thread,NULL,this);
        pthread_create(&video_tid,0,sfp_refresh_thread,this);

        return 0;
}


void media_player::recv_event()
{
    int ret;
    
    while(!quit)
        {
            SDL_WaitEvent(&event);
            switch (event.type)
            {
                case SDL_QUIT:                      //用户请求退出
                    printf(">>>===SDL_QUIT\n");
                    thread_exit=1;
                    break;
                case SFM_REFRESH_EVENT:             //用户刷新事件
                    decode_func();
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
                    if(event.key.keysym.sym==SDLK_SPACE)
                        thread_pause=!thread_pause;
                    break;
                case SFM_BREAK_EVENT:               //退出事件
                    quit = true;
                    break;
                default:
//                    printf(">>>===error event type...\n");
                    break;
            }
        }
}


