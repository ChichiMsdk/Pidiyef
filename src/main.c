#include "init.h"
#include "engine/pdf.h"
#include "gui.h"
#include "platform/os_threads.h"
#include "containers.h"

#include <tracy/TracyC.h>

#include <stdio.h>
#include <stdbool.h>

/*
 * -	Initialize the mudpf context.
 * 
 * -	Allocate an array of PDFPage of size of total number of pages.
 * 
 * -	Take 20 pages following the first one requested.
 * 		Spawn as many threads as needed to request the pages, dividing
 * 		the job equally between them.
 *
 * -	Add mutexes to signal the availability of a texture.
 * 
 * -	Only thread allowed to destroy texture is the main one.	
 */

/*
 * TODO:
 * - Don't forget to free mutexes array and destroy them ! (OS dependant)
 * - Separate main rendering thread (SDL) from "engineMUPDF" one.
 * - TimeOut least susceptible used page to free memory
 * - Vim motions: ":" + "cmd"
 * - Refactor the main and the way things set themselves up (separate muPDF ?)
 * - SDL_QueryTexture() when we create a new one to udpate gView
 * - Wrap modifying globals with mutexes i.e: ToggleState(some global)
 */

/*	FEATURES
 * - Bookmark a page to put it on the left (in scratch form so we can draw?)
 */
static inline void UpdateSmooth(float factor);

extern int gRender;
extern Instance gInst;
Instance gInst = {.running = true, .width = 1000, .height = 700, .pWin = NULL, .pMutexes = NULL};
int gRender = false;
EventQueue gEventQueue = {0};
PDFView gView = {0};
PDFContext gPdf = {0};

#define MAX_TEXTURE_ARRAY_SIZE 100

typedef struct TextureArray
{
	SDL_Texture *ppTexture[MAX_TEXTURE_ARRAY_SIZE];
	size_t first;
	size_t last;
	size_t currentSize;
}TextureArray;

SDL_Texture* 
LoadTextures(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, int textureFormat);
void ReloadPage(void);
fz_pixmap * CreatePDFPage(fz_context *pCtx, const char *pFile, sInfo *sInfo);

