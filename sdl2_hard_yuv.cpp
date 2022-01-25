#include <iostream>

#include <SDL2/SDL.h>

#include "SDL2/SDL_image.h"



int main_hard_yuv(const char *src)
{
    const char *infile;
	int width = 960;//yuv视频分辨率，宽度，注意此处一定要填写正确的.yuv文件的分辨率宽度
	int height = 544;//yuv视频分辨率，高度，注意此处一定要填写正确的.yuv文件的分辨率高度
	int size = width * height * 3 / 2;//这是每帧画面的字节数量
    Uint32 pixformat = SDL_PIXELFORMAT_IYUV;//该文件的格式
    unsigned char* buffer = new unsigned char[size];//每帧存储缓存
    SDL_Rect rect = { 0, 0, width, height };    //确定位置
    FILE *video_dst_file = NULL;
    SDL_Event event;

    infile        = src;

    video_dst_file = fopen(infile, "rb");
    if (!video_dst_file) {
        fprintf(stderr, "Could not open %s\n", infile);
        exit(1);
    }

    //初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {

        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);


    //创建窗口
    SDL_Window *win = nullptr;
    win = SDL_CreateWindow("Hello SDL2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, SDL_WINDOW_SHOWN);
    if (win == nullptr)
    {
        std::cout << SDL_GetError() << std::endl;
        return 1;
    }
    SDL_SetWindowOpacity(win, 1);//设置透明度

    //创建渲染器
    SDL_Renderer *ren = nullptr;
    ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (ren == nullptr)
    {
        std::cout << SDL_GetError() << std::endl;
        return 1;
    }

    //创建材质
    SDL_Texture *tex = nullptr;
    tex = SDL_CreateTexture(ren, pixformat, SDL_TEXTUREACCESS_STREAMING, width, height);//创建对应纹理
    bool quit = false;
    while (quit == false)
    {
#if 0
        SDL_WaitEvent(&event);
		//用户请求退出
		if( event.type == SDL_QUIT )
		{
            printf(">>>===SDL_QUIT\n");
			quit = true;
		}
#endif  
        if(fread(buffer,1,size,video_dst_file) <=0)
        {
            printf("Could not read input file.\n");
            return -1;
        }
        //填充材质
        SDL_UpdateTexture(tex,NULL,buffer,width);
        //清空渲染器
        SDL_RenderClear(ren);
        //将材质复制到渲染器
        SDL_RenderCopy(ren, tex, NULL, NULL);
        //呈现渲染器
        SDL_RenderPresent(ren);
        SDL_Delay(1000 / 30);
    }


    //释放资源
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    //退出SDL
    SDL_Quit();

  }


