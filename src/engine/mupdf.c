#include "pdf.h"

#include <stdio.h>
#include <stdlib.h>

#include "platform/os_threads.h"
#include "init.h"

#include "tracy/TracyC.h"

#define MAX_PIXMAPS 100
#define MAX_TEXTURES 100

extern int gRender;
/*
 * const float gpZoom[] = {
 * 	10.0f, 20.0f, 50.0f, 75.0f, 100.0f, 125.0f, 150.0f, 175.0f, 200.0f, 400.0f, 600.0f
 * };
 */
const float gpZoom[] = {
	10.0f, 20.0f, 40.0f, 72.0f, 100.0f, 200.0f, 640.0f, 1280.0f, 2560.0f, 5120.0f
};

/*
 * const float gpZoom[] = {
 * 	10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f
 * };
 */

int gCurrentZoom = 3;
int gSizeZoomArray = sizeof(gpZoom) / sizeof(gpZoom[0]);

SDL_Texture* 
PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, SDL_Texture *pTexture);

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* How to think of display logic: physical screen of size
   viewPort.Size() is a window into (possibly much larger)
   total area (canvas) of size canvasSize.

   In DM_SINGLE_PAGE mode total area is the size of currently displayed page
   given current zoom level and rotation.
   In DM_CONTINUOUS mode canvas area consist of all pages rendered sequentially
   with a given zoom level and rotation. canvasSize.dy is the sum of heights
   of all pages plus spaces between them and canvasSize.dx is the size of
   the widest page.

   A possible configuration could look like this:

 -----------------------------------
 |                                 |
 |          -------------          |
 |          | window    |          |
 |          | i.e.      |          |
 |          | view port |          |
 |          -------------          |
 |                                 |
 |                                 |
 |    canvas                       |
 |                                 |
 |                                 |
 |                                 |
 |                                 |
 -----------------------------------

  We calculate the canvas size and position of each page we display on the
  canvas.

  Changing zoom level or rotation requires recalculation of canvas size and
  position of pages in it.

  We keep the offset of view port relative to canvas. The offset changes
  due to scrolling (with keys or using scrollbars).

  To draw we calculate which part of each page overlaps draw area, we render
  those pages to a bitmap and display those bitmaps.
*/

/*
 * TODO: change error handling here
 */
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

    /*
	 * fz_drop_document(PdfCtx->pCtx, PdfCtx->pDoc);
	 * PdfCtx->pDoc = NULL;
     */
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

/*
 * NOTE: Should probable clone the ctx here since we want heavy multithreading..
 */

SDL_Texture* 
LoadTextures(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, int textureFormat)
{
	TracyCZone(ctx4, 1);
	TracyCZoneName(ctx4, "LoadTexture", 1);

	int x = fz_pixmap_x(pCtx, pPix);
	int y = fz_pixmap_y(pCtx, pPix);
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
    SDL_Texture *pTexture = SDL_CreateTexture(pRenderer, sdl_pixel_format, textureFormat, width, height);
	if (!pTexture) fprintf(stderr, "Couldn't create texture: %s\n", SDL_GetError());

	int w = 0, h = 0;
	SDL_QueryTexture(pTexture, NULL, NULL, &w, &h);
    /*
	 * gView3.nextView.w = w;
	 * gView3.nextView.h = h;
     */

	TracyCZoneEnd(ctx4);
    return pTexture;
}

extern float fscalex;
fz_irect ibounds;

