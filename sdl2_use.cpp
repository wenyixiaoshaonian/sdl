#include "stdio.h"
#include <SDL2/SDL.h>
#include "SDL2/SDL_image.h"

extern int main_soft();
extern int main_hard(const char *src);
extern int main_audio();

int main(int argc, char** argv)
{
//    main_soft();
//    main_hard(argv[1]);
    main_audio();

    return 0;
}

