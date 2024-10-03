#include "event.h"
#include "engine/pdf.h"
#include "import/gui.h"
#include "import/camera.h"
#include "utils.h"
#include "init.h"

#include <SDL2/SDL_render.h>
#include "tracy/TracyC.h"

extern PDFView gView3;
extern int gRender;

float MOVE[] = {50.0f, 500.0f, 5000.0f};
int MMOVE = 0;

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

extern float gZoom;

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
	TracyCZoneNC(ch, "ChangePage", 0Xab8673b, 1)
	SDL_DestroyTexture(gInst.pMainTexture);
	gInst.pMainTexture = NULL;
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
}

int LoadPixMapFromThreads(PDFContext *pdf, fz_context *pCtx, const char *pFile, sInfo sInfo);
inline static void
MoveCanvas(int key, Canvas *canvas);

void
ResizeWindow(void)
{
	printf("gInst.width %d, gInst.height %d\t->", gInst.width, gInst.height);
	SDL_GetWindowSize(gInst.pWin, &gInst.width, &gInst.height);
	printf("gInst.width %d, gInst.height %d\n", gInst.width, gInst.height);
}

void
EventWindow(SDL_WindowEvent e)
{
	switch (e.event)
	{
		case (SDL_WINDOWEVENT_RESIZED):
		case (SDL_WINDOWEVENT_MAXIMIZED):
		case (SDL_WINDOWEVENT_EXPOSED):
		case (SDL_WINDOWEVENT_SIZE_CHANGED):
			ResizeWindow();
			break;
	}
}

void
Event(SDL_Event *e)
{
	while(SDL_PollEvent(e))
	{
		sInfo sInfo = {.nbrPages = 5, 3, 800, 0};
		if (e->type == SDL_QUIT) { gInst.running = false; }
		else if (e->type == SDL_WINDOWEVENT) EventWindow(e->window);
		else if (e->type == SDL_KEYDOWN) 
		{
			switch (e->key.keysym.sym)
			{
				case (SDLK_ESCAPE):
					gInst.running = false; 
					break;
				case (SDLK_h):
				case (SDLK_l):
				case (SDLK_j):
				case (SDLK_k):
				case (SDLK_1):
				case (SDLK_2):
				case (SDLK_SPACE):
					MoveCanvas(e->key.keysym.sym, &gCanvas);
					/* MoveRect(e->key.keysym.sym, &gView3, &gView3.nextView); */
					break;
				case (SDLK_RIGHT):
					/* ChangePage(NEXT_P); */
					break;
				case (SDLK_LEFT):
					/* ChangePage(BACK_P); */
					break;
				case (SDLK_3):
					/* LoadPixMapFromThreads(&gPdf, gPdf.pCtx, gPdf.pFile, sInfo); */
					break;
			}
		}
		else if (e->type == SDL_MOUSEWHEEL)
		{
			/*
			 * if (e->wheel.y > 0)
			 * 	ChangePage(BACK_P);
			 * else if (e->wheel.y < 0)
			 * 	ChangePage(NEXT_P);
			 */
		}
	}
}

inline static void
MoveCanvas(int key, Canvas *canvas)
{
	switch (key)
	{
		case (SDLK_SPACE):
			MMOVE++;
			if (MMOVE >= sizeof(MOVE) / sizeof(float))
				MMOVE = 0;
			break;
		case (SDLK_h):
			{
				canvas->x -= MOVE[MMOVE];
				if (canvas->x <= 0) canvas->x = 0;
				/* printf("canvas->x: %d\n", canvas->x); */
				break;
			}
		case (SDLK_l):
			{
				canvas->x += MOVE[MMOVE];
				if (canvas->x + canvas->w / 2 >= gInst.width) canvas->x -= MOVE[MMOVE];
				/* printf("canvas->x: %d\n", canvas->x); */
				break;
			}
		case (SDLK_j):
			{
				canvas->y += MOVE[MMOVE];
				if (canvas->y + canvas->w / 2 >= gInst.height) canvas->y -= MOVE[MMOVE];
				/* printf("canvas->y: %d\n", canvas->y); */
				break;
			}
		case (SDLK_k):
			{
				canvas->y -= MOVE[MMOVE];
				if (canvas->y <= (-1 * canvas->h)) canvas->y += MOVE[MMOVE];
				/* printf("canvas->y: %d\n", canvas->y); */
				break;
			}
		case (SDLK_1):
			{
				gScale /= 2.0f;
				if (gScale <= 0.1f)
					gScale = 0.1f;
				break;
			}
		case (SDLK_2):
			{
				gScale *= 2.0f;
				if (gScale >= 20.0f)
					gScale = 20.0f;
				break;
			}
		default:
			break;
	}
}

