const char* __asan_default_options() { return "detect_leaks=0"; }

#include "init.h"
#include "engine/pdf.h"
#include "gui.h"
#include "platform/os_threads.h"
#include "containers.h"
#include "camera.h"
#include "buttons.h"

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
PDFView gView3 = {0};
SDL_FRect gView = {0};
PDFContext gPdf = {0};
extern float gZoom;
float gZoom = 1.0f;

#define MAX_TEXTURE_ARRAY_SIZE 10

typedef struct TextureArray
{
	SDL_Texture *ppTexture[MAX_TEXTURE_ARRAY_SIZE];
	size_t first;
	size_t last;
	size_t currentSize;
}TextureArray;

typedef struct VisibleArray
{
	int array[20]; // magic number
	int count;
}VisibleArray;

extern VisibleArray gVisible;
VisibleArray gVisible = {0};

SDL_Texture* 
LoadTextures(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, int textureFormat);
void ReloadPage(void);
fz_pixmap * CreatePDFPage(fz_context *pCtx, const char *pFile, sInfo *sInfo);
static inline void UpdateSmooth2(float factor, int i, PDFPage **p);
void MegaLoop(void);

int num = 0;
int incr = -900;
bool stop = false;
float timing = 0.1f;
int mode = 0;

void resize(void);

void
event(SDL_Event *es)
{
	size_t jump = 0;
	size_t oldJump = 0;
	SDL_Event e = *es;
	SDL_PollEvent(&e); 
	const uint8_t *arr = SDL_GetKeyboardState(&num);

	if (e.type == SDL_QUIT) exit(1); 
	// reset the page's positions
	else if (e.type == SDL_WINDOWEVENT_RESIZED || e.type == SDL_WINDOWEVENT_SIZE_CHANGED) { SDL_GetWindowSize(gInst.pWin, &gInst.width, &gInst.height); printf("%d\t%d\n", gInst.width, gInst.height); }
	else if (e.type == SDL_KEYDOWN)
	{ 
		if (arr[SDL_SCANCODE_LCTRL] && arr[SDL_SCANCODE_O]) { int tmp = gPdf.viewingPage; gPdf.viewingPage = oldJump; oldJump = tmp; }
		else if (arr[SDL_SCANCODE_LSHIFT] && arr[SDL_SCANCODE_K]) { fscalex /= 1.1f; if (gView.w <= 0.1f || gView.h <= 0.1f) gView.w = gInst.width; gView.h = gInst.height; resize();}
		else if (arr[SDL_SCANCODE_LSHIFT] && arr[SDL_SCANCODE_J]) { fscalex *= 1.1f; if (gView.w >= 100000.0f || gView.h <= 100000.0f) gView.w = gInst.width; gView.h = gInst.height; resize();}
		else if (e.key.keysym.sym == SDLK_ESCAPE) { exit(1);}
		else if (e.key.keysym.sym == SDLK_k) { gView.y += (300 / fscalex); if (gView.y >= 4000000000) gView.y = 0.0f;}
		else if (e.key.keysym.sym == SDLK_j) { gView.y -= (300 / fscalex); if (gView.y < 0.0f) gView.y = 0.0f;}
		else if (e.key.keysym.sym == SDLK_UP) { incr = 900; printf("Down\n");}
		else if (e.key.keysym.sym == SDLK_DOWN) { incr = -900; printf("Up\n");}
		else if (e.key.keysym.sym == SDLK_SPACE) { stop = !stop; printf("stop is %d\n", stop);}
		else if (e.key.keysym.sym == SDLK_RIGHT) { timing -= 0.1; if (timing <= 0) timing = 1; printf("%f\n", timing);}
		else if (e.key.keysym.sym == SDLK_LEFT) { timing += 0.1; if (timing >= 1000) timing = 1000; printf("%f\n", timing);}
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
		resize();
	}
}

