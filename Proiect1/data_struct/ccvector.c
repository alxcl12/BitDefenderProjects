#include "ccvector.h"
#include "common.h"
#include "string.h"
#include <stdio.h>

#define INITIAL_SIZE  2048

int VecCreate(CC_VECTOR **Vector)
{
    CC_VECTOR *vec = NULL;

    if (NULL == Vector)
    {
        return -1;
    }

    vec = (CC_VECTOR*)malloc(sizeof(CC_VECTOR));
    if (NULL == vec)
    {
        return -1;
    }

    memset(vec, 0, sizeof(*vec));

    vec->Count = 0;
    vec->Size = INITIAL_SIZE;
    vec->Array = (int*)malloc( sizeof(int) * INITIAL_SIZE );
    if (NULL == vec->Array) 
    {
        free(vec);
        return -1;
    }

    *Vector = vec;

    return 0;
}

int VecDestroy(CC_VECTOR **Vector)
{
    CC_VECTOR *vec = *Vector;

    if (NULL == Vector)
    {
        return -1;
    }

    free(vec->Array);
    free(vec);

    *Vector = NULL;

    return 0;
}

int VecInsertTail(CC_VECTOR *Vector, int Value)
{
    if (NULL == Vector)
    {
        return -1;
    }
    
    if (Vector->Count >= Vector->Size)
    {
        /// REALLOC
        int* new = NULL;

        new = (int*)realloc(Vector->Array, (Vector->Size) * sizeof(int) + INITIAL_SIZE * sizeof(int));
        if (new) 
        {
            Vector->Array = new;
            Vector->Size += INITIAL_SIZE;
        }
        else 
        {
            return -1;
        }
    }
    
    Vector->Array[Vector->Count] = Value;
    Vector->Count += 1;

    return 0;
}

int VecInsertHead(CC_VECTOR *Vector, int Value)
{
    if (NULL == Vector)
    {
        return -1;
    }

    if (Vector->Count >= Vector->Size)
    {
        /// REALLOC
        int* new = NULL;

        new = (int*)realloc(Vector->Array, (Vector->Size) * sizeof(int) + INITIAL_SIZE * sizeof(int));
        if (new) 
        {
            Vector->Array = new;
            Vector->Size += INITIAL_SIZE;
        }
        else 
        {
            return -1;
        }
    }

    for (int i = Vector->Count-1; i >= 0; i--)
    {
        if (Vector->Array[i] && Vector->Array[i + 1])
        {
            Vector->Array[i + 1] = Vector->Array[i];
        }   
    }
    Vector->Array[0] = Value;
    Vector->Count += 1;

    return 0;
}

int VecInsertAfterIndex(CC_VECTOR *Vector, int Index, int Value)
{
    
    if (NULL == Vector || Index < 0)
    {
        return -1;
    }

    if (Vector->Count >= Vector->Size)
    {
        /// REALLOC
        int* new = NULL;

        new = (int*)realloc(Vector->Array, (Vector->Size) * sizeof(int) + INITIAL_SIZE * sizeof(int));
        if (new) 
        {
            Vector->Array = new;
            Vector->Size += INITIAL_SIZE;
        }
        else 
        {
            return -1;
        }
    }

    for (int i = Vector->Count - 1; i > Index; i--)
    {
        if (Vector->Array[i] && Vector->Array[i + 1])
        {
            Vector->Array[i + 1] = Vector->Array[i];
        }     
    }
    if (Vector->Array[Index + 1])
    {
        Vector->Array[Index + 1] = Value;
    }
    Vector->Count += 1;

    return 0;
}

int VecRemoveByIndex(CC_VECTOR *Vector, int Index)
{
    if (NULL == Vector || Index < 0) 
    {
        return -1;
    }

    for (int i = Index; i <= Vector->Count - 2; i++) 
    {
        if (Vector->Array[i] && Vector->Array[i + 1])
        {
            Vector->Array[i] = Vector->Array[i + 1];
        }  
    }

    Vector->Count -= 1;
    return 0;
}

int VecGetValueByIndex(CC_VECTOR *Vector, int Index, int *Value)
{
    if (NULL == Vector || Index < 0 || NULL == Value) 
    {
        return -1;
    }

    if (Index < Vector->Count) 
    {
        (*Value) = Vector->Array[Index];
        return 0;
    }
    else 
    {
        return -1;
    }

    return -1;
}

int VecGetCount(CC_VECTOR *Vector)
{
    if (NULL == Vector)
    {
        return -1;
    }
    return Vector->Count;
}

int VecClear(CC_VECTOR *Vector)
{
    if (NULL == Vector) 
    {
        return -1;
    }

    for (int i = 0; i < Vector->Count; i++) 
    {
        Vector->Array[i] = 0;
    }
    Vector->Count = 0;
    return 0;
}

int Partition(int Vec[], int Left, int Right)
{
    int pivot = Vec[Right];     
    int i = (Left - 1);   

    for (int j = Left; j <= Right - 1; j++)
    {        
        if (Vec[j] > pivot)
        {
            i += 1;
            int temporary;
            temporary = Vec[i];
            Vec[i] = Vec[j];
            Vec[j] = temporary;
        }
    }
    int temporary;
    temporary = Vec[i+1];
    Vec[i+1] = Vec[Right];
    Vec[Right] = temporary;
    return (i + 1);
}

void QuickSort(int Vec[], int Left, int Right)
{
    if (Left < Right)
    {
        int pivot = Partition(Vec, Left, Right);
        QuickSort(Vec, Left, pivot - 1);
        QuickSort(Vec, pivot + 1, Right);
    }
}

int VecSort(CC_VECTOR *Vector)
{
    if (NULL == Vector)
    {
        return -1;
    }
    QuickSort(Vector->Array, 0, Vector->Count);
    return 0;
}

int VecAppend(CC_VECTOR *DestVector, CC_VECTOR *SrcVector)
{
    if (DestVector == NULL || SrcVector == NULL)
    {
        return -1;
    }

    if (DestVector->Count == 0)
    {
        return 0;
    }
    else
    {
        for (int i = 0; i < DestVector->Count; i++) 
        {
            VecInsertTail(SrcVector, DestVector->Array[i]);
        }
        return 0;
    }
}
