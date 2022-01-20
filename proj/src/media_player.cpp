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
    *pCodecCtx_v= NULL;
    *pCodecCtx_a= NULL;
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
    //退出SDL
    SDL_Quit();
}
int media_player::open_input_file(const char *filename)
{
    int ret = 0;

    infile = filename;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        std::cout << "Cannot open input file\n";
        return ret;
    }
 
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        std::cout << "Cannot find stream information\n";
        return ret;
    }
    av_dump_format(ifmt_ctx, 0, filename, 0);
    return ret;

}

int media_player::open_codec_context(enum AVMediaType type)
{
    int ret = 0, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    AVCodecContext *pCodecCtx,

    if(type == AVMEDIA_TYPE_VIDEO)
        pCodecCtx = pCodecCtx_v;
    else if(type == AVMEDIA_TYPE_AUDIO)
        pCodecCtx = pCodecCtx_a;

    ret = av_find_best_stream(ifmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        std::cout << "Could not find %s stream in input file\n",
                av_get_media_type_string(type);
        return ret;
    } else {
        stream_index = ret;
        st = ifmt_ctx->streams[stream_index];
        
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            std::cout << "Failed to find %s codec\n",
                    av_get_media_type_string(type);
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        pCodecCtx = avcodec_alloc_context3(dec);
        if (!pCodecCtx) {
            std::cout << "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type);
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(pCodecCtx, st->codecpar)) < 0) {
            std::cout << "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type);
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(pCodecCtx, dec, &opts)) < 0) {
            std::cout << "Failed to open %s codec\n",
                    av_get_media_type_string(type);
            return ret;
        }
        if(type == AVMEDIA_TYPE_VIDEO) {
            stream_idx_v = stream_index;
            width = pCodecCtx->width;
            height = pCodecCtx->height;
            pix_fmt = pCodecCtx->pix_fmt;
        }
        else if(type == AVMEDIA_TYPE_AUDIO)
            stream_idx_a = stream_index;

    }

    return 0;
}

int media_player::alloc_image()
{
    int ret;
    ret = av_image_alloc(video_dst_data, video_dst_linesize,
                         width, height, pix_fmt, 1);
    if (ret < 0) {
        std::cout << "Could not allocate raw video buffer\n";
        return ret;
    }
    video_dst_bufsize = ret;

    pkt = av_packet_alloc();
    if (!pkt)
        return -1;

    frame = av_frame_alloc();
    if (!frame) {
        std::cout << "Could not allocate video frame\n";
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
        std::cout << "Error sending a packet for decoding\n";
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(pCodecCtx_v, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        else if (ret < 0) {
            std::cout << "Error during decoding\n";
            return ret;
        }

        printf("saving frame %3d\n", pCodecCtx_v->frame_number);
        fflush(stdout);
        
        std::cout << ">>>====  Width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height or "
                        "pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %d\n"
                        "new: width = %d, height = %d, format = %d\n",
                        pCodecCtx_v->width, pCodecCtx_v->height, pCodecCtx_v->pix_fmt,
                        frame->width, frame->height,
                        pCodecCtx_v->pix_fmt;


        //如果原始格式为yuv，将解码帧复制到目标缓冲区后直接写入

        /* copy decoded frame to destination buffer:
         * this is required since rawvideo expects non aligned data */
        av_image_copy(video_dst_data, video_dst_linesize,
                      (const uint8_t **)(frame->data), frame->linesize,
                      pCodecCtx_v->pix_fmt, pCodecCtx_v->width, pCodecCtx_v->height);

        /* write to rawvideo file */
 //       fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);

    }
    av_packet_unref(pkt);
    return ret;
}

int media_player::decode_audio()
{
    int ret, data_size;

    /* send the packet with the compressed data to the decoder */
    ret = avcodec_send_packet(pCodecCtx_a, pkt);
    if (ret < 0) {
        std::cout << "Error submitting the packet to the decoder\n";
        return -1;
    }

    /* read all the output frames (in general there may be any number of them */
    while (ret >= 0) {
        ret = avcodec_receive_frame(pCodecCtx_a, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            std::cout << "Error during decoding\n";
            return -1;
        }
        data_size = av_get_bytes_per_sample(pCodecCtx_a->sample_fmt);
        if (data_size < 0) {
            /* This should not occur, checking just for paranoia */
            std::cout << "Failed to calculate data size\n";
            return -1;
        }
        std::cout << "audio_frame n:%d nb_samples:%d pts:%s\n",
           audio_frame_count++, frame->nb_samples,
           av_ts2timestr(frame->pts, &pCodecCtx_a->time_base);
    }
    av_packet_unref(pkt);
    return 0;
}

int media_player::decode_func()
{
    int ret;
    AVCodecContext *pCodecCtx;

    ret = av_read_frame(ifmt_ctx,pkt);
    if (ret < 0)
        break;
    if(pkt->stream_index == stream_idx_v) {
        ret = decode_video();
        if(ret < 0) {
            std::cout << "decode_video error!!!\n";
            return ret;
        }
    }
    else if(pkt->stream_index == stream_idx_a) {
            return 0;       //audio在其他地方处理
    }
    return 0;
}

void media_player::sfp_refresh_thread(void *opaque){

	thread_exit=0;
	thread_pause=0;
    
	while (!thread_exit) {
		if(!thread_pause){
			event.type = SFM_REFRESH_EVENT;
			SDL_PushEvent(&event);
		}
		SDL_Delay(40);
	}
	thread_exit=0;
	thread_pause=0;
	//Break
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

}

int media_player::init_sdl()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        {
            std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
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
            std::cout << SDL_GetError() << std::endl;
            return -1;
        }
    
        //创建渲染器
        ren = NULL;
        ren = SDL_CreateRenderer(win, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == NULL)
        {
            std::cout << SDL_GetError() << std::endl;
            return -1;
        }

        //创建纹理
        tex = NULL;   
    
        video_tid = SDL_CreateThread(sfp_refresh_thread,NULL,NULL);
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
                    std::cout << ">>>===SDL_QUIT\n";
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
                    std::cout << ">>>===error event type...\n";
                    break;
            }
        }
}