inline static void
MoveRect(int key, PDFView *pView, SDL_FRect *pRect)
{
	// TODO: Add reloading bool when texture's size changes !
	switch (key)
	{
		case (SDLK_SPACE):
			goto reloading;
		case (SDLK_h):
			{
				pRect->x -= MOVE[MMOVE]; 
				if (pRect->x <= (-1.0f * pRect->w))
					pRect->x = (pRect->w * -1.0f) + 1;
				goto nothing;
			}
		case (SDLK_l):
			{
				pRect->x += MOVE[MMOVE]; 
				if (pRect->x >= (gInst.width))
					pRect->x = (gInst.width - 100);
				goto nothing;
			}
		case (SDLK_j):
			{
				pRect->y += MOVE[MMOVE]; 
				if (pRect->y >= (gInst.height))
					pRect->y = (gInst.height) - 1;
				goto nothing;
			}
		case (SDLK_k):
			{
				pRect->y -= MOVE[MMOVE]; 
				if (pRect->y <= (-1.0f * pRect->h))
					pRect->y = (pRect->h * -1.0f) + 1;
				goto nothing;
			}
		case (SDLK_1):
			{
				gZoom /=2.0f;
				if (gZoom <= 0.12f)
					gZoom = 0.12f;

				gCurrentZoom--;
				if (gCurrentZoom < 0)
					gCurrentZoom = 0;
				printf("CurrentZoom[%d]: %f\n", gCurrentZoom, gpZoom[gCurrentZoom]);
				goto reloading;
			}
		case (SDLK_2):
			{
				gZoom *=2.0f;
				if (gZoom >= 20.0f)
					gZoom = 20.0f;

				gCurrentZoom++;
				if (gCurrentZoom >= gSizeZoomArray)
					gCurrentZoom = gSizeZoomArray - 1;
				printf("CurrentZoom[%d]: %f\n", gCurrentZoom, gpZoom[gCurrentZoom]);
				goto reloading;
			}
		default:
			goto nothing;
	}
reloading:
nothing:
	if (gPdf.pPages[gPdf.viewingPage].bPpmCache)
	{
		fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gPdf.viewingPage].pPix);
		gPdf.pPages[gPdf.viewingPage].pPix = NULL;
		gPdf.pPages[gPdf.viewingPage].bPpmCache = false;
		gPdf.pPages[gPdf.viewingPage].bTextureCache = false;
		SDL_DestroyTexture(gInst.pMainTexture);
		gInst.pMainTexture = NULL;
	}
	ReloadPage();
	return ;
    /*
	 * pView->oldView.x = pView->currentView.x;
	 * pView->oldView.y = pView->currentView.y;
	 * pView->oldView.w = pView->currentView.w;
	 * pView->oldView.h = pView->currentView.h;
     */
}

float timing = 0.1f;
int num = 0;
int mode = 0;