SDL_Texture* 
PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, SDL_Texture *pTexture) 
{
	fz_irect v = fz_pixmap_bbox(pCtx, pPix);
	void *pixels;
	int pitch = 0;
	int width = fz_pixmap_width(pCtx, pPix);
	int height = fz_pixmap_height(pCtx, pPix);
	int x3 = fz_pixmap_x(pCtx, pPix);
	int y3 = fz_pixmap_y(pCtx, pPix);
	int components = fz_pixmap_components(pCtx, pPix);
    /*
	 * printf("vrai w: %d\th: %d\tx: %d\t y: %d\n", width, height, x3, y3);
	 * printf("ibounds w: %d\th: %d\tx: %d\t y: %d\n",
	 * 		ibounds.x1 - ibounds.x0, ibounds.y1 - ibounds.y0, ibounds.x0, ibounds.y0);
     */

	if (SDL_LockTexture(pTexture, NULL, &pixels, &pitch) != 0)
	{
		fprintf(stderr, "Failed to lock texture: %s\n", SDL_GetError());
		return NULL;
	}
	unsigned char *dest = (unsigned char *)pixels;

	TracyCZoneNS(ctx2, "PixMapToTexture", 5, 1);
    /*
	 * int y = 0;
	 * // TODO: vectorize dis bitch
	 * for (y = 0; y < height2; ++y)
	 * {
	 * 	memcpy(dest + y * pitch, pix->samples + y * width2 * components2, width2 * components2);
	 * }
     */
    /*
	 * int row_size = ibounds.x1 - ibounds.x0 * components;
     * 
	 * // Copy each row of the rectangle
	 * for (int i = 0; i < height; i++)
	 * {
	 * 	// Calculate the starting point in the source pixmap
	 * 	unsigned char* src_row = pPix->samples + ((ibounds.x0 + i) * width + ibounds.x0) * components;
     * 
	 * 	// Calculate the destination row in the destination buffer (or wherever you're copying to)
	 * 	unsigned char* dst_row = dest + (i * pitch);
     * 
	 * 	// Copy the row
	 * 	memcpy(dst_row, src_row, row_size);
	 * }
     */
	int y;
	// TODO: vectorize dis bitch
	for (y = 0; y < height; ++y)
	{
		memcpy(dest + y * pitch, pPix->samples + y * width * components, width * components);
	}
	/* printf("y2: %d\n", y); */
	SDL_UnlockTexture(pTexture);
    /* if (SDL_UpdateTexture(pTexture, NULL, pPix->samples, width * components)) return NULL; */
	TracyCZoneEnd(ctx2);
    return pTexture;
}

/*
 * For now and as a *TEST* only retrieve one page at a time
 * and see how long it takes/how viable this really is !
 * 
 *  NOTE: 400 ms measured in debug mode
 */
