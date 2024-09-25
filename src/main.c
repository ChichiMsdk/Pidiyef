#include "init.h"
#include "utils.h"

#include "engine/pdf.h"
#include "import/gui.h"
#include "platform/os_threads.h"
#include "import/containers.h"
#include "import/camera.h"
#include "import/buttons.h"

#include <tracy/TracyC.h>

#include <stdio.h>
#include <stdbool.h>

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

Instance gInst = {.running = true, .width = 1300, .height = 900, .pWin = NULL, .pMutexes = NULL};
int gRender = false;
EventQueue gEventQueue = {0};
PDFView gView3 = {0};
PDFView gView2 = {0};
SDL_FRect gView = {0};
PDFContext gPdf = {0};
float gZoom = 1.0f;
EventQueue	gCacheQueue = {0};

#define MAX_TEXTURE_ARRAY_SIZE 10

typedef struct TextureArray
{
	SDL_Texture *ppTexture[MAX_TEXTURE_ARRAY_SIZE];
	size_t first;
	size_t last;
	size_t currentSize;
}TextureArray;

VisibleArray gVisible = {0};

void AppUpdate(void (*Update)(SDL_Event *e), SDL_Event *e);
void AppQuit();
void Version1(SDL_Event *e);
void Version2(SDL_Event *e);
const char* __asan_default_options() { return "detect_leaks=0"; }

int
main(int Argc, char **ppArgv)
{
	void(*Version[2])(SDL_Event *e) = {Version1, Version2};
	enum { ONE = 0, TWO = 1};
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
	int version = ONE;
	CreatePDFContext(&gPdf, ppArgv[1], sInfo);
	// Load the very first page
	if (version == ONE)
		ReloadPage();

	SDL_Event e;

	gInst.lastPoll = GetCounter();
	uint64_t startTime = GetCounter();
	uint64_t timer = GetCounter();
	double timing = 15;
	InitView();
	gView = gView3.currentView2[2];
	while(gInst.running)
	{
		AppUpdate(Version[version], &e);
	}
	AppQuit();
	return 0;
}

void
AppUpdate(void(*Update)(SDL_Event *e), SDL_Event *e)
{
	Update(e);
}

void
Version1(SDL_Event *e)
{
	SDL_FRect check = {-1, -1, -1, -1};
	float factor = 1.0f;
	Event(e);
	/* UpdateTextures(gInst.pRenderer, PollEvent(&gEventQueue)); */
	mUpdateSmooth(factor);
	/*
	 * Checks if it moved
	 * TODO: make an UpdateFrameShown() to render when page changed, window is focused
	 * or anything else
	 */
	if (!SDL_FRectEquals(&check, &gView3.currentView) || gRender == true)
	{
		TracyCZoneNCS(render, "Rendering", 0xffffff, 5, 0)
		bool isTextureCached = gPdf.pPages[gPdf.viewingPage].bTextureCache;
		SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
		SDL_RenderClear(gInst.pRenderer);
		
		int w = 0, h = 0;
		SDL_QueryTexture(gInst.pMainTexture, NULL, NULL, &w, &h);
		SDL_FRect tmp = {
			.x = gView3.currentView.x,
			.y = gView3.currentView.y,
			.w = gView3.currentView.w,
			.h = gView3.currentView.h
		};
		if (isTextureCached == true)
		{
			SDL_RenderCopyF(gInst.pRenderer, gInst.pMainTexture,
					NULL, &tmp);
			SDL_SetRenderDrawColor(gInst.pRenderer, 0xaa, 0xaa, 0x00, 100);
			SDL_Rect defaultRect = {
				.x = gView3.currentView.x * 2,
				.y = gView3.currentView.y,
				.w = gView3.currentView.w / 100,
				.h = gInst.height
			};
			SDL_RenderFillRect(gInst.pRenderer, &defaultRect);
		}
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
				.x = gView3.currentView.x,
				.y = gView3.currentView.y,
				.w = gView3.currentView.w,
				.h = gView3.currentView.h
			};
			SDL_RenderFillRect(gInst.pRenderer, &defaultRect);
		}
	
		SDL_RenderPresent(gInst.pRenderer);
		check.x = gView3.currentView.x;
		check.y = gView3.currentView.y;
		check.w = gView3.currentView.w;
		check.h = gView3.currentView.h;
	    /*
		 * NOTE: Probably be better to add a mutex here as threads 
		 * might be able to trigger it
	     */
		gRender = false;
		TracyCZoneEnd(render);
	}
}

void
Version2(SDL_Event *e)
{
	event(e);
	SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
	SDL_RenderClear(gInst.pRenderer);
	MegaLoop();
	SDL_RenderPresent(gInst.pRenderer);
}

void
AppQuit()
{
	printf("Quitting\n");
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
}
