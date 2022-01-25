#include <iostream>
#include <SDL2/SDL.h>
#include "SDL2/SDL_image.h"


static Uint8 *pAudio_chunk;
static Uint32 audio_len;
static Uint8 *pAudio_pos;

void fill_audio_buffer(void *userdata, Uint8 * stream, int len)
{
	SDL_memset(stream, 0, len);
	// 判断是否有读到数据
	if (audio_len == 0)
		return;

	len = (len > audio_len ? audio_len : len);
	SDL_MixAudio(stream, pAudio_pos, len, SDL_MIX_MAXVOLUME);
	pAudio_pos += len;
	audio_len -= len;
}


int main_audio()
{
    SDL_AudioSpec audioSpec;
    
    //初始化SDL
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    {
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    //初始化音频设备结构体
    // 音频数据的采样率。常用的有48000，44100等
    audioSpec.freq = 48000; 
    
    // 音频数据的格式
    audioSpec.format = AUDIO_F32SYS;
    
    // 声道数。例如单声道取值为1，立体声取值为2
    audioSpec.channels = 2;
    
    // 设置静音的值
    audioSpec.silence = 0;

    // 音频缓冲区中的采样个数，要求必须是2的n次方  AAC为1024
    audioSpec.samples = 1024;

    // 填充音频缓冲区的回调函数
    audioSpec.callback = fill_audio_buffer;

    // 打开音频设备
    if (SDL_OpenAudio(&audioSpec, NULL) < 0)
    {
        printf("Can not open audio!");
        return -1;
    }

    FILE *pAudioFile = fopen("audio.pcm", "rb+");
    if (pAudioFile == NULL)
    {
        printf("Can not open audio file!");
        return -1;
    }

//    int pcm_buffer_size = 8192;  
    int pcm_buffer_size = audioSpec.samples * audioSpec.channels * audioSpec.format;    
    char *pcm_buffer = (char *)malloc(pcm_buffer_size);
    int data_count = 0;

    SDL_PauseAudio(0);

    while(!feof(pAudioFile))
    {
        // 循环播放
        if (fread(pcm_buffer, 1, pcm_buffer_size, pAudioFile) <= 0)
        {
//            fseek(pAudioFile, 0, SEEK_SET);
//            fread(pcm_buffer, 1, pcm_buffer_size, pAudioFile);
//            data_count = 0;
        break;
        }
        printf("Playing %10d Bytes data.\n", data_count);
        data_count += pcm_buffer_size;

        pAudio_chunk = (Uint8 *)pcm_buffer;
        audio_len = pcm_buffer_size;
        pAudio_pos = pAudio_chunk;
        
        while (audio_len > 0)
            SDL_Delay(1);
    }
    free(pcm_buffer);
    SDL_Quit();
    
    return 0;
}