fz_pixmap *
CreatePDFPage(fz_context *pCtx, const char *pFile, sInfo *sInfo)
{
	TracyCZoneNC(createpdf, "CreatePDFPage", 0xFF0000, 1)

	fz_document		*pDoc = gPdf.pDoc;
	fz_context		*pCtxClone;
	fz_pixmap		*pPix;
	fz_device		*pDev;
	fz_matrix		ctm;
	fz_page			*pPage;
	fz_rect			bbox;
	fz_rect			t_bounds;
	/* pCtxClone = gPdf.pCtx; */

	TracyCZoneNC(clone, "CloneContext", 0xffff00, 1)
	pCtxClone = fz_clone_context(pCtx);
	TracyCZoneEnd(clone);

	TracyCZoneNC(lp, "LoadPage", 0x00ff00, 1)
	fz_try(pCtx)
		pPage = fz_load_page(pCtxClone, pDoc, sInfo->pageStart);
	fz_catch(pCtx)
	{
		fz_report_error(pCtx);
		fprintf(stderr, "Cannot load the page\n");
		fprintf(stderr, "Info: zoom: %f\tDpi: %f\tFile: %s\n", sInfo->fZoom, sInfo->fDpi, pFile);
		exit(1);
	}
	TracyCZoneEnd(lp);

	bbox = fz_bound_page(pCtxClone, pPage);
	ctm = fz_transform_page(bbox, gpZoom[gCurrentZoom], sInfo->fRotate);

	ibounds = fz_round_rect(fz_transform_rect(bbox, ctm));
	fz_irect prout = fz_round_rect(fz_transform_rect(bbox, ctm));

    
	/* ibounds.x0 = 100; */
	/* if (ibounds.x1 + gView3.nextView.x > gInst.width)  */
	/* if (ibounds.x1 > gInst.width && gView3.nextView.x > 0)  */
	if (ibounds.x1 > gInst.width) 
		ibounds.x1 = gInst.width - gView3.nextView.x;
	if (gView3.nextView.x < 0)
	{
		/* ibounds.x0 = ibounds.x1 + gView3.currentView.x; */
		ibounds.x0 = gView3.nextView.x * -1;
	}

	/* if (ibounds.y1 + gView3.nextView.y > gInst.height) */
	/* if (ibounds.y1 > gInst.height && gView3.nextView.y > 0) */
	if (ibounds.y1 > gInst.height)
	{
		ibounds.y1 = gInst.height - gView3.nextView.y;
	}
	if (gView3.nextView.y < 0)
	{
		ibounds.y0 = gView3.nextView.y * -1;
	}

	gView3.nextView.w = ibounds.x1 - ibounds.x0;
	gView3.nextView.h = ibounds.y1 - ibounds.y0;
    /*
	 * printf("ibounds x0: %d\ty0: %d\tx1: %d\ty1: %d\n", ibounds.x0, ibounds.y0, ibounds.x1, ibounds.y1);
	 * printf("view x0: %f\ty0: %f\tw: %f\th: %f\n", gView3.nextView.x, gView3.nextView.y, gView3.nextView.w, gView3.nextView.h);
     */

	TracyCZoneNC(pix, "LoadPixMap", 0x00ffff, 1)
	pPix = fz_new_pixmap_with_bbox(pCtxClone, fz_device_rgb(pCtxClone), ibounds, NULL, 0);
	TracyCZoneEnd(pix);

	/* NOTE: According to official docs, this function shouldn't be used and is horrible  */
	TracyCZoneNC(pixClear, "ClearPixMapWithValue", 0xccffcc, 1)
	/* fz_clear_pixmap_rect_with_value(pCtxClone, pPix, 255, ibounds); */
	fz_clear_pixmap_with_value(pCtxClone, pPix, 255);
	TracyCZoneEnd(pixClear);

	pDev = fz_new_draw_device(pCtxClone, ctm, pPix);
	/* pDev = fz_new_draw_device_with_bbox(pCtxClone, ctm, pPix, &ibounds); */

	TracyCZoneNC(pixRun, "RunPage", 0xffccff, 1)
	fz_run_page(pCtxClone, pPage, pDev, fz_identity, NULL);
	TracyCZoneEnd(pixRun);

	TracyCZoneNC(drop, "Dropping everything", 0x00fff0, 1)
	fz_close_device(pCtxClone, pDev);
	fz_drop_device(pCtxClone, pDev);
	fz_drop_page(pCtx, pPage); // NOTE: should we always drop that ?
	fz_drop_context(pCtxClone);
	/* fz_drop_document(pCtxClone, pDoc); */
	TracyCZoneEnd(drop);

	TracyCZoneEnd(createpdf);
	return pPix;
}

PDFPage *
LoadPagesArray(size_t nbOfPages)
{
	PDFPage *pPages = malloc(sizeof(PDFPage) * nbOfPages);
	/* NOTE: This is probably right, have to check: Want every value to 0 ! */
	memset(pPages, 0, sizeof(PDFPage) * nbOfPages);
	int width = 595;
	int height = 842;
	int gap = 20;
	// TODO: retrieve the data from mupdf for page dimensions !
	for (int i = 0; i < nbOfPages; i++)
	{
		pPages[i].bPpmCache = false;
		pPages[i].bTextureCache = false;
		pPages[i].pTexture = NULL;
		pPages[i].pPix = NULL;
		pPages[i].position.w = width;
		pPages[i].position.h = height;
		pPages[i].position.x = (gInst.width / 2.0f) - (width / 2.0f);
		pPages[i].position.y = (i * (height + gap));
		for (int j = 0; j < 3; j++)
			pPages[i].views[j] = pPages[i].position;
	}
	return pPages;
}

