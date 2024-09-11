#include "containers.h"

#include <stdlib.h>


/* TODO: add more robust error handling (ERRNO like ?) */
void *
InitQueue(EventQueue *pQueue, size_t size)
{
	pQueue->q = calloc(size, sizeof(int));
	if (!pQueue->q)
		goto FAIL;

	pQueue->head = 0;
	pQueue->tail = 0;
	pQueue->lastPoll = GetCounter();
	if (myCreateMutex(&pQueue->mutex) != 0)
		goto FAIL;

	return pQueue;
FAIL:
	free(pQueue->q);
	pQueue->q = NULL;
	pQueue->mutex = NULL;
	return NULL;
}

void
DestroyQueue(EventQueue *pQueue)
{
	myDestroyMutex(pQueue->mutex);
	free(pQueue->q);
}

void
PushEvent(EventQueue *pQueue, int value)
{
	pQueue->q[pQueue->tail] = value;

	/*
	 * WARNING: this WILL overwrite non handled event when roundtripping
	 */
	pQueue->tail = (pQueue->tail + 1) % MY_MAX_EVENTS;
}

static inline void
PopEvent(EventQueue *pQueue)
{
	pQueue->head = (pQueue->head + 1) % MY_MAX_EVENTS;
}

double timeToPoll = 50.0; // Every 50ms

int
PollEvent(EventQueue *pQueue)
{
	int value = -1;
	if (GetElapsed(GetCounter(), pQueue->lastPoll) >= timeToPoll)
	{
		if (pQueue->head == pQueue->tail)
			return value;
		value = pQueue->q[pQueue->head];
		PopEvent(pQueue);
		pQueue->lastPoll = GetCounter();
	}
	return value;
}

void 
InitArray(dyArray *pArray, size_t size) 
{
	pArray->array = malloc(size * sizeof(int));
	pArray->used = 0;
	pArray->size = size;
}

void
DestroyArray(dyArray *pArray)
{
	free(pArray->array);
	pArray->array = NULL;
	pArray->used = pArray->size = 0;
}

void
InsertArray(dyArray *pArray, int value) 
{
  if (pArray->used == pArray->size) 
  {
    pArray->size *= 2;
	/* NOTE: Should we free if failure here ?  */
    pArray->array = realloc(pArray->array, pArray->size * sizeof(int));
  }
  pArray->array[pArray->used++] = value;
}
