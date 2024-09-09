#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN

#include <assert.h>
#include <windows.h>
#include "os_threads.h"

/* -IC:/Lib/mupdf/include -LC:/Lib/mupdf-1.24.9-source/platform/win32/x64/Debug */

#define MAX_THREADS 24
#define BUF_SIZE 255

DWORD WINAPI tRenderPage(LPVOID pData);
void ErrorHandler(LPTSTR lpszFunction);

void
myUnlockMutex(void *pData, int lock)
{
	/* fprintf(stderr, "trying to unlock %d\n", lock); */
	Mutex **ppMutex = (Mutex **) pData;
	ReleaseMutex(ppMutex[lock]);
}

void
myLockMutex(void *pData, int lock)
{
	/* fprintf(stderr, "trying to lock %d\n", lock); */
	Mutex **ppMutex = (Mutex **) pData;
	WaitForSingleObject(ppMutex[lock], INFINITE);
}

pMutex 
myCreateMutex(pMutex pMutex)
{
	return CreateMutexA(NULL, FALSE, NULL);
}

int
myDestroyMutex(Mutex *pMutex)
{
	return 1;
}

void
myWaitThreads(myThread *pThreads, int threadCount)
{
	WaitForMultipleObjects(threadCount, ppThreads, TRUE, INFINITE);
}

myThread
myCreateThread(tData *ptData, myThread *pThread)
{
	return CreateThread(NULL, 0, tRenderPage, ptData, 0, &ptData->id);
}

int
myDestroyThread(myThread Thread)
{
	return CloseHandle(Thread);
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
			
			ppThreads[i] = CreateThread(NULL, 0, tRenderPage, ptData[i], 0, &ptData[i]->id);
			if (ppThreads[i] == NULL) 
			{
				ErrorHandler(TEXT("CreateThread"));
				ExitProcess(3);
			}
		}
		myWaitThreads(ppThreads, count);
		fprintf(stderr, "MainThread leaving\n");
		for(int i = 0; i < count; i++)
		{
			char pFileName[10000];
			CloseHandle(ppThreads[i]);
			if (ptData[i]->failed)
				ErrorHandler("Rendering Failed\n");
			else
			{
				if (pdf->page_count >= 100)
				{
					ErrorHandler("Page_count >= 100\n");
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
		ErrorHandler("Error bro\n");
	}
    return 0;
}

DWORD WINAPI
tRenderPage(void *pData)
{
	tData *ptData = (tData*)pData;
	int pageNumber = ptData->pageNumber;
	fz_context *pCtx;
	fz_display_list *pList = ptData->list;
	fz_rect bbox = ptData->bbox;
	fz_device *pDev = NULL;

	fprintf(stderr, "thread at page %d loading!\n", pageNumber);

	// The context pointer is pointing to the main thread's
	// context, so here we create a new context based on it for
	// use in this thread.
	pCtx = fz_clone_context(ptData->ctx);

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

	return 1;
}
#endif //PLATFORM_WINDOWS
