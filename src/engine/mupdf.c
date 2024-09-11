#include "pdf.h"

#include <stdio.h>
#include <stdlib.h>
#include "platform/os_threads.h"

#include <SDL2/SDL_render.h>
#include "tracy/TracyC.h"

/*
 * For now and as a *TEST* only retrieve one page at a time
 * and see how long it takes/how viable this really is !
 * 
 *  NOTE: 400 ms measured in debug mode
 */

fz_pixmap *
CreatePDFPage(fz_context *pCtx, const char *pFile, sInfo *sInfo)
{
	TracyCZoneNC(ctx3, "CreatePDFPage", 0xFF0000, 1)
	fz_document *pDoc;
	fz_pixmap *pPix;
	fz_page *pPage;
	fz_device *pDev;
	fz_rect bbox;
	fz_rect t_bounds;
	fz_context *pCtxClone;

	pCtxClone = fz_clone_context(pCtx);
	pDoc = fz_open_document(pCtxClone, pFile);
	assert(pDoc);

	fz_matrix ctm;
	ctm = fz_scale(sInfo->fZoom / sInfo->fDpi, sInfo->fZoom / sInfo->fDpi);
	ctm = fz_pre_rotate(ctm, sInfo->fRotate);

	TracyCZoneNC(ctx2, "LoadPage", 0x00ff00, 1)
	pPage = fz_load_page(pCtxClone, pDoc, sInfo->pageStart);
	TracyCZoneEnd(ctx2);

	bbox = fz_bound_page(pCtxClone, pPage);
	t_bounds = fz_transform_rect(bbox, ctm);

	TracyCZoneNC(ctx, "LoadPixMap", 0x00ffff, 1)
	pPix = fz_new_pixmap_from_page_number(pCtxClone,pDoc, sInfo->pageStart, ctm,
			fz_device_rgb(pCtxClone), 0);
	TracyCZoneEnd(ctx);

	fz_drop_page(pCtx, pPage);
	fz_drop_document(pCtxClone, pDoc);
	fz_drop_context(pCtxClone);
	TracyCZoneEnd(ctx3);
	return pPix;
}

#include "platform/os_threads.h"
#define MAX_PIXMAPS 100

PDFContext
CreatePDF(char *input, int page_number, float zoom, float rotate)
{
	int page_count;
	fz_context *ctx;
	fz_document *doc;
	fz_pixmap *pix;
	fz_matrix ctm;
	int x, y;
	PDFContext pdf = {0};
	pdf.ppPix = malloc(sizeof(fz_pixmap*) * MAX_PIXMAPS);
	pdf.zoom = zoom;
	pdf.rotate = rotate;

	fz_locks_context locks;
	Mutex	*pMutexes = malloc(sizeof(Mutex) * FZ_LOCK_MAX);

	for (int i = 0; i < FZ_LOCK_MAX; i++)
	{
		/* pdf.pMutexes[i] = myCreateMutex(&pdf.pMutexes[i]); */
		/* if (!ppMutexes[i]) */
		if(myCreateMutex(&pMutexes[i]) != 0)
		{
			fprintf(stderr, "Could not create mutex\n");
			exit(1);
		}
	}
	pdf.pMutexes = pMutexes;
	locks.user = pdf.pMutexes;
	locks.lock = myLockMutex;
	locks.unlock = myUnlockMutex;

	/* Create a context to hold the exception stack and various caches. */
	ctx = fz_new_context(NULL, &locks, FZ_STORE_UNLIMITED);
	if (!ctx)
	{ fprintf(stderr, "cannot create mupdf context\n"); return pdf; }
	/* Register the default file types to handle. */
	fz_try(ctx)
		fz_register_document_handlers(ctx);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot register document handlers\n");
		fz_drop_context(ctx);
		return pdf;
	}

	fz_try(ctx)
		doc = fz_open_document(ctx, input);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot open document\n");
		fz_drop_context(ctx);
		return pdf;
	}

	fz_try(ctx)
		page_count = fz_count_pages(ctx, doc);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot count number of pages\n");
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return pdf;
	}

	if (page_number < 0 || page_number >= page_count)
	{
		fprintf(stderr, "page number out of range:"
				"%d (page count %d)\n", page_number + 1, page_count);

		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return pdf;
	}
	pdf.nbOfPages = page_count;
	/* Compute a transformation matrix for the zoom and rotation desired. */
	/* The default resolution without scaling is 72 dpi. */
	ctm = fz_scale(zoom / 100, zoom / 100);
	ctm = fz_pre_rotate(ctm, rotate);

	/* Render page to an RGB pixmap. */
	fz_try(ctx)
		pix = fz_new_pixmap_from_page_number(ctx,
				doc, page_number, ctm, fz_device_rgb(ctx), 0);

	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot render page\n");
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return pdf;
	}
	/* fz_drop_pixmap(ctx, pix); */
	/* fz_drop_context(ctx); */
	pdf.ppPix[0] = pix;
	pdf.pCtx = ctx;
	pdf.nbPagesRetrieved = 1;
	pdf.viewingPage = 1;
	pdf.pFile = input;
	fz_drop_document(ctx, doc);
	return pdf;
}

