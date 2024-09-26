#ifdef PLATFORM_LINUX

#	include "platform/os_threads.h"
#	include "import/containers.h"
#	include <unistd.h>
#	include <pthread.h>
#	include <assert.h>
#	include <time.h>

#define MAX_THREADS 8

double gPCFreq = 0;
uint64_t gCounterStart = 0;
uint64_t gCounterEnd = 0;
uint64_t gStartProgram = 0;

void
InitPerfFreq(void)
{
	// Only win32
}

uint64_t
StartCounter(void)
{
    struct timespec counter;
	clock_gettime(CLOCK_MONOTONIC, &counter);
	return gCounterStart = counter.tv_sec / 1000;
}

uint64_t
GetCounter(void)
{
    struct timespec counter;
	clock_gettime(CLOCK_MONOTONIC, &counter);
	return counter.tv_nsec;
}
/*
 * NOTE: Put in place some mechanism to check gPCFreq was init
 */
double
GetElapsed(uint64_t endTime, uint64_t startTime)
{
    return ((endTime - startTime) / 1000.0 / 1000.0);
}

uint64_t
GetNbProc(void)
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}

void
ThreadFail(char *pMsg)
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
	if (lock == -1)
	{
		if (pthread_mutex_lock(mutex) != 0)
			ThreadFail("pthread_mutex_lock()");
	}
	else
	{
		if (pthread_mutex_lock(&mutex[lock]) != 0)
			ThreadFail("pthread_mutex_lock()");
	}
}

void 
myUnlockMutex(void *user, int lock)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *) user;
	if (pthread_mutex_unlock(mutex) != 0)
	{
		printf("lock: %d\n", lock);
		ThreadFail("pthread_mutex_unlock()");
	}
	else
	{
		if (pthread_mutex_unlock(&mutex[lock]) != 0)
		{
			printf("lock: %d\n", lock);
			ThreadFail("pthread_mutex_unlock()");
		}
	}
}

int
myCreateMutex(Mutex *pMutex)
{
	return pthread_mutex_init(pMutex, NULL);
}

int
myDestroyMutex(Mutex *pMutex)
{
	pthread_mutex_destroy(pMutex);
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
    /*
	 * 
	 * int pageNumber = ptData->pPage->sInfo.pageStart;
	 * fz_context *pCtx;
	 * fz_display_list *pList = ptData->list;
	 * fz_rect bbox = ptData->bbox;
	 * fz_device *pDev = NULL;
     * 
	 * fprintf(stderr, "thread at page %d loading!\n", pageNumber);
     * 
	 * // The context pointer is pointing to the main thread's
	 * // context, so here we create a new context based on it for
	 * // use in this thread.
	 * pCtx = fz_clone_context(ptData->ctx);
     * 
	 * // Next we run the display list through the draw device which
	 * // will render the request area of the page to the pixmap.
	 * fz_var(pDev);
	 * fprintf(stderr, "thread at page %d rendering!\n", pageNumber);
	 * fz_try(pCtx)
	 * {
	 * 	// Create a white pixmap using the correct dimensions.
	 * 	ptData->pPage->pPix = fz_new_pixmap_with_bbox(pCtx,
	 * 			fz_device_rgb(pCtx), fz_round_rect(bbox), NULL, 0);
     * 
	 * 	fz_clear_pixmap_with_value(pCtx, ptData->pPage->pPix, 0xff);
     * 
	 * 	// Do the actual rendering.
	 * 	pDev = fz_new_draw_device(pCtx, fz_identity, ptData->pPage->pPix);
	 * 	fz_run_display_list(pCtx, pList, pDev, fz_identity, bbox, NULL);
	 * 	fz_close_device(pCtx, pDev);
	 * }
	 * fz_always(pCtx)
	 * 	fz_drop_device(pCtx, pDev);
	 * fz_catch(pCtx)
	 * 	ptData->failed = 1;
     * 
	 * // Free this thread's context.
	 * fz_drop_context(ptData->ctx);
	 * myLockMutex(&gEventQueue.mutex, -1);
	 * PushEvent(&gEventQueue, pageNumber); //signals main thread to render texture
	 * myUnlockMutex(&gEventQueue.mutex, -1);
	 * fprintf(stderr, "thread at page %d done!\n", pageNumber);
     */
	return ptData;
}

#endif // PLATFORM_LINUX
