#include "stdafx.h"
#include "ThreadPool.h"

int gDestroyed;

int ThreadPoolCreate(CM_THREAD_POOL **pThreadPool, unsigned int Capacity)
{
    *pThreadPool = (PCM_THREAD_POOL)malloc(sizeof(CM_THREAD_POOL));
    if (*pThreadPool == NULL)
    {
        //alloc error
        return -1;
    }

    (*pThreadPool)->Capacity = Capacity;    // How many threads can run at the same time
    (*pThreadPool)->hSemaphore = CreateSemaphoreA(
        NULL,
        Capacity,
        Capacity,
        NULL
    );

    if ((*pThreadPool)->hSemaphore == NULL)
    {
        //error semaphore creating
        free(*pThreadPool);
        return -1;
    }

    (*pThreadPool)->Threads = (THREAD_ARRAY_ELEMENT*)malloc(sizeof(THREAD_ARRAY_ELEMENT) * Capacity);    //thread handles and status will be kept here
    if ((*pThreadPool)->Threads == NULL)
    {
        //alloc error
        free(*pThreadPool);
        return -1;
    }

    for (unsigned int i = 0; i < Capacity; i++)
    {
        (*pThreadPool)->Threads[i].Status = 2; //mark them as empty
    }

    gDestroyed = 0; //threadpool not destroyed
    return 0;
}

int THreadPoolDestroy(CM_THREAD_POOL *pThreadPool)
{
    CloseHandle(pThreadPool->hSemaphore);
    for (unsigned int i = 0; i < pThreadPool->Capacity; i++)
    {
        //close handles of remaining threads
        if (pThreadPool->Threads[i].Status != 2)
        {
            WaitForSingleObject(pThreadPool->Threads[i].ThreadHandle, 100);
            CloseHandle(pThreadPool->Threads[i].ThreadHandle);
        }
    }

    free(pThreadPool->Threads);
    free(pThreadPool);

    gDestroyed = 1; //threadpool was destoryed
    return 0;
}

typedef struct _CM_THREAD_POOL_PARAM
{
    CM_THREAD_POOL_FUNC pFunc;      // Work unit received from module caller
    PVOID pParam;
    PCM_THREAD_POOL pThreadPool;    // ThreadPool instance for internal management
    unsigned int Position;          // Position of current thread in ThreadArray
} CM_THREAD_POOL_PARAM;

DWORD WINAPI ThreadPoolThreadFunc(PVOID lpParameter)
{
    CM_THREAD_POOL_PARAM *pParam = (CM_THREAD_POOL_PARAM*)lpParameter;
    
    // Call original func, work unit.
    DWORD ret;
    ret = pParam->pFunc(pParam->pParam);

    if (gDestroyed == 0)
    {
        // Mark current thread as finished
        pParam->pThreadPool->Threads[pParam->Position].Status = 0;

        // Release resources, notify pool
        ReleaseSemaphore(pParam->pThreadPool->hSemaphore, 1, NULL);
    }

    free(pParam);
    return 0;
}

int ThreadPoolApply(CM_THREAD_POOL *ThreadPool, CM_THREAD_POOL_FUNC pFunc, PVOID pParam)
{
    HANDLE hThread;
    CM_THREAD_POOL_PARAM *param;

    // Acquire thread
    DWORD result = WaitForSingleObject(ThreadPool->hSemaphore, 1);
    if (result == WAIT_TIMEOUT)
    {
        //signal that timeout occured, meaning no more space for another client
        return 1;
    }

    for (unsigned int i = 0; i < ThreadPool->Capacity; i++)
    {
        if (ThreadPool->Threads[i].Status == 0)
        {
            WaitForSingleObject(ThreadPool->Threads[i].ThreadHandle, INFINITE);
            CloseHandle(ThreadPool->Threads[i].ThreadHandle);
            ThreadPool->Threads[i].Status = 2;  //mark empty position in vector
        }
    }

    param = (CM_THREAD_POOL_PARAM*)malloc(sizeof(CM_THREAD_POOL_PARAM));
    param->pFunc = pFunc;
    param->pParam = pParam;
    param->pThreadPool = ThreadPool;

    unsigned int position = 0;
    //find empty position in thread array
    for (unsigned int i = 0; i < ThreadPool->Capacity; i++)
    {
        if (ThreadPool->Threads[i].Status == 2)
        {
            position = i;
            break;
        }
    }

    param->Position = position;

    // Call pFunc in acquired thread.
    // Release resources after call, will be done by the intermediary thread func.
    hThread = CreateThread(
        NULL,
        0,
        ThreadPoolThreadFunc,
        (PVOID)param,
        0,
        NULL
    );
    
    // Save thread handle for cleanup
    
    ThreadPool->Threads[position].Status = 1;
    ThreadPool->Threads[position].ThreadHandle = hThread;

    // Return
    return 0;
}