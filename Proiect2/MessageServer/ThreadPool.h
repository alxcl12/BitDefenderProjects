#pragma once

#include <Windows.h>
#include <stdlib.h>

typedef DWORD (*CM_THREAD_POOL_FUNC)(PVOID pParam);

typedef struct _THREAD_ARRAY_ELEMENT
{
    HANDLE ThreadHandle;   //handle for thread
    DWORD Status;          //1 if working, 0 if finished and 2 if empty position in array
}THREAD_ARRAY_ELEMENT;

typedef struct _CM_THREAD_POOL
{
    unsigned int Capacity;  // How many threads can run at the same time
    HANDLE hSemaphore;
    THREAD_ARRAY_ELEMENT* Threads;  //all handles and status will be here
} CM_THREAD_POOL, *PCM_THREAD_POOL;

int ThreadPoolCreate(CM_THREAD_POOL **pThreadPool, unsigned int Capacity);

int THreadPoolDestroy(CM_THREAD_POOL *pThreadPool);

// Interface implemented by a thread pool
int ThreadPoolApply(CM_THREAD_POOL *ThreadPool, CM_THREAD_POOL_FUNC pFunc, PVOID pParam);   

