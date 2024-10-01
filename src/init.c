#include <stdio.h>

#include "init.h"
#include "import/containers.h"

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
	// Either VSYNC or Vulkan causes problems
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
	if(SDL_RenderSetVSync(renderer, 0))
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
	/* bbox.x1 595.280029      bbox.y1 841.890015 */
	gView3.currentView.w = 595.280029;
	gView3.currentView.h = 841.890015;

	gView3.currentView.x = gInst.width / 2.0f - gView3.currentView.w / 2.0f;
	gView3.currentView.y = gInst.height / 2.0f - gView3.currentView.h / 2.0f;

	gView3.nextView.x = gView3.currentView.x; gView3.nextView.y = gView3.currentView.y;
	gView3.nextView.w = gView3.currentView.w; gView3.nextView.h = gView3.currentView.h;

	gView3.oldView.x = gView3.currentView.x; gView3.oldView.y = gView3.currentView.y;
	gView3.oldView.w = gView3.currentView.w; gView3.oldView.h = gView3.currentView.h;

	gInst.pTextureMap = malloc(sizeof(TextureMap) * 20);
	for (int i = 0; i < 20; i++)
	{
		gInst.pTextureMap[i].pTexture = NULL;
		gInst.pTextureMap[i].pageIndex = -1;
	}
}
