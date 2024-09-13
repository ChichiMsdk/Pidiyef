#ifndef INIT_H
#define INIT_H

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "event.h"
#include "platform/os_threads.h"

typedef struct Instance
{
	SDL_Renderer	*pRenderer;
	SDL_Window		*pWin;
	SDL_Texture		*pLoadingTexture;
	SDL_Texture		*pMainTexture;
	int				width;
	int				height;
	uint64_t		nbThreads;
	Mutex			*pMutexes;
	int				running;
	uint64_t		lastPoll;
}Instance;

extern Instance gInst;

void	Init(int argc, char **ppArgv, Instance *pInst);

#endif //INIT_H
