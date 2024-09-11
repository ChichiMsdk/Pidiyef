#ifndef CONTAINERS_H
#define CONTAINERS_H

#include "platform/os_threads.h"

#define MY_MAX_EVENTS 100

typedef struct EventQueue
{
	int *q;
	size_t head, tail;
	uint64_t lastPoll;
	Mutex mutex;
}EventQueue;

typedef struct dyArray
{
	int *array;
	size_t used;
	size_t size;
}dyArray;

extern EventQueue gEventQueue;

void	InitArray(dyArray *a, size_t Size);
void	DestroyArray(dyArray *pArray);
void	InsertArray(dyArray *pArray, int value);

void	*InitQueue(EventQueue *pQueue, size_t size);
void	DestroyQueue(EventQueue *pQueue);
void	PushEvent(EventQueue *pQueue, int value);
int		PollEvent(EventQueue *pQueue);

#endif  //CONTAINERS_H

