#include "init.h"
#include "engine/pdf.h"
#include "gui.h"
#include "platform/os_threads.h"

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
SDL_Texture* PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx) ;
static inline void UpdateSmooth(float factor);

extern int gRender;
extern Instance gInst;
int gRender = false;
Instance gInst = {.running = true, .width = 1000, .height = 700};
PDFView gView = {0};
PDFContext gPdf = {0};

PDFPage *
LoadPagesArray(size_t nbOfPages)
{
	PDFPage *pPages = malloc(sizeof(PDFPage) * nbOfPages);
	/* NOTE: This is probably right, have to check: Want every value to 0 ! */
	memset(pPages, 0, sizeof(PDFPage) * nbOfPages);
	return pPages;
}

void *
CreatePDFContext(PDFContext *PdfCtx, char *pFile, sInfo sInfo)
{
	PdfCtx->pFile = pFile;
	PdfCtx->DefaultInfo = sInfo;
	fz_locks_context LocksCtx;
	PdfCtx->pFzMutexes = malloc(sizeof(Mutex) * FZ_LOCK_MAX);

	for (int i = 0; i < FZ_LOCK_MAX; i++)
	{
		if(myCreateMutex(&PdfCtx->pFzMutexes[i]) != 0)
		{ fprintf(stderr, "Could not create mutex\n"); exit(1); }
	}
	LocksCtx.user = PdfCtx->pFzMutexes;
	LocksCtx.lock = myLockMutex;
	LocksCtx.unlock = myUnlockMutex;
	/* Create a context to hold the exception stack and various caches. */
	PdfCtx->pCtx = fz_new_context(NULL, &LocksCtx, FZ_STORE_UNLIMITED);
	if (!PdfCtx->pCtx)
	{
		fprintf(stderr, "cannot create mupdf context\n"); 
		return NULL; 
	}
	fz_try(PdfCtx->pCtx)
		fz_register_document_handlers(PdfCtx->pCtx);
	fz_catch(PdfCtx->pCtx)
		goto myErrorHandle;

	fz_try(PdfCtx->pCtx)
		PdfCtx->pDoc = fz_open_document(PdfCtx->pCtx, pFile);
	fz_catch(PdfCtx->pCtx)
		goto MyErrorOpen;

	fz_try(PdfCtx->pCtx)
		PdfCtx->nbOfPages = fz_count_pages(PdfCtx->pCtx, PdfCtx->pDoc);
	fz_catch(PdfCtx->pCtx)
		goto myErrorCount;

	fz_drop_document(PdfCtx->pCtx, PdfCtx->pDoc);
	PdfCtx->pDoc = NULL;
	PdfCtx->pPages = LoadPagesArray(PdfCtx->nbOfPages);
	PdfCtx->viewingPage = sInfo.pageStart;
	return PdfCtx;

myErrorCount:
	fprintf(stderr, "cannot count number of pages\n");
	fz_drop_document(PdfCtx->pCtx, PdfCtx->pDoc);
	goto MyErrorEnd;
MyErrorOpen:
	fprintf(stderr, "cannot open document\n");
	goto MyErrorEnd;
myErrorHandle:
	fprintf(stderr, "cannot register document handlers\n");
MyErrorEnd:
	fz_report_error(PdfCtx->pCtx);
	fz_drop_context(PdfCtx->pCtx);
	return NULL;
}

int
Main(int Argc, char **ppArgv)
{
	Init(Argc, ppArgv, &gInst);
	sInfo sInfo = {
		.pageStart = Argc > 2 ? atoi(ppArgv[2]) - 1 : 1,
		.fZoom = Argc > 3 ? atof(ppArgv[3]) : 100,
		.fRotate = Argc > 4 ? atof(ppArgv[4]) : 0
	};

	/* TODO: change error handling here */
	CreatePDFContext(&gPdf, ppArgv[1], sInfo);

	pdf.pTexture = PixmapToTexture(gInst.pRenderer, pdf.ppPix[0], pdf.pCtx);
	if (!pdf.pTexture) return 1;

	int w = 0, h = 0; 
	SDL_QueryTexture(pdf.pTexture, NULL, NULL, &w, &h);
	gView.currentView.w = w;
	gView.currentView.h = h;

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

        /*
		 * Checks if it moved
		 * TODO: make an UpdateFrameShown() to render when page changed, window is focused
		 * or anything else
         */
		if (!SDL_FRectEquals(&check, &gView.currentView) || gRender == true)
		{
			SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
			SDL_RenderClear(gInst.pRenderer);

			SDL_RenderCopyF(gInst.pRenderer, gPdf.pTexture, NULL, &gView.currentView);

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
/*
 * NOTE: Should probable clone the ctx here since we want heavy multithreading..
 */

SDL_Texture* 
PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx) 
{
	int width = fz_pixmap_width(pCtx, pPix);
	int height = fz_pixmap_height(pCtx, pPix);
	int components = fz_pixmap_components(pCtx, pPix);

	int sdl_pixel_format; // You may need to expand grayscale to RGB
	if (components == 1) 
		sdl_pixel_format = SDL_PIXELFORMAT_RGB24;
	else if (components == 3) 
		sdl_pixel_format = SDL_PIXELFORMAT_RGB24;
	else if (components == 4)
		sdl_pixel_format = SDL_PIXELFORMAT_RGBA32;
	else
		return NULL;
    SDL_Texture *pTexture = SDL_CreateTexture(pRenderer, 
        sdl_pixel_format, 
        SDL_TEXTUREACCESS_STATIC, 
        width, 
        height
    );

	if (!pTexture)
		fprintf(stderr, "Couldn't create texture: %s\n", SDL_GetError());


    if (SDL_UpdateTexture(pTexture, NULL, pPix->samples, width * components))
		return NULL;
    return pTexture;
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
