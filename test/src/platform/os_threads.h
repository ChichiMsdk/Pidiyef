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
	PDFPage				*pPage;
	fz_context			*ctx;
	fz_display_list		*list;
	fz_rect				bbox;
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

// NOTE: in ms
extern double gPCFreq;
extern uint64_t gCounterStart;
extern uint64_t gCounterEnd;
extern uint64_t gStartProgram;

double		GetElapsed(uint64_t endTime, uint64_t startTime);
void		InitPerfFreq(void);
uint64_t	GetCounter(void);
uint64_t	StartCounter(void);
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
