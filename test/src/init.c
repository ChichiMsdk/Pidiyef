#include <stdio.h>

#include "init.h"
#include "containers.h"

extern Instance gInst;
uint64_t GetNbProc(void);

void
Init(int ac, char **av, Instance *inst)
{
    if(SDL_Init(SDL_INIT_VIDEO) < 0) 
	{
        printf("SDLfailed!\nSDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
	SDL_Window *window = SDL_CreateWindow( "Pdf Viewer",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			gInst.width , gInst.height,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
	if (!window)
	{
		fprintf(stderr, "Window failed\nSDL_Error: %s\n", SDL_GetError()); 
		exit(1);
	}
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
	{
		fprintf(stderr, "Renderer failed!\nSDL_Error: %s\n", SDL_GetError());
		exit(1);
	}
	if(SDL_RenderSetVSync(renderer, 1))
	{
		fprintf(stderr, "Vsync failed!\nSDL_Error: %s\n", SDL_GetError());
	}
	gInst.pMutexes = malloc(sizeof(Mutex) * MAX_MUTEX);
	for (int i = 0; i < MAX_MUTEX; i++)
	{
		if (myCreateMutex(&gInst.pMutexes[i]) != 0)
		{
			fprintf(stderr, "Could not create mutex in Init()\n");
			exit(1);
		}
	}
	gInst.pRenderer = renderer;
	gInst.pWin = window;
	gInst.nbThreads = GetNbProc();
	InitPerfFreq();
	if (!InitQueue(&gEventQueue, MY_MAX_EVENTS))
	{
		fprintf(stderr, "Couldn not create EventQueue\n");
		exit(1);
	}
}