void
resize(void)
{
		// Because resizing
		for (int i = 0; i < gVisible.count; i++)
		{
			SDL_DestroyTexture(gInst.ppTextArray[i]);
			gInst.ppTextArray[i] = NULL;
			fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gVisible.array[i]].pPix);
			gPdf.pPages[gVisible.array[i]].pPix = NULL;
			gPdf.pPages[gVisible.array[i]].bTextureCache = false;
			gPdf.pPages[gVisible.array[i]].bPpmCache = false;
		}
}

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
	// Load the very first page
	ReloadPage();

	gInst.ppTextArray = malloc(sizeof(SDL_Texture *) * 20);

	for (int i = 0; i < 20; i++)
		gInst.ppTextArray[i] = NULL;

	SDL_Event e;
	SDL_FRect check = {-1, -1, -1, -1};

	int err = 0;
	float factor = 0.4f;
	gInst.lastPoll = GetCounter();

	int mode = 0;
	int z = 0;
	int old = 0;
	uint64_t startTime = GetCounter();
	uint64_t timer = GetCounter();
	int count = 0;
	uint64_t jump = 0;
	uint64_t oldJump = 0;
	float max = 0;
	int incr = -900;
	bool stop = false;
	double timing = 15;
	bool redraw = false;
	int num = 0;
		/*
		 * for (int i = 0; i < 4; i++)
		 * {
		 * 	gView3.currentView2[i].x = gView3.currentView.x; gView3.currentView2[i].w = gView3.currentView.w;
		 * 	gView3.currentView2[i].h = gView3.currentView.h;
		 * 
		 * 	gView3.nextView2[i].x = gView3.nextView.x; gView3.nextView2[i].w = gView3.nextView.w;
		 * 	gView3.nextView2[i].h = gView3.nextView.h;
		 * 
		 * 	gView3.oldView2[i].x = gView3.oldView.x; gView3.oldView2[i].w = gView3.oldView.w;
		 * 	gView3.oldView2[i].h = gView3.oldView.h;
		 * 
		 * 	int bord = 0;
		 * 	gView3.currentView2[i].y = SDL_fmodf(((gView3.currentView2[i].h + bord)* i) + z, (float)4 * gView3.currentView.h);
		 * 	gView3.nextView2[i].y = SDL_fmodf(((gView3.currentView2[i].h + bord)* i) + z, (float)4 * gView3.currentView.h);
		 * 	gView3.oldView2[i].y = SDL_fmodf(((gView3.currentView2[i].h + bord)* i) + z, (float)4 * gView3.currentView.h);
		 * }
		 */
	while(gInst.running)
	{
		TracyCFrameMark
		event(&e);
		SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
		SDL_RenderClear(gInst.pRenderer);
		MegaLoop();
		SDL_RenderPresent(gInst.pRenderer);
        /*
		 * Event(&e);
		 * UpdateSmooth(factor);
		 * UpdateTextures(gInst.pRenderer, PollEvent(&gEventQueue));
         */
        /*
		 * Checks if it moved
		 * TODO: make an UpdateFrameShown() to render when page changed, window is focused
		 * or anything else
         */
			/*
			 * if (!SDL_FRectEquals(&check, &gView3.currentView) || gRender == true)
			 * {
			 * 	bool isTextureCached = gPdf.pPages[gPdf.viewingPage].bTextureCache;
			 * 	SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
			 * 	SDL_RenderClear(gInst.pRenderer);
			 * 
			 * 	if (isTextureCached == true)
			 * 		SDL_RenderCopyF(gInst.pRenderer, gInst.pMainTexture,
			 * 				NULL, &gView3.currentView);
			 *     #<{(|
			 * 	 * if (isTextureCached == true)
			 * 	 * 	SDL_RenderCopyF(gInst.pRenderer, gPdf.pPages[gPdf.viewingPage].pTexture,
			 * 	 * 			NULL, &gView.currentView);
			 *      |)}>#
			 * 	else
			 * 	{
			 * 		#<{(| TODO: add default texture |)}>#
			 * 		SDL_SetRenderDrawColor(gInst.pRenderer, 0xFF, 0x00, 0x00, 0xFF);
			 * 		SDL_Rect defaultRect = {
			 * 			.x = gView3.currentView.x,
			 * 			.y = gView3.currentView.y,
			 * 			.w = gView3.currentView.w,
			 * 			.h = gView3.currentView.h
			 * 		};
			 * 		SDL_RenderFillRect(gInst.pRenderer, &defaultRect);
			 * 	}
			 * 
			 * 	SDL_RenderPresent(gInst.pRenderer);
			 * 	check.x = gView3.currentView.x;
			 * 	check.y = gView3.currentView.y;
			 * 	check.w = gView3.currentView.w;
			 * 	check.h = gView3.currentView.h;
			 *     #<{(|
			 * 	 * NOTE: Probably be better to add a mutex here as threads 
			 * 	 * might be able to trigger it
			 *      |)}>#
			 * 	gRender = false;
			 * }
             */
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

void
Reload(int i)
{
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

void
MegaLoop(void)
{
	int guess = 0;
	static int oldGuess = 0;
	guess = (gView.y / (297 + 20));

	if (oldGuess != guess)
	{
		oldGuess = guess;
		printf("Page: %d\n", guess);
	}
	if (guess > 0)
		guess--;
	if (guess > 0)
		guess--;
	float offsetCam = 0;
	float offsetReal = 0;

	float offsetx = 0;
	float offsetx2 = 0;

	float factor = 0.4;
	gVisible.count = 0;

	int i = 0;
	int j = 0;
	for(i = guess; i < gPdf.nbOfPages; i++, j++)
	{
		int rx = 0, ry = 0; 
		int rw = gPdf.pPages[i].position.w, rh = gPdf.pPages[i].position.h; 

		world_to_screen(gPdf.pPages[i].views[0].x, gPdf.pPages[i].views[0].y, &rx, &ry);

		if (i == 0 && ry >= 0)
		{
			offsetCam = ry - 0;
			offsetReal = gPdf.pPages[i].views[2].y - 0;
		}
		if (i == 0 && (rx >= gInst.width || rx + rw <= 0))
		{
			offsetx = 0;
		}

		/* ME = GENIUS (NO) */
		gPdf.pPages[i].views[2].x = gPdf.pPages[i].position.x - (gView.x);
		gPdf.pPages[i].views[2].y = gPdf.pPages[i].position.y - (gView.y) - offsetReal;

		SDL_FRect tmp = {
			.x = (gInst.width / 2.0f) - (gPdf.pPages[i].position.w * fscalex / 2),
			.y = ry - offsetCam,
			.w = gPdf.pPages[i].position.w * fscalex,
			.h = gPdf.pPages[i].position.h * fscalex 
		};

		if (tmp.y <= gInst.height && tmp.y + tmp.h >= 0)
		{
			gVisible.array[gVisible.count] = i;
			gVisible.count++;
			// index for the text array
			int z = gVisible.count - 1;

			bool isTextureCached = gPdf.pPages[i].bTextureCache;
			bool isPpmCached = gPdf.pPages[i].bPpmCache;
			sInfo sInfo = { .fDpi = 100 / fscalex, .fZoom = 100, .fRotate = 0, .nbrPages = 1, .pageStart = i };
			TracyCZoneNC(ctx1, "LoadAndDraw", 0x00FF00, 1);

			if (isPpmCached == false)
			{
				gPdf.pPages[i].pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
				gPdf.pPages[i].bPpmCache = true;
			}
			// if page resized
			if (!gInst.ppTextArray[z])
				gInst.ppTextArray[z] = LoadTextures(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, SDL_TEXTUREACCESS_STREAMING);

			// if wasn't visible beforehand
			if (isTextureCached == false)
			{
				printf("z: %d\n", z);
				// MAKE STRUCT WITH INDEX + TEXTURE
				gInst.ppTextArray[z] = PixmapToTexture(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, gInst.ppTextArray[z]);
				/* gPdf.pPages[i].bTextureCache = true; */
			}
			SDL_RenderCopyF(gInst.pRenderer, gInst.ppTextArray[z], NULL, &tmp);
            /*
			 * gInst.pMainTexture = PixmapToTexture(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, gInst.pMainTexture);
			 * SDL_RenderCopyF(gInst.pRenderer, gInst.pMainTexture, NULL, &tmp);
             */
            /*
		 	 * SDL_SetRenderDrawColor(gInst.pRenderer, 0xFF, 0x00, 0x00, 0xFF);
			 * SDL_RenderFillRectF(gInst.pRenderer, &tmp);
             */
		} 
		else
		{
			fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[i].pPix);
			gPdf.pPages[i].pPix = NULL;
			gPdf.pPages[i].bTextureCache = false;
			gPdf.pPages[i].bPpmCache = false;

			gPdf.pPages[i].views[0].x = gPdf.pPages[i].position.x - gView.x;
			gPdf.pPages[i].views[0].y = gPdf.pPages[i].position.y - gView.y;
			UpdateSmooth2(factor, i, &gPdf.pPages);
			if (gVisible.count >= 4)
				break;
		}
	}
	static int tmpCount = 0;
	if (tmpCount != gVisible.count)
	{
		printf("visible count: %d\n", gVisible.count);
		tmpCount = gVisible.count;
	}
}

static inline void
UpdateSmooth2(float factor, int i, PDFPage **p)
{
	(*p)[i].views[0].w
		= mInterpolate2((*p)[i].views[1].w, (*p)[i].views[2].w, factor);
	(*p)[i].views[0].x
		= mInterpolate2((*p)[i].views[1].x, (*p)[i].views[2].x, factor);
	(*p)[i].views[0].h
		= mInterpolate2((*p)[i].views[1].h, (*p)[i].views[2].h, factor);
	(*p)[i].views[0].y
		= mInterpolate2((*p)[i].views[1].y, (*p)[i].views[2].y, factor);
	(*p)[i].views[1].x = (*p)[i].views[0].x; (*p)[i].views[1].y = (*p)[i].views[0].y;
	(*p)[i].views[1].w = (*p)[i].views[0].w; (*p)[i].views[1].h = (*p)[i].views[0].h;
}

static inline void
UpdateSmooth(float factor)
{
	gView3.currentView.x = mInterpolate2(gView3.oldView.x, gView3.nextView.x, factor);
	gView3.currentView.y = mInterpolate2(gView3.oldView.y, gView3.nextView.y, factor);
	gView3.currentView.w = mInterpolate2(gView3.oldView.w, gView3.nextView.w, factor);
	gView3.currentView.h = mInterpolate2(gView3.oldView.h, gView3.nextView.h, factor);
	gView3.oldView.x = gView3.currentView.x; gView3.oldView.y = gView3.currentView.y;
	gView3.oldView.w = gView3.currentView.w; gView3.oldView.h = gView3.currentView.h;
}
