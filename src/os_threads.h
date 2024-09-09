#ifndef OS_THREADS_H
#define OS_THREADS_H

#include "pdf.h"

#include <stdio.h>
#include <stdlib.h>

// thread data
typedef struct tData
{
	fz_context			*ctx;
	int					pageNumber;
	fz_display_list		*list;
	fz_rect				bbox;
	fz_pixmap			*pix;
	unsigned long		id;
	int					failed;

}tData;

#ifdef PLATFORM_LINUX
	typedef pthread_t myThread;
	typedef pthread_mutex_t Mutex;
#elif PLATFORM_WINDOWS
	typedef void* myThread;
	typedef void* Mutex;
#endif


// provide function for specific pages ?..
typedef struct sInfo
{
	int		pageNbr;
	int		pageStart;
	float	zoom;
	float	rotate;
	float	dpi;
}sInfo;

Mutex	myCreateMutex(Mutex *pMutex);
int		myDestroyMutex(Mutex *pMutex);
void	myLockMutex(void *pData, int lock);
void	myUnlockMutex(void *pData, int lock);
void	myWaitThreads(myThread *pThreads, int threadCount);
int		LoadPixMapFromThreads(PDF *pdf, fz_context *pCtx, const char *pFile, sInfo sInfo);

#endif // OS_THREADS_H
