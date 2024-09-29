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

SDL_Texture* 
LoadTextures(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, int textureFormat)
{
	TracyCZoneNC(ctx1, "CreateTexture", 0xffff00, 1);
	int x = fz_pixmap_x(pCtx, pPix);
	int y = fz_pixmap_y(pCtx, pPix);
	int width = fz_pixmap_width(pCtx, pPix);
	int height = fz_pixmap_height(pCtx, pPix);
	int components = fz_pixmap_components(pCtx, pPix);
	int sdl_pixel_format; // You may need to expand grayscale to RGB

	if (components == 1) sdl_pixel_format = SDL_PIXELFORMAT_RGB24;
	else if (components == 3) sdl_pixel_format = SDL_PIXELFORMAT_RGB24;
	else if (components == 4) sdl_pixel_format = SDL_PIXELFORMAT_RGBA32;
	else return NULL;

    SDL_Texture *pTexture = SDL_CreateTexture(pRenderer, sdl_pixel_format, textureFormat, width, height);
	if (!pTexture) fprintf(stderr, "Couldn't create texture: %s\n", SDL_GetError());

	int w = 0, h = 0;
	SDL_QueryTexture(pTexture, NULL, NULL, &w, &h);
	TracyCZoneEnd(ctx1);
    return pTexture;
}

SDL_Texture* 
PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, SDL_Texture *pTexture) 
{
	TracyCZoneNC(ctx, "PixmapToTexture", 0xff0000, 1);
	void *pixels = NULL;
	int pitch = 0;
	int width = fz_pixmap_width(pCtx, pPix);
	int height = fz_pixmap_height(pCtx, pPix);
	int components = fz_pixmap_components(pCtx, pPix);

	if (SDL_LockTexture(pTexture, NULL, &pixels, &pitch) != 0)
	{ fprintf(stderr, "Failed to lock texture: %s\n", SDL_GetError()); return NULL; }

	unsigned char *dest = (unsigned char *)pixels;

	// TODO: vectorize dis bitch
	for (int y = 0; y < height; ++y)
		memcpy(dest + y * pitch, pPix->samples + y * width * components, width * components);

	SDL_UnlockTexture(pTexture);
	TracyCZoneEnd(ctx);
    return pTexture;
}

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
	fz_irect		ibounds;

	TracyCZoneNC(clone, "CloneContext", 0xffff00, 1)
	pCtxClone = fz_clone_context(pCtx);
	TracyCZoneEnd(clone);

	TracyCZoneNC(lp, "LoadPage", 0x00ff00, 1)
	fz_try(pCtx) {pPage = fz_load_page(pCtxClone, pDoc, sInfo->pageStart);}
	fz_catch(pCtx) { fz_report_error(pCtx); exit(1); }
	TracyCZoneEnd(lp);

	bbox = fz_bound_page(pCtxClone, pPage);
	ctm = fz_transform_page(bbox, sInfo->fDpi, sInfo->fRotate);
	/* ctm = fz_transform_page(bbox, gpZoom[gCurrentZoom], sInfo->fRotate); */
	ibounds = fz_round_rect(fz_transform_rect(bbox, ctm));

	TracyCZoneNC(pix, "LoadPixMap", 0x00ffff, 1)
	pPix = fz_new_pixmap_with_bbox(pCtxClone, fz_device_rgb(pCtxClone), ibounds, NULL, 0);
	TracyCZoneEnd(pix);

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
	fz_drop_page(pCtx, pPage);
	fz_drop_context(pCtxClone);
	TracyCZoneEnd(drop);

	TracyCZoneEnd(createpdf);
	return pPix;
}

fz_pixmap *
RenderPdfPage(fz_context *pCtx, const char *pFile, sInfo *sInfo, fz_document *pDoc)
{
	fz_context		*pCtxClone;
	fz_pixmap		*pPix;
	fz_device		*pDev;
	fz_matrix		ctm;
	fz_page			*pPage;
	fz_rect			bbox;
	fz_rect			t_bounds;
	fz_irect		ibounds;

	pCtxClone = fz_clone_context(pCtx);

	fz_try(pCtx) { pPage = fz_load_page(pCtxClone, pDoc, sInfo->pageStart); }
	fz_catch(pCtx) { fz_report_error(pCtx); fprintf(stderr, "Cannot load the page\n"); exit(1); }

	bbox = fz_bound_page(pCtxClone, pPage);
	ctm = fz_transform_page(bbox, sInfo->fDpi, sInfo->fRotate);
	ibounds = fz_round_rect(fz_transform_rect(bbox, ctm));

	pPix = fz_new_pixmap_with_bbox(pCtxClone, fz_device_rgb(pCtxClone), ibounds, NULL, 0);

	/* NOTE: According to official docs, this function shouldn't be used and is horrible  */
	/* fz_clear_pixmap_rect_with_value(pCtxClone, pPix, 255, ibounds); */
	fz_clear_pixmap_with_value(pCtxClone, pPix, 255);

	pDev = fz_new_draw_device(pCtxClone, ctm, pPix);
	/* pDev = fz_new_draw_device_with_bbox(pCtxClone, ctm, pPix, &ibounds); */

	fz_run_page(pCtxClone, pPage, pDev, fz_identity, NULL);

	fz_close_device(pCtxClone, pDev);
	fz_drop_device(pCtxClone, pDev);
	fz_drop_page(pCtx, pPage); // NOTE: should we always drop that ?
	fz_drop_context(pCtxClone);
	return pPix;
}

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

	PdfCtx->pCtx = fz_new_context(NULL, &LocksCtx, FZ_STORE_UNLIMITED);
	if (!PdfCtx->pCtx) { fprintf(stderr, "cannot create mupdf context\n"); return NULL; }

	fz_try(PdfCtx->pCtx)
		fz_register_document_handlers(PdfCtx->pCtx);
	fz_catch(PdfCtx->pCtx)
		goto myErrorHandle;
	fz_try(PdfCtx->pCtx)
		PdfCtx->pDoc = fz_open_document(PdfCtx->pCtx, pFile);
	fz_catch(PdfCtx->pCtx)
		goto myError;
	fz_try(PdfCtx->pCtx)
		PdfCtx->nbOfPages = fz_count_pages(PdfCtx->pCtx, PdfCtx->pDoc);
	fz_catch(PdfCtx->pCtx)
		goto myError;
	PdfCtx->pPages = LoadPagesArray(PdfCtx->nbOfPages);
	PdfCtx->viewingPage = sInfo.pageStart;
	return PdfCtx;

myErrorHandle:
	fz_drop_document(PdfCtx->pCtx, PdfCtx->pDoc);
myError:
	fz_report_error(PdfCtx->pCtx);
	fz_drop_context(PdfCtx->pCtx);
	return NULL;
}

void
PrintRect(void *rect);

PDFPage *
LoadPagesArray(size_t nbOfPages)
{
	PDFPage *pPages = malloc(sizeof(PDFPage) * nbOfPages);
	/* NOTE: This is probably right, have to check: Want every value to 0 ! */
	memset(pPages, 0, sizeof(PDFPage) * nbOfPages);

	// NOTE: are all the pages the same size ? 
	fz_page *pPage = fz_load_page(gPdf.pCtx, gPdf.pDoc, 1);
	fz_rect bounds = fz_bound_page(gPdf.pCtx, pPage);
	fz_irect ibounds = fz_round_rect(bounds);
	fz_drop_page(gPdf.pCtx, pPage);

	int width = ibounds.x1 - ibounds.x0;
	int height = ibounds.y1 - ibounds.y0;
	int gap = 20;

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
