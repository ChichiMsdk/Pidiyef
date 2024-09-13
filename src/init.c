#include <stdio.h>

#include "init.h"
#include "containers.h"

extern Instance gInst;
uint64_t GetNbProc(void);

void
Init(int ac, char **av, Instance *inst)
{
	if (ac < 3)
	{
		fprintf(stderr, "usage: example input-file page-number [ zoom [ rotate ] ]\n");
		fprintf(stderr, "\tinput-file: path of PDF, XPS, CBZ or EPUB document to open\n");
		fprintf(stderr, "\tPage numbering starts from one.\n");
		fprintf(stderr, "\tZoom level is in percent (100 percent is 72 dpi).\n");
		fprintf(stderr, "\tRotation is in degrees clockwise.\n");
		return;
	}

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

	gView.currentView.w = (float)1000 / 2;
	gView.currentView.h = (float)700 / 2;

	gView.currentView.x = gInst.width / 2.0f - gView.currentView.w / 2.0f;
	gView.currentView.y = gInst.height / 2.0f - gView.currentView.h / 2.0f;

	gView.nextView.x = gView.currentView.x; gView.nextView.y = gView.currentView.y;
	gView.nextView.w = gView.currentView.w; gView.nextView.h = gView.currentView.h;

	gView.oldView.x = gView.currentView.x; gView.oldView.y = gView.currentView.y;
	gView.oldView.w = gView.currentView.w; gView.oldView.h = gView.currentView.h;
}
