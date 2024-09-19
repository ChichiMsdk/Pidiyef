#include "pdf.h"

#include <stdio.h>
#include <stdlib.h>

#include "platform/os_threads.h"
#include "init.h"

#include "tracy/TracyC.h"

#define MAX_PIXMAPS 100
#define MAX_TEXTURES 100

extern int gRender;
SDL_Texture* 
PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, SDL_Texture *pTexture);

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
	gView3.currentView.w = w;
	gView3.currentView.h = h;
}

PDFPage *
LoadPagesArray(size_t nbOfPages)
{
	PDFPage *pPages = malloc(sizeof(PDFPage) * nbOfPages);
	/* NOTE: This is probably right, have to check: Want every value to 0 ! */
	memset(pPages, 0, sizeof(PDFPage) * nbOfPages);
	int width = 210;
	int height = 297;
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
 * NOTE: Should probable clone the ctx here since we want heavy multithreading..
 */

SDL_Texture* 
LoadTextures(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, int textureFormat)
{
	TracyCZone(ctx4, 1);
	TracyCZoneName(ctx4, "LoadTexture", 1);
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
	gView3.currentView.w = w;
	gView3.currentView.h = h;
	gView3.nextView.w = gView3.currentView.w; gView3.nextView.h = gView3.currentView.h;
	gView3.oldView.w = gView3.currentView.w; gView3.oldView.h = gView3.currentView.h;
	TracyCZoneEnd(ctx4);
    return pTexture;
}

SDL_Texture* 
PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, SDL_Texture *pTexture) 
{
	void *pixels;
	int pitch = 0;
	int width = fz_pixmap_width(pCtx, pPix);
	int height = fz_pixmap_height(pCtx, pPix);
	int components = fz_pixmap_components(pCtx, pPix);

	if (SDL_LockTexture(pTexture, NULL, &pixels, &pitch) != 0)
	{
		fprintf(stderr, "Failed to lock texture: %s\n", SDL_GetError());
		return NULL;
	}
	unsigned char *dest = (unsigned char *)pixels;

	TracyCZone(ctx2, 1);
	TracyCZoneName(ctx2, "PixMapToTexture", 1);
	int y = 0;
	// TODO: vectorize dis bitch
	for (y = 0; y < height; ++y)
		memcpy(dest + y * pitch, pPix->samples + y * width * components, width * components);

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

	fz_document *pDoc = gPdf.pDoc;
	fz_pixmap *pPix;
	fz_page *pPage;
	fz_device *pDev;
	fz_rect bbox;
	fz_rect t_bounds;
	fz_context *pCtxClone;

	TracyCZoneNC(clone, "CloneContext", 0xffff00, 1)
	/* pCtxClone = fz_clone_context(pCtx); */
	TracyCZoneEnd(clone);

	pCtxClone = gPdf.pCtx;

	/* TracyCZoneNC(doc, "OpenDoc", 0x0fff00, 1) */

	/* NOTE:Opening the document everytime ? Maybe keep it instead ..? */

    /*
	 * pDoc = fz_open_document(pCtxClone, pFile);
	 * assert(pDoc);
     */

	/* TracyCZoneEnd(doc); */

	fz_matrix ctm;
	ctm = fz_scale(sInfo->fZoom / sInfo->fDpi, sInfo->fZoom / sInfo->fDpi);
	ctm = fz_pre_rotate(ctm, sInfo->fRotate);

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
	t_bounds = fz_transform_rect(bbox, ctm);

	TracyCZoneNC(pix, "LoadPixMap", 0x00ffff, 1)
	pPix = fz_new_pixmap_from_page(pCtxClone, pPage, ctm, fz_device_rgb(pCtxClone), 0);
    /*
	 * pPix = fz_new_pixmap_from_page_number(pCtxClone, pDoc, sInfo->pageStart, ctm,
	 * 		fz_device_rgb(pCtxClone), 0);
     */

	TracyCZoneEnd(pix);

	TracyCZoneNC(drop, "Dropping everything", 0x00fff0, 1)

	fz_drop_page(pCtx, pPage);
	/* fz_drop_document(pCtxClone, pDoc); */
	/* fz_drop_context(pCtxClone); */

	TracyCZoneEnd(drop);
	TracyCZoneEnd(createpdf);
	return pPix;
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