int
main(int Argc, char **ppArgv)
{
	Init(Argc, ppArgv, &gInst);

	sInfo sInfo = {
		.pageStart = Argc > 2 ? atoi(ppArgv[2]) - 1 : 0,
		.fZoom = Argc > 3 ? atof(ppArgv[3]) : 100,
		.fRotate = Argc > 4 ? atof(ppArgv[4]) : 0
	};

    /*
	 * NOTE: This should load the context based on the page that was last open
	 * or on the first page if first time open
     */
	CreatePDFContext(&gPdf, ppArgv[1], sInfo);

	SDL_Event e;
	SDL_FRect check = {-1, -1, -1, -1};

	int err = 0;
	float factor = 0.4f;
	gInst.lastPoll = GetCounter();

	// Load the very first page
	/* ReloadPage(); */
	struct TextureArray ta = {0};
	// Loads the viewed textures
	for (int i = 0; i < 3; i++)
	{
		sInfo.pageStart = gPdf.viewingPage + i;
		sInfo.nbrPages = 1;
		sInfo.fZoom = 100;
		sInfo.fDpi = 100;
		sInfo.fRotate = 0;

		gPdf.pPages[gPdf.viewingPage + i].pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
		gPdf.pPages[i].bPpmCache = true;
		ta.ppTexture[i] = 
			LoadTextures(gInst.pRenderer, gPdf.pPages[gPdf.viewingPage + i].pPix, gPdf.pCtx, SDL_TEXTUREACCESS_STREAMING);
		ta.ppTexture[i] = 
			PixmapToTexture(gInst.pRenderer, gPdf.pPages[gPdf.viewingPage + i].pPix, gPdf.pCtx, ta.ppTexture[i]);
		ta.currentSize++;
	}
	int mode = 0;
	int z = 0;
	int old = 0;
	uint64_t startTime = GetCounter();
	uint64_t timer = GetCounter();
	int count = 0;
	uint64_t jump = 0;
	float max = 0;
	int incr = -900;
	bool stop = false;
	double timing = 1;
	while(gInst.running)
	{
		SDL_PollEvent(&e); 
		if (e.type == SDL_QUIT) exit(1); 
		else if (e.type == SDL_KEYDOWN)
		{ 
			if (e.key.keysym.sym == SDLK_ESCAPE) { exit(1);}
			else if (e.key.keysym.sym == SDLK_UP) { incr = 900; printf("Down\n");}
			else if (e.key.keysym.sym == SDLK_DOWN) { incr = -900; printf("Up\n");}
			else if (e.key.keysym.sym == SDLK_SPACE) { stop = !stop; printf("stop is %d\n", stop);}
			else if (e.key.keysym.sym == SDLK_RIGHT) { timing -= 0.1; if (timing <= 0) timing = 1; printf("%f\n", timing);}
			else if (e.key.keysym.sym == SDLK_LEFT) { timing += 0.1; if (timing >= 1000) timing = 1000; printf("%f\n", timing);}
			else if (e.key.keysym.sym == SDLK_SEMICOLON) { mode = 2; }
			else if (mode == 2)
			{
				switch (e.key.keysym.sym)
				{
					case (SDLK_0): jump = jump * 10 + 0; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_1): jump = jump * 10 + 1; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_2): jump = jump * 10 + 2; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_3): jump = jump * 10 + 3; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_4): jump = jump * 10 + 4; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_5): jump = jump * 10 + 5; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_6): jump = jump * 10 + 6; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_7): jump = jump * 10 + 7; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_8): jump = jump * 10 + 8; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_9): jump = jump * 10 + 9; printf(":%zu\n", jump); if (jump >= 1000000) jump = 0; break;
					case (SDLK_RETURN):
						mode = 0;
						printf("jumping to: %zu\n", jump);
						if (jump < gPdf.nbOfPages)
						{
							for (int i = 0; i < 3; i++)
								fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gPdf.viewingPage + i].pPix);
							gPdf.viewingPage = jump;
						}
						jump = 0;
						break;
				}
			}
			else
				printf("%d\n", e.key.keysym.sym);
		}
		else if (e.type == SDL_MOUSEWHEEL)
		{
			if (e.wheel.y > 0) { z += 300;}
			else if (e.wheel.y < 0) { z -= 300;}
		}

		SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
		SDL_RenderClear(gInst.pRenderer);

		if (z > 0 && z % 900 == 0 && z != 0 && gPdf.viewingPage > 0)
		{
			z = 0;
			for (int i = 0; i < 3; i++)
				fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gPdf.viewingPage + i].pPix);
			gPdf.viewingPage--;
			for (int i = 0; i < 3; i++)
			{
				sInfo.pageStart = gPdf.viewingPage + i;
				sInfo.nbrPages = 1;
				sInfo.fZoom = 100;
				sInfo.fDpi = 100;
				sInfo.fRotate = 0;

				gPdf.pPages[gPdf.viewingPage + i].pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
				gPdf.pPages[i].bPpmCache = true;
				ta.ppTexture[i] = 
					PixmapToTexture(gInst.pRenderer, gPdf.pPages[gPdf.viewingPage + i].pPix, gPdf.pCtx, ta.ppTexture[i]);
				ta.currentSize++;
			}
		}
		else if (z % 900 == 0 && z != 0)
		{
			z = 0;
			for (int i = 0; i < 3; i++)
				fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gPdf.viewingPage + i].pPix);
			gPdf.viewingPage++;
			for (int i = 0; i < 3; i++)
			{
				sInfo.pageStart = gPdf.viewingPage + i;
				sInfo.nbrPages = 1;
				sInfo.fZoom = 100;
				sInfo.fDpi = 100;
				sInfo.fRotate = 0;

				gPdf.pPages[gPdf.viewingPage + i].pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
				gPdf.pPages[i].bPpmCache = true;
				ta.ppTexture[i] = 
					PixmapToTexture(gInst.pRenderer, gPdf.pPages[gPdf.viewingPage + i].pPix, gPdf.pCtx, ta.ppTexture[i]);
				ta.currentSize++;
			}
		}
		for (int i = 0; i < 3; i++)
		{
			SDL_FRect view = gView.currentView;
			view.y = SDL_fmodf(((view.h + 20)* i) + z, (float)3 * view.h);

			/* if (z != old) { printf("%d\n", z); old = z; } */
			if (max < view.y)
			{
				max = view.y;
				printf("max: %f\n", max);
			}
			SDL_RenderCopyF(gInst.pRenderer, ta.ppTexture[i], NULL, &view);
		}
		double elapsed = GetElapsed(GetCounter(), startTime);
		if (elapsed >= timing && stop)
		{
			startTime = GetCounter();
			z += incr;
		} 
		if (mode == 2)
		{
			SDL_SetRenderDrawColor(gInst.pRenderer, 0xFF, 0x00, 0x00, 0xFF);
			SDL_Rect defaultRect = {
				.x = gView.currentView.x,
				.y = gView.currentView.y,
				.w = gView.currentView.w,
				.h = gView.currentView.h
			};
			SDL_RenderFillRect(gInst.pRenderer, &defaultRect);
		}
		SDL_RenderPresent(gInst.pRenderer);
	}
	exit(1);
	while(gInst.running)
	{
		TracyCFrameMark
		Event(&e);
		UpdateSmooth(factor);
		UpdateTextures(gInst.pRenderer, PollEvent(&gEventQueue));
        /*
		 * Checks if it moved
		 * TODO: make an UpdateFrameShown() to render when page changed, window is focused
		 * or anything else
         */
		if (!SDL_FRectEquals(&check, &gView.currentView) || gRender == true)
		{
			bool isTextureCached = gPdf.pPages[gPdf.viewingPage].bTextureCache;
			SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
			SDL_RenderClear(gInst.pRenderer);

			if (isTextureCached == true)
				SDL_RenderCopyF(gInst.pRenderer, gInst.pMainTexture,
						NULL, &gView.currentView);
            /*
			 * if (isTextureCached == true)
			 * 	SDL_RenderCopyF(gInst.pRenderer, gPdf.pPages[gPdf.viewingPage].pTexture,
			 * 			NULL, &gView.currentView);
             */
			else
			{
				/* TODO: add default texture */
				SDL_SetRenderDrawColor(gInst.pRenderer, 0xFF, 0x00, 0x00, 0xFF);
				SDL_Rect defaultRect = {
					.x = gView.currentView.x,
					.y = gView.currentView.y,
					.w = gView.currentView.w,
					.h = gView.currentView.h
				};
				SDL_RenderFillRect(gInst.pRenderer, &defaultRect);
			}
        
			SDL_RenderPresent(gInst.pRenderer);
			check.x = gView.currentView.x;
			check.y = gView.currentView.y;
			check.w = gView.currentView.w;
			check.h = gView.currentView.h;
            /*
			 * NOTE: Probably be better to add a mutex here as threads 
			 * might be able to trigger it
             */
			gRender = false;
		}
	}

    /*
	 * Loop through the pPages->pPix;
	 * fz_drop_pixmap(pdf.pCtx, pdf.ppPix[0]);
     */
	fz_drop_document(gPdf.pCtx, gPdf.pDoc);
	fz_drop_context(gPdf.pCtx);
    /*
	 * Loop through the pPages->pTextures;
	 * SDL_DestroyTexture();
     */
	free(gPdf.pPages);
	free(gPdf.pFzMutexes);
	SDL_DestroyRenderer(gInst.pRenderer);
	SDL_DestroyWindow(gInst.pWin);
	SDL_Quit();
	return 0;
}

static inline void
UpdateSmooth(float factor)
{
	gView.currentView.x = mInterpolate2(gView.oldView.x, gView.nextView.x, factor);
	gView.currentView.y = mInterpolate2(gView.oldView.y, gView.nextView.y, factor);
	gView.currentView.w = mInterpolate2(gView.oldView.w, gView.nextView.w, factor);
	gView.currentView.h = mInterpolate2(gView.oldView.h, gView.nextView.h, factor);
	gView.oldView.x = gView.currentView.x; gView.oldView.y = gView.currentView.y;
	gView.oldView.w = gView.currentView.w; gView.oldView.h = gView.currentView.h;
}