fz_matrix
FzCreateViewCtm(fz_rect mediabox, float zoom, int rotation)
{
	fz_matrix ctm = fz_pre_scale(fz_rotate((float)rotation), zoom, zoom);

	rotation = (rotation + 360) % 360;
	if (90 == rotation) ctm = fz_pre_translate(ctm, 0, -1 * mediabox.y1);
	else if (180 == rotation) ctm = fz_pre_translate(ctm, -1 * mediabox.x1, -1 * mediabox.y1);
	else if (270 == rotation) ctm = fz_pre_translate(ctm, -1 * mediabox.x1, 0);

	if (fz_matrix_expansion(ctm) == 0) return fz_identity;
	return ctm;
}

fz_matrix
viewctm(fz_context *pCtx, fz_page* pPage, float zoom, int rotation)
{
    fz_rect bounds;
    fz_var(bounds);
    fz_try(pCtx) {
        bounds = fz_bound_page(pCtx, pPage);
    }
    fz_catch(pCtx) {
        fz_report_error(pCtx);
        bounds = (fz_rect){};
    }
    if (fz_is_empty_rect(bounds)) {
        bounds = (fz_rect){0, 0, 612, 792};
    }
    return FzCreateViewCtm(bounds, zoom, rotation);
}

fz_rect
FzRectFromSDL(SDL_FRect rect)
{
	return (fz_rect){ .x0 = rect.x, .y0 = rect.y, .x1 = rect.x + rect.w, .y1 = rect.y + rect.h };
}

int
LoadTexturesFromThreads(PDFContext *pdf, fz_context *pCtx, const char *pFile, sInfo sInfo)
{
	return 1;
}

int
LoadPixMapFromThreads(PDFContext *pPdf, fz_context *pCtx, const char *pFile, sInfo sInfo)
{
	tData *ptData[100];
	int count = 0;
	fz_document *pDoc = fz_open_document(pCtx, pFile);
	assert(pDoc);
	fz_rect t_bounds;

	fz_matrix ctm;
	ctm = fz_scale(sInfo.fZoom / 100, sInfo.fZoom / 100);
	ctm = fz_pre_rotate(ctm, sInfo.fRotate);

	fz_var(gInst.pThreads);
	fz_try(pCtx)
	{
		count = fz_count_pages(pCtx, pDoc);
		pPdf->nbOfPages = count;
		assert(count > 0);
		count = count > sInfo.pageStart + sInfo.nbrPages ? sInfo.nbrPages : count; 
		assert(count < GetNbProc());
		fprintf(stderr, "nbPage: %d\n", count);
		gInst.pThreads = malloc(sizeof(void *) * count);

		for (int i = 0; i < count; i++)
		{
			fz_page *pPage;
			fz_rect bbox;
			fz_display_list *pList;
			fz_device *pDevice = NULL;
			fz_pixmap *pPix;
			pPdf->pPages[i + sInfo.pageStart].sInfo = sInfo;
			pPdf->pPages[i + sInfo.pageStart].sInfo.pageStart = i + sInfo.pageStart;
			fz_var(pDevice);

			fz_try(pCtx)
			{
				pPage = fz_load_page(pCtx, pDoc, i + sInfo.pageStart);
				bbox = fz_bound_page(pCtx, pPage);
				t_bounds = fz_transform_rect(bbox, ctm);
				pList = fz_new_display_list(pCtx, bbox);
				pDevice = fz_new_list_device(pCtx, pList);
				fz_run_page(pCtx, pPage, pDevice, ctm, NULL);
				fz_close_device(pCtx, pDevice);
			}
			fz_always(pCtx)
			{
				fz_drop_device(pCtx, pDevice);
				fz_drop_page(pCtx, pPage);
			}
			fz_catch(pCtx)
				fz_rethrow(pCtx);

			ptData[i] = malloc(sizeof(tData));

			ptData[i]->ctx = pCtx;
			ptData[i]->pPage = &pPdf->pPages[i + sInfo.pageStart];
			ptData[i]->list = pList;
			ptData[i]->bbox = t_bounds;
			ptData[i]->failed = 0;
			ptData[i]->id = 0;
			
			gInst.pThreads[i] = myCreateThread(ptData[i]);
			if (gInst.pThreads[i] == (myThread)0) 
			{
				ThreadFail("CreateThread");
				exit(1);
			}
		}
		myWaitThreads(gInst.pThreads, count);
		fprintf(stderr, "MainThread leaving\n");
		for(int i = 0; i < count; i++)
		{
			char pFileName[10000];
			myDestroyThread(gInst.pThreads[i]); // Only for WIN32 compatibility
			if (ptData[i]->failed)
				ThreadFail("Rendering Failed\n");
			else
			{
				if (pPdf->nbPagesRetrieved >= 100)
				{
					ThreadFail("Page_count >= 100\n");
					exit(1);
				}
			}
			fz_drop_display_list(pCtx, ptData[i]->list);
			free(ptData[i]);
		}
	}
	fz_always(pCtx)
	{
		free(gInst.pThreads);
	}
	fz_catch(pCtx)
	{
		fz_report_error(pCtx);
		ThreadFail("Error bro\n");
	}
    return 0;
}