int
LoadPixMapFromThreads(PDFContext *pdf, fz_context *pCtx, const char *pFile, sInfo sInfo)
{
	tData *ptData[100];
	myThread *ppThreads;
	int count = 0;
	fz_document *pDoc = fz_open_document(pCtx, pFile);
	assert(pDoc);
	fz_rect t_bounds;

	fz_matrix ctm;
	ctm = fz_scale(sInfo.fZoom / 100, sInfo.fZoom / 100);
	ctm = fz_pre_rotate(ctm, sInfo.fRotate);

	fz_var(ppThreads);
	fz_try(pCtx)
	{
		count = fz_count_pages(pCtx, pDoc);
		pdf->nbOfPages = count;
		assert(count > 0);
		count = count > sInfo.pageStart + sInfo.nbrPages ? sInfo.nbrPages : count; 
		assert(count < GetNbProc());
		fprintf(stderr, "nbPage: %d\n", count);
		ppThreads = malloc(sizeof(void *) * count);

		for (int i = 0; i < count; i++)
		{
			fz_page *pPage;
			fz_rect bbox;
			fz_display_list *pList;
			fz_device *pDevice = NULL;
			fz_pixmap *pPix;
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

			ptData[i]->pageNumber = i + 1; // starting page
			ptData[i]->ctx = pCtx;
			ptData[i]->list = pList;
			ptData[i]->bbox = t_bounds;
			ptData[i]->pix = NULL;
			ptData[i]->failed = 0;
			ptData[i]->id = 0;
			
			ppThreads[i] = myCreateThread(ptData[i]);
			if (ppThreads[i] == (myThread)0) 
			{
				ThreadFail("CreateThread");
				exit(1);
			}
		}
		myWaitThreads(ppThreads, count);
		fprintf(stderr, "MainThread leaving\n");
		for(int i = 0; i < count; i++)
		{
			char pFileName[10000];
			myDestroyThread(ppThreads[i]); // Only for WIN32 compatibility
			if (ptData[i]->failed)
				ThreadFail("Rendering Failed\n");
			else
			{
				if (pdf->nbPagesRetrieved >= 100)
				{
					ThreadFail("Page_count >= 100\n");
					exit(1);
				}
				pdf->ppPix[pdf->nbPagesRetrieved++] = ptData[i]->pix; 
			}
			fz_drop_display_list(pCtx, ptData[i]->list);
			free(ptData[i]);
		}
	}
	fz_always(pCtx)
	{
		free(ppThreads);
	}
	fz_catch(pCtx)
	{
		fz_report_error(pCtx);
		ThreadFail("Error bro\n");
	}
    return 0;
}
