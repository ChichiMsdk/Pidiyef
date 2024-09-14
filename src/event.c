#include "event.h"
#include "engine/pdf.h"
#include "gui.h"
#include "init.h"

#include <SDL2/SDL_render.h>
#include "tracy/TracyC.h"

extern PDFView gView;
extern int gRender;

#define MOVE 300.0f

inline static void
MoveRect(int key, PDFView *pView, SDL_FRect *pRect);

inline static void
SmoothMoveRect(int key, PDFView *pView, float factor)
{
	MoveRect(key, pView, &(pView)->nextView);
}

SDL_Texture* PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, SDL_Texture *pTexture);
fz_pixmap *CreatePDFPage(fz_context *pCtx, const char *pFile, sInfo *sInfo);
SDL_Texture* 
LoadTextures(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, int textureFormat);
void ReloadPage(void);

float gZoom = 1.0f;

/*
 * 
 * NOTE: 400 ms measured in debug mode for CreatePDFPage (Win11-i9-12900k-RTX3080)
 * 
 * TODO: Make it a "circular" buffer, meaning we cache a little before
 * and a little after !
 */
double timeToPoll2 = 99.0; //ms
void
ChangePage(DIRECTION direction)
{
	TracyCZoneNC(ch, "NextPage", 0XFF00FF, 1)
	if (direction == NEXT_P)
	{
		int old = gPdf.viewingPage;
		gPdf.viewingPage++;
		if (gPdf.viewingPage >= gPdf.nbOfPages)
			gPdf.viewingPage = 0;

		if (gPdf.pPages[old].bPpmCache == true)
		{
			fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[old].pPix);
			gPdf.pPages[old].pPix = NULL;
			gPdf.pPages[old].bPpmCache = false;
			gPdf.pPages[old].bTextureCache = false;
		}
	}
	else if(direction == BACK_P)
	{
		int old = gPdf.viewingPage;
		if (gPdf.viewingPage == 0)
		{
			int tmp = gPdf.nbOfPages - 1;
			if (tmp >= 0)
				gPdf.viewingPage = gPdf.nbOfPages - 1;
			else
				gPdf.viewingPage = 0;
		}
		else
		{
			gPdf.viewingPage--;
		}
		int i = gPdf.viewingPage; 
		if (gPdf.pPages[old].bPpmCache == true && old != i)
		{
			fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[old].pPix);
			gPdf.pPages[old].pPix = NULL;
			gPdf.pPages[old].bPpmCache = false;
			gPdf.pPages[old].bTextureCache = false;
		}
	}
	ReloadPage();
	TracyCZoneEnd(ch);
}

void
ReloadPage(void)
{
	int i = gPdf.viewingPage;
	/* printf("page requested is : %d/%zu\n", i, gPdf.nbOfPages); */
	sInfo sInfo = {
		.fDpi = 100 * gZoom,
		.fZoom = 100,
		.fRotate = 0,
		.nbrPages = 1,
		.pageStart = i
	};

	if (!gPdf.pPages[i].bPpmCache)
	{
		gPdf.pPages[i].pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
		gPdf.pPages[i].bPpmCache = true;
	}
	if (!gInst.pMainTexture)
	{
		gInst.pMainTexture = LoadTextures(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, SDL_TEXTUREACCESS_STREAMING);
		gPdf.pPages[i].bTextureCache = true;
	}
	gInst.pMainTexture = PixmapToTexture(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, gInst.pMainTexture);
	if (!gInst.pMainTexture)
		exit(1);
	gPdf.pPages[i].bTextureCache = true;
	gRender = true;
}

int LoadPixMapFromThreads(PDFContext *pdf, fz_context *pCtx, const char *pFile, sInfo sInfo);

void
Event(SDL_Event *e)
{
	SDL_PollEvent(e);
	sInfo sInfo = {.nbrPages = 5, 3, 800, 0};
	if (e->type == SDL_QUIT) { gInst.running = false; }
	if (e->type == SDL_WINDOWEVENT_ENTER || e->type == SDL_WINDOWEVENT_LEAVE)
	{
		gRender = true;
	}
	else if (e->type == SDL_KEYDOWN) 
	{
		switch (e->key.keysym.sym)
		{
			case (SDLK_ESCAPE):
				gInst.running = false; 
				printf("running is %d\n", gInst.running); 
				break;
			case (SDLK_h):
			case (SDLK_l):
			case (SDLK_j):
			case (SDLK_k):
			case (SDLK_1):
			case (SDLK_2):
				MoveRect(e->key.keysym.sym, &gView, &gView.nextView);
				break;
			case (SDLK_RIGHT):
				ChangePage(NEXT_P);
				break;
			case (SDLK_LEFT):
				ChangePage(BACK_P);
				break;
			case (SDLK_3):
				LoadPixMapFromThreads(&gPdf, gPdf.pCtx, gPdf.pFile, sInfo);
				break;
		}
	}
	else if (e->type == SDL_MOUSEWHEEL)
	{
		if (e->wheel.y > 0)
			ChangePage(BACK_P);
		else if (e->wheel.y < 0)
			ChangePage(NEXT_P);
	}
}

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
		case (SDLK_1):
			{
				gZoom /=2.0f;
				if (gZoom <= 0.12f)
					gZoom = 0.12f;
				if (gPdf.pPages[gPdf.viewingPage].bPpmCache)
				{
					fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gPdf.viewingPage].pPix);
					gPdf.pPages[gPdf.viewingPage].bPpmCache = false;
					gPdf.pPages[gPdf.viewingPage].bTextureCache = false;
					SDL_DestroyTexture(gInst.pMainTexture);
					gInst.pMainTexture = NULL;
				}
				pRect->x = gInst.width / 4.0f;
				gRender = true;
				ReloadPage();
                /*
				 * pRect->w *= 1.1f;
				 * pRect->h *= 1.1f;
				 * if (pRect->w >= 16000.0f || pRect->h >= 16000.0f)
				 * {
				 * 	pRect->w = 8000.0f;
				 * 	pRect->h = 8000.0f;
				 * }
                 */
				/* printf("w:%f\th:%f\n", pRect->w, pRect->h); */
				break;
			}
		case (SDLK_2):
			{
				gZoom *=2.0f;
				if (gZoom >= 20.0f)
					gZoom = 20.0f;
				if (gPdf.pPages[gPdf.viewingPage].bPpmCache)
				{
					fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gPdf.viewingPage].pPix);
					gPdf.pPages[gPdf.viewingPage].bPpmCache = false;
					gPdf.pPages[gPdf.viewingPage].bTextureCache = false;
					SDL_DestroyTexture(gInst.pMainTexture);
					gInst.pMainTexture = NULL;
				}
				gRender = true;
				ReloadPage();
				pRect->x = gInst.width / 4.0f;
                /*
				 * pRect->w /= 1.1f;
				 * pRect->h /= 1.1f;
				 * if (pRect->w <= 0.5f || pRect->h <= 0.5f)
				 * {
				 * 	pRect->w = 8000.0f;
				 * 	pRect->h = 8000.0f;
				 * }
                 */
				/* printf("w:%f\th:%f\n", pRect->w, pRect->h); */
				break;
			}
	}
	pView->oldView.x = pView->currentView.x;
	pView->oldView.y = pView->currentView.y;
	pView->oldView.w = pView->currentView.w;
	pView->oldView.h = pView->currentView.h;
}
