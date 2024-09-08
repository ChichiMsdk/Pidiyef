#include "event.h"
#include "pdf.h"
#include "gui.h"

#include <SDL2/SDL_render.h>

typedef struct Instance
{
	SDL_Renderer	*pRenderer;
	SDL_Window		*pWin;
	int				width;
	int				height;
	int				running;
}Instance;

extern Instance gInst;
extern PDFView gView;

#define MOVE 300.0f

inline static void
MoveRect(int key, PDFView *pView, SDL_FRect *pRect)
{
	switch (key)
	{
		case (SDLK_h):
			{
				pRect->x -= MOVE; 
				if (pRect->x <= (-1.0f * pRect->w))
					pRect->x = (pRect->w * -1.0f);
				break;
			}
		case (SDLK_l):
			{
				pRect->x += MOVE; 
				if (pRect->x >= (gInst.width + pRect->w))
					pRect->x = (gInst.width);
				break;
			}
		case (SDLK_j):
			{
				pRect->y += MOVE; 
				if (pRect->y >= (gInst.height))
					pRect->y = (gInst.height);
				break;
			}
		case (SDLK_k):
			{
				pRect->y -= MOVE; 
				if (pRect->y <= (-1.0f * pRect->h))
					pRect->y = (pRect->h * -1.0f);
				break;
			}
		case (SDLK_3):
				printf("w:%f\th:%f\n", pRect->w, pRect->h);
				break;
		case (SDLK_1):
			{
				pRect->w *= 1.1f;
				pRect->h *= 1.1f;
				if (pRect->w >= 16000.0f || pRect->h >= 16000.0f)
				{
					pRect->w = 8000.0f;
					pRect->h = 8000.0f;
				}
				printf("w:%f\th:%f\n", pRect->w, pRect->h);
				break;
			}
		case (SDLK_2):
			{
				pRect->w /= 1.1f;
				pRect->h /= 1.1f;
				if (pRect->w <= 0.5f || pRect->h <= 0.5f)
				{
					pRect->w = 8000.0f;
					pRect->h = 8000.0f;
				}
				printf("w:%f\th:%f\n", pRect->w, pRect->h);
				break;
			}
	}
	pView->oldView.x = pView->currentView.x;
	pView->oldView.y = pView->currentView.y;
	pView->oldView.w = pView->currentView.w;
	pView->oldView.h = pView->currentView.h;
}

inline static void
SmoothMoveRect(int key, PDFView *pView, float factor)
{
	MoveRect(key, pView, &(pView)->nextView);
}

void
Event(SDL_Event *e)
{
	SDL_PollEvent(e);
	if(e->type == SDL_QUIT) { gInst.running = 0; }
	if(e->type == SDL_KEYDOWN) 
	{
		switch (e->key.keysym.sym)
		{
			case (SDLK_ESCAPE):
				gInst.running = 0; 
				break;
			case (SDLK_h):
			case (SDLK_l):
			case (SDLK_j):
			case (SDLK_k):
			case (SDLK_1):
			case (SDLK_2):
			case (SDLK_3):
				MoveRect(e->key.keysym.sym, &gView, &gView.nextView);
				break;
		}
	}
}