/*
 * TODO: add mechanism to privilege creation of actually visible textures (gPdf.viewingPage)
 * As well as destroying "timed out" cached textures..
 */
void
UpdateTextures(SDL_Renderer *pRenderer, int index)
{
	if (index == -1)
		return;
	SDL_Texture *pTmp;
	pTmp = PixmapToTexture(pRenderer, gPdf.pPages[index].pPix, gPdf.pCtx, pTmp);
	if (!pTmp)
	{
		fprintf(stderr, "mupdf.c:22:\t %s\n", SDL_GetError());
		exit(1); // probably should not do that ?
	}
	gPdf.pPages[index].pTexture = pTmp;
	gPdf.pPages[index].bPpmCache = true;
	gRender = true;
	/*
	 * TODO: Measure the time it takes to query
	 * in case it's not negligible
	 */
	int w = 0, h = 0; 
	SDL_QueryTexture(gPdf.pPages[index].pTexture, NULL, NULL, &w, &h);
    /*
	 * gView3.nextView.w = w;
	 * gView3.nextView.h = h;
	 * if (gView3.nextView.x + gView3.nextView.w <= 0)
	 * 	gView3.nextView.x = (gInst.width / 2) - (gView3.nextView.x / 2);
	 * if (gView3.nextView.y + gView3.nextView.h <= 0)
	 * 	gView3.nextView.y = (gInst.height / 2) - (gView3.nextView.h / 2);
	 * gView3.oldView.x = gView3.currentView.x;
	 * gView3.oldView.y = gView3.currentView.y;
	 * gView3.oldView.w = gView3.currentView.w;
	 * gView3.oldView.h = gView3.currentView.h;
     */
}


