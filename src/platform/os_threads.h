#ifndef OS_THREADS_H
#define OS_THREADS_H

#include "engine/pdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * NOTE: Where does that even come from ?
 * Could it be superior to 8 ? Infinite ? Wtf
 */
#define MAX_THREADS 8

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

uint64_t	GetNbProc(void);
int			myCreateMutex(Mutex *pMutex);
int			myDestroyMutex(Mutex *pMutex);
void		myLockMutex(void *pData, int lock);
void		myUnlockMutex(void *pData, int lock);

myThread	myCreateThread(tData *ptData);
int			myDestroyThread(myThread Thread);
void		myWaitThreads(myThread *pThreads, int threadCount);

int			LoadPixMapFromThreads(PDFContext *pdf, fz_context *pCtx, const char *pFile, sInfo sInfo);
void		ThreadFail(char *pMsg);

#endif // OS_THREADS_H
