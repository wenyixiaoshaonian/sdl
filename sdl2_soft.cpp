#include "stdio.h"
#include <SDL2/SDL.h>
#include "SDL2/SDL_image.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
//按键表面常量
enum KeyPressSurfaces
{
    KEY_PRESS_SURFACE_DEFAULT,
    KEY_PRESS_SURFACE_UP,
    KEY_PRESS_SURFACE_DOWN,
    KEY_PRESS_SURFACE_LEFT,
    KEY_PRESS_SURFACE_RIGHT,
    KEY_PRESS_SURFACE_TOTAL
};

//我们要渲染的窗口
SDL_Window* gWindow = NULL;
//窗口所包含的表面
SDL_Surface* gScreenSurface = NULL;
//按键对应的图片
SDL_Surface* gKeyPressSurfaces[KEY_PRESS_SURFACE_TOTAL];
//当前显示的图像
SDL_Surface* gCurrentSurface = NULL;


SDL_Surface* loadSurface(const char *s)
{
    //在指定路径加载图像
    SDL_Surface* loadedSurface = IMG_Load(s);
    if( loadedSurface == NULL )
    {
        printf( "Unable to load image %s! SDL Error: %s\n", s, SDL_GetError() );
    }

    return loadedSurface;
}
bool loadMedia()
{
    //加载成功标志
    bool success = true;

    //加载默认表面
    gKeyPressSurfaces[ KEY_PRESS_SURFACE_DEFAULT ] = loadSurface( "press.jpg" );
    if( gKeyPressSurfaces[ KEY_PRESS_SURFACE_DEFAULT ] == NULL )
    {
        printf( "Failed to load default image!\n" );
        success = false;
    }

    //加载up对应的表面
    gKeyPressSurfaces[ KEY_PRESS_SURFACE_UP ] = loadSurface( "up.jpg" );
    if( gKeyPressSurfaces[ KEY_PRESS_SURFACE_UP ] == NULL )
    {
        printf( "Failed to load up image!\n" );
        success = false;
    }

    //加载down对应的表面
    gKeyPressSurfaces[ KEY_PRESS_SURFACE_DOWN ] = loadSurface( "down.jpg" );
    if( gKeyPressSurfaces[ KEY_PRESS_SURFACE_DOWN ] == NULL )
    {
        printf( "Failed to load down image!\n" );
        success = false;
    }

    //加载left对应的表面
    gKeyPressSurfaces[ KEY_PRESS_SURFACE_LEFT ] = loadSurface( "left.jpg" );
    if( gKeyPressSurfaces[ KEY_PRESS_SURFACE_LEFT ] == NULL )
    {
        printf( "Failed to load left image!\n" );
        success = false;
    }

    //加载right对应的表面
    gKeyPressSurfaces[ KEY_PRESS_SURFACE_RIGHT ] = loadSurface( "right.jpg" );
    if( gKeyPressSurfaces[ KEY_PRESS_SURFACE_RIGHT ] == NULL )
    {
        printf( "Failed to load right image!\n" );
        success = false;
    }

    return success;
}

int main_soft()
{
    //Main loop flag
    bool quit = false;
    //Event handler
    SDL_Event e;
    int ret = 0;
    SDL_Surface* optimizedSurface = NULL;   //优化拉伸过的表面
    SDL_Rect stretchRect;

    // 初始化SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }
    ret = IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    if(ret < 0)
    {
        printf( "IMG_Init could not initialize! SDL_Error: %s\n", SDL_GetError() );
        return -1;
    }    
    printf("init success....\n");
    loadMedia();
    
    IMG_Quit();

    stretchRect.x = 0;
    stretchRect.y = 0;
    stretchRect.w = SCREEN_WIDTH;
    stretchRect.h = SCREEN_HEIGHT;

    // 创建窗口
    gWindow = SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
    if( gWindow == NULL )
    {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
    }
    else
    {
        //设置默认的当前表面
        gCurrentSurface = gKeyPressSurfaces[ KEY_PRESS_SURFACE_DEFAULT ];
        // 获取窗口表面
        gScreenSurface = SDL_GetWindowSurface( gWindow );
        while( !quit )
        {
            SDL_WaitEvent(&e);
    		//用户请求退出
    		if( e.type == SDL_QUIT )
    		{
                printf(">>>===SDL_QUIT\n");
    			quit = true;
    		}
            else if( e.type == SDL_KEYDOWN )
            {
                printf(">>>===SDL_KEYDOWN SDLK_UP = %d   value = %d\n",SDLK_UP,e.key.keysym.sym);
                //根据按键选择表面
                switch( e.key.keysym.sym )
                {
                    case SDLK_UP:
                    gCurrentSurface = gKeyPressSurfaces[ KEY_PRESS_SURFACE_UP ];
                    break;

                    case SDLK_DOWN:
                    gCurrentSurface = gKeyPressSurfaces[ KEY_PRESS_SURFACE_DOWN ];
                    break;

                    case SDLK_LEFT:
                    gCurrentSurface = gKeyPressSurfaces[ KEY_PRESS_SURFACE_LEFT ];
                    break;

                    case SDLK_RIGHT:
                    gCurrentSurface = gKeyPressSurfaces[ KEY_PRESS_SURFACE_RIGHT ];
                    break;

                    default:
                    gCurrentSurface = gKeyPressSurfaces[ KEY_PRESS_SURFACE_DEFAULT ];
                    break;
                }
            }
//            optimizedSurface = SDL_ConvertSurface( gCurrentSurface, gScreenSurface->format, 0 );    //修改格式
            SDL_BlitScaled( gCurrentSurface, NULL, gScreenSurface, &stretchRect );  //拉伸图片
            //应用图片
//    	    SDL_BlitSurface( optimizedSurface, NULL, gScreenSurface, NULL );
            // 更新表面
            SDL_UpdateWindowSurface(gWindow);
        }

    }

	// 销毁窗户
	SDL_DestroyWindow( gWindow );
	// 退出SDL子系统
	SDL_Quit();
	return 0;
}
