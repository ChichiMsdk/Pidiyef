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

int
main(int Argc, char **ppArgv)
{
	Init(Argc, ppArgv, &gInst);
	sInfo sInfo = {
		.pageStart = Argc > 2 ? atoi(ppArgv[2]) - 1 : 1,
		.fZoom = Argc > 3 ? atof(ppArgv[3]) : 100,
		.fRotate = Argc > 4 ? atof(ppArgv[4]) : 0
	};

    /*
	 * NOTE: This should load the context based on the page that was last open
	 * or on the first page if first time open
     */
	CreatePDFContext(&gPdf, ppArgv[1], sInfo);
    /*
	 * int w = 0, h = 0; 
	 * SDL_QueryTexture(pdf.pTexture, NULL, NULL, &w, &h);
	 * gView.currentView.w = w;
	 * gView.currentView.h = h;
     */

	 int w = 0, h = 0; 
	 gView.currentView.w = (float)1000 / 2;
	 gView.currentView.h = (float)700 / 2;

	gView.currentView.x = gInst.width / 2.0f - gView.currentView.w / 2.0f;
	gView.currentView.y = gInst.height / 2.0f - gView.currentView.h / 2.0f;

	gView.nextView.x = gView.currentView.x; gView.nextView.y = gView.currentView.y;
	gView.nextView.w = gView.currentView.w; gView.nextView.h = gView.currentView.h;

	gView.oldView.x = gView.currentView.x; gView.oldView.y = gView.currentView.y;
	gView.oldView.w = gView.currentView.w; gView.oldView.h = gView.currentView.h;

	SDL_Event e;
	SDL_FRect check = {-1, -1, -1, -1};

	int err = 0;
	float factor = 0.4f;

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
				SDL_RenderCopyF(gInst.pRenderer, gPdf.pPages[gPdf.viewingPage].pTexture,
						NULL, &gView.currentView);
			else
			{
				/* TODO: add default texture */
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
