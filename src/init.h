#ifndef INIT_H
#define INIT_H

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "event.h"
#include "platform/os_threads.h"

typedef struct TextureMap
{
	SDL_Texture	*pTexture;
	int			pageIndex;
}TextureMap;

typedef struct Instance
{
	SDL_Renderer	*pRenderer;
	SDL_Window		*pWin;
	SDL_Texture		*pLoadingTexture;
	SDL_Texture		*pMainTexture;
	TextureMap		*pTextureMap;
	int				width;
	int				heigth;
	uint64_t		nbThreads;
	Mutex			*pMutexes;
	int				running;
	uint64_t		lastPoll;
	myThread		*pThreads;

}Instance;

typedef struct Canvas
{
	int x, y;
	int w, h;
	SDL_Color color;
}Canvas;

extern float gZoom;
extern int gRender;
extern Canvas gCanvas;
extern float gScale;
extern Instance gInst;
extern SDL_FRect gView;

void	Init(int argc, char **ppArgv, Instance *pInst);

#endif //INIT_H