/*
 * PDFContext
 * CreatePDF(char *input, int page_number, float zoom, float rotate)
 * {
 * 	int page_count;
 * 	fz_context *ctx;
 * 	fz_document *doc;
 * 	fz_pixmap *pix;
 * 	fz_matrix ctm;
 * 	int x, y;
 * 	PDFContext pdf = {0};
 * 	pdf.ppPix = malloc(sizeof(fz_pixmap*) * MAX_PIXMAPS);
 * 	pdf.zoom = zoom;
 * 	pdf.rotate = rotate;
 * 
 * 	fz_locks_context locks;
 * 	Mutex	*pMutexes = malloc(sizeof(Mutex) * FZ_LOCK_MAX);
 * 
 * 	for (int i = 0; i < FZ_LOCK_MAX; i++)
 * 	{
 * 		#<{(| pdf.pMutexes[i] = myCreateMutex(&pdf.pMutexes[i]); |)}>#
 * 		#<{(| if (!ppMutexes[i]) |)}>#
 * 		if(myCreateMutex(&pMutexes[i]) != 0)
 * 		{
 * 			fprintf(stderr, "Could not create mutex\n");
 * 			exit(1);
 * 		}
 * 	}
 * 	pdf.pFzMutexes = pMutexes;
 * 	locks.user = pdf.pFzMutexes;
 * 	locks.lock = myLockMutex;
 * 	locks.unlock = myUnlockMutex;
 * 
 * 	#<{(| Create a context to hold the exception stack and various caches. |)}>#
 * 	ctx = fz_new_context(NULL, &locks, FZ_STORE_UNLIMITED);
 * 	if (!ctx)
 * 	{ fprintf(stderr, "cannot create mupdf context\n"); return pdf; }
 * 	#<{(| Register the default file types to handle. |)}>#
 * 	fz_try(ctx)
 * 		fz_register_document_handlers(ctx);
 * 	fz_catch(ctx)
 * 	{
 * 		fz_report_error(ctx);
 * 		fprintf(stderr, "cannot register document handlers\n");
 * 		fz_drop_context(ctx);
 * 		return pdf;
 * 	}
 * 
 * 	fz_try(ctx)
 * 		doc = fz_open_document(ctx, input);
 * 	fz_catch(ctx)
 * 	{
 * 		fz_report_error(ctx);
 * 		fprintf(stderr, "cannot open document\n");
 * 		fz_drop_context(ctx);
 * 		return pdf;
 * 	}
 * 
 * 	fz_try(ctx)
 * 		page_count = fz_count_pages(ctx, doc);
 * 	fz_catch(ctx)
 * 	{
 * 		fz_report_error(ctx);
 * 		fprintf(stderr, "cannot count number of pages\n");
 * 		fz_drop_document(ctx, doc);
 * 		fz_drop_context(ctx);
 * 		return pdf;
 * 	}
 * 
 * 	if (page_number < 0 || page_number >= page_count)
 * 	{
 * 		fprintf(stderr, "page number out of range:"
 * 				"%d (page count %d)\n", page_number + 1, page_count);
 * 
 * 		fz_drop_document(ctx, doc);
 * 		fz_drop_context(ctx);
 * 		return pdf;
 * 	}
 * 	pdf.nbOfPages = page_count;
 * 	#<{(| Compute a transformation matrix for the zoom and rotation desired. |)}>#
 * 	#<{(| The default resolution without scaling is 72 dpi. |)}>#
 * 	ctm = fz_scale(zoom / 100, zoom / 100);
 * 	ctm = fz_pre_rotate(ctm, rotate);
 * 
 * 	#<{(| Render page to an RGB pixmap. |)}>#
 * 	fz_try(ctx)
 * 		pix = fz_new_pixmap_from_page_number(ctx,
 * 				doc, page_number, ctm, fz_device_rgb(ctx), 0);
 * 
 * 	fz_catch(ctx)
 * 	{
 * 		fz_report_error(ctx);
 * 		fprintf(stderr, "cannot render page\n");
 * 		fz_drop_document(ctx, doc);
 * 		fz_drop_context(ctx);
 * 		return pdf;
 * 	}
 * 	#<{(| fz_drop_pixmap(ctx, pix); |)}>#
 * 	#<{(| fz_drop_context(ctx); |)}>#
 * 	pdf.ppPix[0] = pix;
 * 	pdf.pCtx = ctx;
 * 	pdf.nbPagesRetrieved = 1;
 * 	pdf.viewingPage = 1;
 * 	pdf.pFile = input;
 * 	fz_drop_document(ctx, doc);
 * 	return pdf;
 * 
 * }
 */