void
event(SDL_Event *es)
{
	size_t jump = 0;
	size_t oldJump = 0;
	SDL_Event e = *es;
	SDL_PollEvent(es); 
	const uint8_t *arr = SDL_GetKeyboardState(&num);

	if (e.type == SDL_QUIT) {gInst.running = false;}
	// reset the page's positions
	else if (e.type == SDL_WINDOWEVENT_RESIZED){ SDL_GetWindowSize(gInst.pWin, &gInst.width, &gInst.height); printf("%d\t%d\n", gInst.width, gInst.height); }
	else if (e.type == SDL_WINDOWEVENT_SIZE_CHANGED) { SDL_GetWindowSize(gInst.pWin, &gInst.width, &gInst.height); printf("%d\t%d\n", gInst.width, gInst.height); }
	else if (e.type == SDL_KEYDOWN)
	{ 
		if (e.key.keysym.sym == SDLK_ESCAPE) { gInst.running = false;}

		else if (arr[SDL_SCANCODE_LCTRL] && arr[SDL_SCANCODE_O]) { int tmp = gPdf.viewingPage; gPdf.viewingPage = oldJump; oldJump = tmp; }
		else if (arr[SDL_SCANCODE_LSHIFT] && arr[SDL_SCANCODE_K]) { fscalex /= 1.1f; if (gView.w <= 0.1f || gView.h <= 0.1f) gView.w = gInst.width; gView.h = gInst.height; }
		else if (arr[SDL_SCANCODE_LSHIFT] && arr[SDL_SCANCODE_J]) { fscalex *= 1.1f; if (gView.w >= 100000.0f || gView.h <= 100000.0f) gView.w = gInst.width; gView.h = gInst.height; }
		 
		else if (e.key.keysym.sym == SDLK_k) { gView.y += (300 / fscalex); if (gView.y >= 4000000000) gView.y = 0.0f;}
		else if (e.key.keysym.sym == SDLK_j) { gView.y -= (300 / fscalex); if (gView.y < 0.0f) gView.y = 0.0f;}
		else if (e.key.keysym.sym == SDLK_h) { gView.x += (100 / fscalex);}
		else if (e.key.keysym.sym == SDLK_l) { gView.x -= (100 / fscalex);}

		else if (e.key.keysym.sym == SDLK_UP) { }
		else if (e.key.keysym.sym == SDLK_DOWN) { }
		else if (e.key.keysym.sym == SDLK_SPACE) { }

		else if (e.key.keysym.sym == SDLK_LEFT) 
		{ 
			gCurrentZoom--;
			if (gCurrentZoom < 0)
				gCurrentZoom = 0;
			printf("CurrentZoom[%d]: %f\n", gCurrentZoom, gpZoom[gCurrentZoom]);
			/* gZoom /= 1.2f; if (gZoom <= 0.1f) {gZoom = 0.1f;} resize(); printf("gZoom: %f\n", gZoom); */
			/* resize(); */
		}
		else if (e.key.keysym.sym == SDLK_RIGHT)
		{ 
			gCurrentZoom++;
			if (gCurrentZoom >= gSizeZoomArray)
				gCurrentZoom = gSizeZoomArray - 1;
			printf("CurrentZoom[%d]: %f\n", gCurrentZoom, gpZoom[gCurrentZoom]);
			/* gZoom *= 1.2f; if (gZoom > 50.0f) {gZoom = 50.0f;} resize(); printf("gZoom: %f\n", gZoom); */
			/* resize(); */
		}

		else if (e.key.keysym.sym == SDLK_SEMICOLON && e.key.keysym.sym == SDLK_LSHIFT) { mode = 2; }
		switch (e.key.keysym.mod) { case (true): if (e.key.keysym.sym == SDLK_SEMICOLON) { mode = 2; } }
		if (mode == 2)
		{
			switch (e.key.keysym.sym)
			{
				case (SDLK_0): jump = jump * 10 + 0; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_1): jump = jump * 10 + 1; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_2): jump = jump * 10 + 2; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_3): jump = jump * 10 + 3; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_4): jump = jump * 10 + 4; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_5): jump = jump * 10 + 5; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_6): jump = jump * 10 + 6; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_7): jump = jump * 10 + 7; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_8): jump = jump * 10 + 8; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_9): jump = jump * 10 + 9; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_RETURN):
							   mode = 0;
							   printf("\rjumping to: %zu\n", jump);
							   if (jump < gPdf.nbOfPages)
							   {
								   oldJump = gPdf.viewingPage;
								   gPdf.viewingPage = jump;
							   }
							   jump = 0;
							   break;
			}
		}
	}
	else if (e.type == SDL_MOUSEWHEEL)
	{
		// the max nbpage * height + gap
        /*
		 * if (e.wheel.y > 0) { gview.y -= 50; if (gview.y < 0.0f) gview.y = 0.0f;}
		 * else if (e.wheel.y < 0) { gview.y += 50; if (gview.y > 4000000000.0f) gview.y = 0.0f;} 
         */
		zoom_world(e.wheel);
		/* resize(); */
	}
}
