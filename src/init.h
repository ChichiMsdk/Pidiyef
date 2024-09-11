#ifndef INIT_H
#define INIT_H

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "event.h"

typedef struct Instance
{
	SDL_Renderer	*pRenderer;
	SDL_Window		*pWin;
	int				width;
	int				height;
	int				running;
	int				nbThreads;
}Instance;

void	Init(int argc, char **ppArgv, Instance *pInst);

#endif //INIT_H
