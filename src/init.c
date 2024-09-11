#include <stdio.h>

#include "init.h"

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
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
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
	InitPerfFreq();
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
}
