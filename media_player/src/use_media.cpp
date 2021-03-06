#include"media_player.h"



int main(int argc, char *argv[])
{
    cmedia_player player;
    int ret;
    const char *file;

    if(argc <2) {
        printf("Usage: %s <input media file>",argv[0]);
    }
    file = argv[1];

    player.open_input_file(file);
    if(ret) {
        printf("open_input_file error...\n");
        return -1;
    }
    player.open_codec_context(AVMEDIA_TYPE_VIDEO);
    if(ret) {
        printf("open_codec_context AVMEDIA_TYPE_VIDEO error...\n");
        return -1;
    }

    player.open_codec_context(AVMEDIA_TYPE_AUDIO);
    if(ret) {
        printf("open_codec_context AVMEDIA_TYPE_AUDIO error...\n");
        return -1;
    }

    player.alloc_image();
    if(ret) {
        printf("alloc_image error...\n");
        return -1;
    }

    //初始化SDL、定时器 创建复分解线程
    player.init_sdl();
    if(ret) {
        printf("init_sdl error...\n");
        return -1;
    }
    //创建复分解线程
    player.creat_demux_thread();

    //开始处理事件
    player.recv_event();
    return 0;
}

