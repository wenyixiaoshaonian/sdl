#include "media_player.h"


int main(int argc, char** argv)
{
    media_player player;
    int ret;
    const char *file;

    if(argc <2) {
        printf("Usage: %s <input media file>",argv[0]);
    }
    file = argv[1];

    ret = player.open_input_file(file);
    if(ret) {
        printf("open_input_file error...\n");
        return -1;
    }
    ret = player.open_codec_context(AVMEDIA_TYPE_VIDEO);
    if(ret) {
        printf("open_codec_context AVMEDIA_TYPE_VIDEO error...\n");
        return -1;
    }

    ret = player.open_codec_context(AVMEDIA_TYPE_AUDIO);
    if(ret) {
        printf("open_codec_context AVMEDIA_TYPE_AUDIO error...\n");
        return -1;
    }

    ret = player.alloc_image();
    if(ret) {
        printf("alloc_image error...\n");
        return -1;
    }
    ret = player.init_filters();
    if(ret) {
        printf("init_filters error...\n");
        return -1;
    }    
    ret = player.init_sdl();
    if(ret) {
        printf("init_sdl error...\n");
        return -1;
    }
    //开始处理事件
    player.recv_event();
    return 0;
}
