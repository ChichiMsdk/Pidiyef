#ifdef PLATFORM_LINUX

#	include "os_threads.h"
#	include <pthread.h>
#	include <assert.h>

#define MAX_THREADS 8

void
ThreadFail(const char *pMsg)
{
	fprintf(stderr, "Failed: %s\n", pMsg);
	abort();
}

// These are the two locking functions required by MuPDF when
// operating in a multi-threaded environment. They each take a user
// argument that can be used to transfer some state, in this case a
// pointer to the array of mutexes.

void 
myLockMutex(void *user, int lock)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *) user;

	if (pthread_mutex_lock(&mutex[lock]) != 0)
	{
		printf("lock: %d\n", lock);
		ThreadFail("pthread_mutex_lock()");
	}
}

void 
myUnlockMutex(void *user, int lock)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *) user;

	if (pthread_mutex_unlock(&mutex[lock]) != 0)
	{
		printf("lock: %d\n", lock);
		ThreadFail("pthread_mutex_unlock()");
	}
}

Mutex
myCreateMutex(Mutex *pMutex)
{
	if (pthread_mutex_init(pMutex, NULL) != 0)
	{
		ThreadFail("myCreateMutex\n");
		abort();
	}
	return *pMutex;
}

int
myDestroyMutex(Mutex *pMutex)
{
	return 1;
}

void
myWaitThreads(myThread *pThreads, int threadCount)
{
	for(int i = 0; i < threadCount; i++)
	{
		if (pthread_join(pThreads[i], NULL) != 0)
			ThreadFail("pthread_join()\n");
	}
}

void * tRenderPage(void *pData);

myThread
myCreateThread(tData *ptData)
{
	myThread pThread;
	if (pthread_create(&pThread, NULL, tRenderPage, ptData) != 0)
		return 0;
	return pThread;
}

int
myDestroyThread(myThread Thread)
{
	// WIN32 compatibility
	return 1;
}

void *
tRenderPage(void *pData)
{
	tData *ptData = (tData *)pData;
	int pageNumber = ptData->pageNumber;
	fz_context *pCtx = ptData->ctx;
	fz_display_list *pList = ptData->list;
	fz_rect bbox = ptData->bbox;
	fz_device *pDev = NULL;

	fprintf(stderr, "thread at page %d loading!\n", pageNumber);

	// The context pointer is pointing to the main thread's
	// context, so here we create a new context based on it for
	// use in this thread.
	pCtx = fz_clone_context(pCtx);

	// Next we run the display list through the draw device which
	// will render the request area of the page to the pixmap.

	fz_var(pDev);
	fprintf(stderr, "thread at page %d rendering!\n", pageNumber);
	fz_try(pCtx)
	{
		// Create a white pixmap using the correct dimensions.
		ptData->pix = fz_new_pixmap_with_bbox(pCtx,
				fz_device_rgb(pCtx), fz_round_rect(bbox), NULL, 0);

		fz_clear_pixmap_with_value(pCtx, ptData->pix, 0xff);

		// Do the actual rendering.
		pDev = fz_new_draw_device(pCtx, fz_identity, ptData->pix);
		fz_run_display_list(pCtx, pList, pDev, fz_identity, bbox, NULL);
		fz_close_device(pCtx, pDev);
	}
	fz_always(pCtx)
		fz_drop_device(pCtx, pDev);
	fz_catch(pCtx)
		ptData->failed = 1;

	// Free this thread's context.
	fz_drop_context(pCtx);
	fprintf(stderr, "thread at page %d done!\n", pageNumber);
	return ptData;
}

