#include "stdio.h"
#include <SDL2/SDL.h>
#include "SDL2/SDL_image.h"

extern int main_soft();
extern int main_hard(const char *src);
extern int main_audio();
extern int main_hard_yuv(const char *src);
extern int main_audio_aac(const char *src,const char *des);
int main(int argc, char** argv)
{
#if 1 
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(0);
    }
#endif
//    main_soft();
//    main_hard(argv[1]);
//    main_audio();
//    main_hard(argv[1]);
//    main_hard_yuv(argv[1]);
    main_audio_aac(argv[1],argv[2]);


    return 0;
}

