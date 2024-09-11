/* -IC:/Lib/mupdf/include -LC:/Lib/mupdf-1.24.9-source/platform/win32/x64/Debug */
#ifdef PLATFORM_WINDOWS
#	define WIN32_LEAN_AND_MEAN
#	define BUF_SIZE 255

#	include <assert.h>
#	include <windows.h>
#	include <tchar.h> // Maybe too bloated .. ?
#	include <strsafe.h>
#	include <sysinfoapi.h>

#	include "containers.h"
#	include "platform/os_threads.h"

double gPCFreq = 0;
uint64_t gCounterStart = 0;
uint64_t gCounterEnd = 0;
uint64_t gStartProgram = 0;

void
InitPerfFreq(void)
{
	LARGE_INTEGER liQuery;
	gPCFreq = QueryPerformanceFrequency(&liQuery);
	gPCFreq = liQuery.QuadPart / 1000.0;
}

uint64_t
StartCounter(void)
{
    LARGE_INTEGER counter;
    const BOOL rc = QueryPerformanceCounter(&counter);
	return gCounterStart = counter.QuadPart;
}

uint64_t
GetCounter(void)
{
    LARGE_INTEGER counter;
    const BOOL rc = QueryPerformanceCounter(&counter);
	return counter.QuadPart;
}
/*
 * NOTE: Put in place some mechanism to check gPCFreq was init
 */
double
GetElapsed(uint64_t endTime, uint64_t startTime)
{
    return (endTime - startTime) / gPCFreq;
}

uint64_t
GetNbProc(void)
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

DWORD WINAPI tRenderPage(LPVOID pData);
void ThreadFail(char *pMsg)
{
	// Retrieve the system error message for the last-error code.
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message.
    lpDisplayBuf = (LPVOID)LocalAlloc(
			LMEM_ZEROINIT, 
			(lstrlen((LPCTSTR) lpMsgBuf) + lstrlen((LPCTSTR) pMsg) + 40) * sizeof(TCHAR)); 

    StringCchPrintf(
			(LPTSTR)lpDisplayBuf, 
			LocalSize(lpDisplayBuf) / sizeof(TCHAR),
			TEXT("%s failed with error %d: %s"), 
			pMsg, dw, lpMsgBuf); 

    MessageBox(NULL, (LPCTSTR) lpDisplayBuf, TEXT("Error"), MB_OK); 

    // Free error-handling buffer allocations.
    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
	abort();
}

/*
 * Locks pData[[lock]] mutex
 * if lock == -1 then pData is not considered as an array
 */
void
myUnlockMutex(void *pData, int lock)
{
	/* fprintf(stderr, "trying to unlock %d\n", lock); */
	if (lock == -1)
		ReleaseMutex(pData);
	else
	{
		Mutex **ppMutex = (Mutex **) pData;
		ReleaseMutex(ppMutex[lock]);
	}
}

/*
 * Locks pData[[lock]] mutex
 * if lock == -1 then pData is not considered as an array
 */
void
myLockMutex(void *pData, int lock)
{
	/* fprintf(stderr, "trying to lock %d\n", lock); */
	if (lock == -1)
		WaitForSingleObject(pData, INFINITE);
	else
	{
		Mutex **ppMutex = (Mutex **) pData;
		WaitForSingleObject(ppMutex[lock], INFINITE);
	}
}

/**
 * return 0 when success, other if failure
 */
int 
myCreateMutex(Mutex *pMutex)
{
	*pMutex = CreateMutexA(NULL, FALSE, NULL);
	if (!(*pMutex))
		return 1;
	return 0;
}

int
myDestroyMutex(Mutex *pMutex)
{
	return 1;
}

void
myWaitThreads(myThread *pThreads, int threadCount)
{
	WaitForMultipleObjects(threadCount, pThreads, TRUE, INFINITE);
}

myThread
myCreateThread(tData *ptData)
{
	return CreateThread(NULL, 0, tRenderPage, ptData, 0, &ptData->id);
}

int
myDestroyThread(myThread Thread)
{
	return CloseHandle(Thread);
}

DWORD WINAPI
tRenderPage(void *pData)
{
	tData *ptData = (tData*)pData;
	int pageNumber = ptData->pPage->sInfo.pageStart;
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
		ptData->pPage->pPix = fz_new_pixmap_with_bbox(pCtx,
				fz_device_rgb(pCtx), fz_round_rect(bbox), NULL, 0);

		fz_clear_pixmap_with_value(pCtx, ptData->pPage->pPix, 0xff);

		// Do the actual rendering.
		pDev = fz_new_draw_device(pCtx, fz_identity, ptData->pPage->pPix);
		fz_run_display_list(pCtx, pList, pDev, fz_identity, bbox, NULL);
		fz_close_device(pCtx, pDev);
	}
	fz_always(pCtx)
		fz_drop_device(pCtx, pDev);
	fz_catch(pCtx)
		ptData->failed = 1;
	// Free this thread's context.
	fz_drop_context(pCtx);
	myLockMutex(gEventQueue.mutex, -1);
	PushEvent(&gEventQueue, pageNumber); //signals main thread to render texture
	myUnlockMutex(gEventQueue.mutex, -1);
	fprintf(stderr, "thread at page %d done!\n", pageNumber);
	return 1;
}

#endif //PLATFORM_WINDOWS
/*
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
			if (ppThreads[i] == NULL) 
			{
				ThreadFail("CreateThread\n");
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
*/