int
LoadPixMapFromThreads(PDF *pdf, fz_context *pCtx, const char *pFile, sInfo sInfo)
{
	tData *ptData[100];
	myThread *ppThreads;
	int count = 0;
	fz_document *pDoc = fz_open_document(pCtx, pFile);
	assert(pDoc);

	fz_matrix ctm;
	ctm = fz_scale(sInfo.zoom / 100, sInfo.zoom / 100);
	ctm = fz_pre_rotate(ctm, sInfo.rotate);

	fz_var(ppThreads);
	fz_try(pCtx)
	{
		count = fz_count_pages(pCtx, pDoc);
		assert(count > 0);
		count = count > sInfo.pageStart + sInfo.pageNbr ? sInfo.pageNbr : count; 
		assert(count < MAX_THREADS);
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
			ptData[i]->bbox = bbox;
			ptData[i]->pix = NULL;
			ptData[i]->failed = 0;
			ptData[i]->id = 0;
			
			ppThreads[i] = myCreateThread(ptData[i]);
			// Should be fine
			if (ppThreads[i] == NULL) 
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
				if (pdf->page_count >= 100)
				{
					ThreadFail("Page_count >= 100\n");
					exit(1);
				}
				pdf->ppPix[pdf->page_count++] = ptData[i]->pix; 
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

#endif // PLATFORM_LINUX

/*
 * int
 * main(int argc, char **argv)
 * {
 * 	char *filename = argc >= 2 ? argv[1] : "";
 * 	pthread_t *thread = NULL;
 * 	fz_locks_context locks;
 * 	pthread_mutex_t mutex[FZ_LOCK_MAX];
 * 	fz_context *ctx;
 * 	fz_document *doc = NULL;
 * 	int threads;
 * 	int i;
 * 
 * 	// Initialize FZ_LOCK_MAX number of non-recursive mutexes.
 * 	for (i = 0; i < FZ_LOCK_MAX; i++)
 * 	{
 * 		if (pthread_mutex_init(&mutex[i], NULL) != 0)
 * 			ThreadFail("pthread_mutex_init()");
 * 	}
 * 
 * 	// Initialize the locking structure with function pointers to
 * 	// the locking functions and to the user data. In this case
 * 	// the user data is a pointer to the array of mutexes so the
 * 	// locking functions can find the relevant lock to change when
 * 	// they are called. This way we avoid global variables.
 * 	locks.user = mutex;
 * 	locks.lock = myLock_mutex;
 * 	locks.unlock = myUnlockMutex;
 * 
 * 	// This is the main thread's context function, so supply the
 * 	// locking structure. This context will be used to parse all
 * 	// the pages from the document.
 * 	ctx = fz_new_context(NULL, &locks, FZ_STORE_UNLIMITED);
 * 
 * 	fz_var(thread);
 * 	fz_var(doc);
 * 
 * 	fz_try(ctx)
 * 	{
 * 		// Register default file types.
 * 		fz_register_document_handlers(ctx);
 * 
 * 		// Open the PDF, XPS or CBZ document.
 * 		doc = fz_open_document(ctx, filename);
 * 
 * 		// Retrieve the number of pages, which translates to the
 * 		// number of threads used for rendering pages.
 * 		threads = fz_count_pages(ctx, doc);
 * 		fprintf(stderr, "spawning %d threads, one per page...\n", threads);
 * 
 * 		thread = malloc(threads * sizeof (*thread));
 * 		for (i = 0; i < threads; i++)
 * 		{
 * 			fz_page *page;
 * 			fz_rect bbox;
 * 			fz_display_list *list;
 * 			fz_device *dev = NULL;
 * 			fz_pixmap *pix;
 * 			struct thread_data *data;
 * 
 * 			fz_var(dev);
 * 
 * 			fz_try(ctx)
 * 			{
 * 				// Load the relevant page for each thread. Note, that this
 * 				// cannot be done on the worker threads, as only one thread
 * 				// at a time can ever be accessing the document.
 * 				page = fz_load_page(ctx, doc, i);
 * 
 * 				// Compute the bounding box for each page.
 * 				bbox = fz_bound_page(ctx, page);
 * 
 * 				// Create a display list that will hold the drawing
 * 				// commands for the page. Once we have the display list
 * 				// this can safely be used on any other thread.
 * 				list = fz_new_display_list(ctx, bbox);
 * 
 * 				// Create a display list device to populate the page's display list.
 * 				dev = fz_new_list_device(ctx, list);
 * 
 * 				// Run the page to that device.
 * 				fz_run_page(ctx, page, dev, fz_identity, NULL);
 * 
 * 				// Close the device neatly, so everything is flushed to the list.
 * 				fz_close_device(ctx, dev);
 * 			}
 * 			fz_always(ctx)
 * 			{
 * 				// Throw away the device.
 * 				fz_drop_device(ctx, dev);
 * 
 * 				// The page is no longer needed, all drawing commands
 * 				// are now in the display list.
 * 				fz_drop_page(ctx, page);
 * 			}
 * 			fz_catch(ctx)
 * 				fz_rethrow(ctx);
 * 
 * 			// Populate the data structure to be sent to the
 * 			// rendering thread for this page.
 * 			data = malloc(sizeof (*data));
 * 
 * 			data->pagenumber = i + 1;
 * 			data->ctx = ctx;
 * 			data->list = list;
 * 			data->bbox = bbox;
 * 			data->pix = NULL;
 * 			data->failed = 0;
 * 
 * 			// Create the thread and pass it the data structure.
 * 			if (myCreateThread(ptData[i],)
 * 			if (pthread_create(&thread[i], NULL, renderer, data) != 0)
 * 				ThreadFail("pthread_create()");
 * 		}
 * 
 * 		// Now each thread is rendering pages, so wait for each thread
 * 		// to complete its rendering.
 * 		fprintf(stderr, "joining %d threads...\n", threads);
 * 		for (i = 0; i < threads; i++)
 * 		{
 * 			char filename[42];
 * 			struct thread_data *data;
 * 
 * 			if (pthread_join(thread[i], (void **) &data) != 0)
 * 				ThreadFail("pthread_join");
 * 
 * 			if (data->failed)
 * 			{
 * 				fprintf(stderr, "\tRendering for page %d failed\n", i + 1);
 * 			}
 * 			else
 * 			{
 * 				sprintf(filename, "out%04d.png", i);
 * 				fprintf(stderr, "\tSaving %s...\n", filename);
 * 
 * 				// Write the rendered image to a PNG file
 * 				fz_save_pixmap_as_png(ctx, data->pix, filename);
 * 			}
 * 
 * 			// Free the thread's pixmap and display list.
 * 			fz_drop_pixmap(ctx, data->pix);
 * 			fz_drop_display_list(ctx, data->list);
 * 
 * 			// Free the data structure passed back and forth
 * 			// between the main thread and rendering thread.
 * 			free(data);
 * 		}
 * 	}
 * 	fz_always(ctx)
 * 	{
 * 		// Free the thread structure
 * 		free(thread);
 * 
 * 		// Drop the document
 * 		fz_drop_document(ctx, doc);
 * 	}
 * 	fz_catch(ctx)
 * 	{
 * 		fz_report_error(ctx);
 * 		ThreadFail("error");
 * 	}
 * 
 * 	// Finally the main thread's context is freed.
 * 	fz_drop_context(ctx);
 * 
 * 	fprintf(stderr, "finally!\n");
 * 	fflush(NULL);
 * 
 * 	return 0;
 * }
 */
