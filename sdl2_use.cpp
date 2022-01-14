#include "stdio.h"
#include <SDL2/SDL.h>

extern int main_soft();
extern int main_hard();

int main(int argc, char** argv)
{
    main_soft();
//    main_hard();
    return 0;
}

