#include "pdf.h"

#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL_render.h>

fz_pixmap *
CreatePDFPage(fz_context *ctx, const char *file)
{
	fz_document *doc;

	fz_try(ctx)
		doc = fz_open_document(ctx, file);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot open document\n");
		fz_drop_context(ctx);
		return NULL;
	}
	fz_pixmap *pix;
	return pix;
}

#include "os_threads.h"
#define MAX_PIXMAPS 100

PDF
CreatePDF(char *input, int page_number, float zoom, float rotate)
{
	int page_count;
	fz_context *ctx;
	fz_document *doc;
	fz_pixmap *pix;
	fz_matrix ctm;
	int x, y;
	PDF pdf = {0};
	pdf.ppPix = malloc(sizeof(fz_pixmap*) * MAX_PIXMAPS);
	pdf.zoom = zoom;
	pdf.rotate = rotate;

	fz_locks_context locks;
	Mutex	*ppMutexes = malloc(sizeof(pthread_mutex_t) * FZ_LOCK_MAX);
	/* pthread_mutex_t	ppMutexes[FZ_LOCK_MAX]; */

	for (int i = 0; i < FZ_LOCK_MAX; i++)
	{
		/* pdf.pMutexes[i] = myCreateMutex(&pdf.pMutexes[i]); */
		ppMutexes[i] = myCreateMutex(&pdf.pMutexes[i]);
        /*
		 * if (!ppMutexes[i])
		 * {
		 * 	fprintf(stderr, "Could not create mutex\n");
		 * 	exit(1);
		 * }
         */
	}
	pdf.ppMutexes = ppMutexes;
	locks.user = pdf.ppMutexes;
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
	/* Open the document. */
	fz_try(ctx)
		doc = fz_open_document(ctx, input);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot open document\n");
		fz_drop_context(ctx);
		return pdf;
	}
	/* Count the number of pages. */
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
		fprintf(stderr, "page number out of range: %d (page count %d)\n", page_number + 1, page_count);
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return pdf;
	}
	/* Compute a transformation matrix for the zoom and rotation desired. */
	/* The default resolution without scaling is 72 dpi. */
	ctm = fz_scale(zoom / 100, zoom / 100);
	ctm = fz_pre_rotate(ctm, rotate);

	/* Render page to an RGB pixmap. */
	fz_try(ctx)
		pix = fz_new_pixmap_from_page_number(ctx, doc, page_number, ctm, fz_device_rgb(ctx), 0);
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
	pdf.page_count = 1;
	pdf.page_nbr = 1;
	pdf.pFile = input;
	fz_drop_document(ctx, doc);
	return pdf;
}
