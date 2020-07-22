#include "ccheap.h"
#include "common.h"

void MakeToHeapMaxUP(CC_HEAP** Heap, int Elements, int Index)
{
    int maxim = Index;
    int left = 2 * Index + 1;
    int right = 2 * Index + 2;
    int resultL = 0;
    int resultR = 0;
    int resultMax = 0;
    int resultIndex = 0;

    VecGetValueByIndex((*Heap)->Array, left, &resultL);
    VecGetValueByIndex((*Heap)->Array, maxim, &resultMax);
    VecGetValueByIndex((*Heap)->Array, right, &resultR);
    VecGetValueByIndex((*Heap)->Array, Index, &resultIndex);

    if (left < Elements && resultL > resultMax)
        maxim = left;

    VecGetValueByIndex((*Heap)->Array, maxim, &resultMax);

    if (right < Elements && resultR > resultMax)
        maxim = right;

    if (maxim != Index) 
    {
        int aux;
        aux = ((*Heap)->Array->Array[maxim]);
        (*Heap)->Array->Array[maxim] = (*Heap)->Array->Array[Index];
        (*Heap)->Array->Array[Index] = aux;
        MakeToHeapMaxUP(Heap, Elements, maxim);
    }
}

void MakeToHeapMinUP(CC_HEAP** Heap, int Elements, int Index)
{
    int maxim = Index;
    int left = 2 * Index + 1;
    int right = 2 * Index + 2;
    int resultL = 0;
    int resultR = 0;
    int resultMax = 0;
    int resultIndex = 0;

    VecGetValueByIndex((*Heap)->Array, left, &resultL);
    VecGetValueByIndex((*Heap)->Array, maxim, &resultMax);
    VecGetValueByIndex((*Heap)->Array, right, &resultR);
    VecGetValueByIndex((*Heap)->Array, Index, &resultIndex);

    if (left < Elements && resultL < resultMax)
        maxim = left;

    VecGetValueByIndex((*Heap)->Array, maxim, &resultMax);

    if (right < Elements && resultR < resultMax)
        maxim = right;

    if (maxim != Index)
    {
        int aux;
        aux = ((*Heap)->Array->Array[maxim]);
        (*Heap)->Array->Array[maxim] = (*Heap)->Array->Array[Index];
        (*Heap)->Array->Array[Index] = aux;
        MakeToHeapMinUP(Heap, Elements, maxim);
    }
}

int HpCreateMaxHeap(CC_HEAP **MaxHeap, CC_VECTOR* InitialElements)
{
    CC_HEAP* heap;
    heap = NULL;
    if (MaxHeap == NULL)
    {
        return -1;
    }

    heap = (CC_HEAP*)malloc(sizeof(CC_HEAP));
    if (heap == NULL)
    {
        return -1;
    }
    heap->Type = 1;
    if (InitialElements == NULL)
    {
        int retValue;
        retValue = VecCreate(&(heap->Array));
        if (retValue != 0)
        {
            return -1;
        }
        *MaxHeap = heap;
        return 0;
    }
    else
    {
        int nrElem;
        int start;
        nrElem = VecGetCount(InitialElements);

        int retValue;
        heap->Array = NULL;
        retValue = VecCreate(&(heap->Array));
        if (retValue != 0)
        {
            return -1;
        }
        *MaxHeap = heap;

        retValue = VecAppend(InitialElements, (*MaxHeap)->Array);
        if (retValue == -1)
        {
            return -1;
        }

        start = (nrElem / 2) - 1;
        for (int i = start; i >= 0; i--)
        {
            MakeToHeapMaxUP(MaxHeap, nrElem, i);
        }
        return 0;
    }
}

int HpCreateMinHeap(CC_HEAP **MinHeap, CC_VECTOR* InitialElements)
{
    CC_HEAP* heap;
    heap = NULL;
    if (MinHeap == NULL)
    {
        return -1;
    }

    heap = (CC_HEAP*)malloc(sizeof(CC_HEAP));
    if (heap == NULL)
    {
        return -1;
    }
    heap->Type = 0;
    if (InitialElements == NULL)
    {
        int retValue;
        retValue = VecCreate(&(heap->Array));
        if (retValue != 0)
        {
            return -1;
        }
        *MinHeap = heap;
        return 0;
    }
    else
    {
        int nrElem;
        int start;
        nrElem = VecGetCount(InitialElements);

        int retValue;
        heap->Array = NULL;
        retValue = VecCreate(&(heap->Array));
        if (retValue != 0)
        {
            return -1;
        }
        *MinHeap = heap;

        retValue = VecAppend(InitialElements, (*MinHeap)->Array);
        if (retValue == -1)
        {
            return -1;
        }
        start = (nrElem / 2) - 1;
        for (int i = start; i >= 0; i--)
        {
            MakeToHeapMinUP(MinHeap, nrElem, i);
        }
        return 0;
    }
}

int HpDestroy(CC_HEAP **Heap)
{
    if (Heap == NULL)
    {
        return -1;
    }

    int retValue;
    retValue = VecDestroy(&(*Heap)->Array);

    if (retValue != 0)
    {
        return -1;
    }

    free(*Heap);
    *Heap = NULL;

    return 0;
}

void MoveUpMin(CC_HEAP* Heap, int Index)
{
    int retVal;
    int child;
    int parent;
    int parentInd;
    parentInd = (Index - 1) / 2;

    if (parentInd >= 0)
    {
        VecGetValueByIndex(Heap->Array, parentInd, &parent);
        VecGetValueByIndex(Heap->Array, Index, &child);

        if (parent > child)
        {
            int aux;
            aux = (Heap->Array->Array[parentInd]);
            Heap->Array->Array[parentInd] = Heap->Array->Array[Index];
            Heap->Array->Array[Index] = aux;

            MoveUpMin(Heap, parent);
        }
    }
}

void MoveUpMax(CC_HEAP* Heap, int Index)
{
    int retVal;
    int child;
    int parent;
    int parentInd;
    parentInd = (Index - 1) / 2;

    if (parentInd >= 0)
    {
        VecGetValueByIndex(Heap->Array, parentInd, &parent);
        VecGetValueByIndex(Heap->Array, Index, &child);

        if (parent < child)
        {
            int aux;
            aux = (Heap->Array->Array[parentInd]);
            Heap->Array->Array[parentInd] = Heap->Array->Array[Index];
            Heap->Array->Array[Index] = aux;

            MoveUpMax(Heap, parent);
        }
    }
}

int HpInsert(CC_HEAP *Heap, int Value)
{
    if (Heap == NULL)
    {
        return -1;
    }
    if (Heap->Type == 0)
    {
        int retVal;
        retVal = VecInsertTail(Heap->Array, Value);
        if (retVal == -1)
        {
            return -1;
        }
        
        int nrElements;
        nrElements = VecGetCount(Heap->Array);
        MoveUpMin(Heap, nrElements-1);

        return 0;
    }
    else if (Heap->Type == 1)
    {
        int retVal;
        retVal = VecInsertTail(Heap->Array, Value);
        if (retVal == -1)
        {
            return -1;
        }

        int nrElements;
        nrElements = VecGetCount(Heap->Array);
        MoveUpMax(Heap, nrElements - 1);

        return 0;
    }
}

int HpRemove(CC_HEAP *Heap, int Value)
{
    if (Heap == NULL)
    {
        return -1;
    }
    if (Heap->Type == 0)
    {
        //min
        int value;
        int retVal;
        retVal = VecGetCount(Heap->Array);

        for (int i = 0; i < retVal; i++)
        {
            VecGetValueByIndex(Heap->Array, i, &value);
            if (value == Value)
            {
                Heap->Array->Array[i] = Heap->Array->Array[retVal - 1];
                VecRemoveByIndex(Heap->Array, retVal - 1);
                MakeToHeapMinUP(&Heap, retVal - 1, i);
                return 0;
            }
        }
        return -1;
    }
    else if (Heap->Type == 1)
    {
        //max
        int value;
        int retVal;
        retVal = VecGetCount(Heap->Array);

        for (int i = 0; i < retVal; i++)
        {
            VecGetValueByIndex(Heap->Array, i, &value);
            if (value == Value)
            {
                Heap->Array->Array[i] = Heap->Array->Array[retVal - 1];
                VecRemoveByIndex(Heap->Array, retVal - 1);
                MakeToHeapMaxUP(&Heap, retVal - 1, i);
                return 0;
            }
        }
        return -1;
    }
}

int HpGetExtreme(CC_HEAP *Heap, int* ExtremeValue)
{
    if (Heap == NULL || ExtremeValue == NULL)
    {
        return -1;
    }

    if (Heap->Array->Count != 0)
    {
        *ExtremeValue = Heap->Array->Array[0];
        return 0;
    }
    else
    {
        return -1;
    }
}

int HpPopExtreme(CC_HEAP *Heap, int* ExtremeValue)
{
    if (Heap == NULL)
    {
        return -1;
    }
    if (Heap->Array->Count != 0)
    {
        *ExtremeValue = Heap->Array->Array[0];
        int retValue;
        retValue = HpRemove(Heap, (*ExtremeValue));

        if (retValue != -1)
        {
            return 0;
        }
        else return -1;
    }
    else return -1;
}

int HpGetElementCount(CC_HEAP *Heap)
{
    if (Heap == NULL)
    {
        return -1;
    }
    int retValue;
    retValue = VecGetCount(Heap->Array);

    if (retValue != -1)
    {
        return retValue;
    }
    else
    {
        return -1;
    }
}

int HpSortToVector(CC_HEAP *Heap, CC_VECTOR* SortedVector)
{
    if (Heap == NULL)
    {
        return -1;
    }

    if (Heap->Type == 1)
    {
        //max
        int retVal;
        if (Heap->Array->Count == 0)
        {
            return -1;
        }
        retVal = VecClear(SortedVector);
        if (retVal == -1)
        {
            return -1;
        }

        int nrElem = VecGetCount(Heap->Array);
        CC_HEAP* copy = Heap;
        for (int i = nrElem - 1; i >= 0; i--)
        {
            int aux;
            aux = copy->Array->Array[0];
            copy->Array->Array[0] = copy->Array->Array[i];
            copy->Array->Array[i] = aux;
            
            retVal = VecInsertHead(SortedVector, aux);
            if (retVal == -1)
            {
                return -1;
            }

            MakeToHeapMaxUP(&copy, i, 0);
        }       
        return 0;
    }
    if (Heap->Type == 0)
    {
        //min
        int retVal;
        if (Heap->Array->Count == 0)
        {
            return -1;
        }
        retVal = VecClear(SortedVector);
        if (retVal == -1)
        {
            return -1;
        }

        int nrElem = VecGetCount(Heap->Array);
        CC_HEAP* copy = Heap;
        for (int i = nrElem - 1; i >= 0; i--)
        {
            int aux;
            aux = copy->Array->Array[0];
            copy->Array->Array[0] = copy->Array->Array[i];
            copy->Array->Array[i] = aux;

            retVal = VecInsertTail(SortedVector, aux);
            if (retVal == -1)
            {
                return -1;
            }

            MakeToHeapMinUP(&copy, i, 0);
        }

        return 0;
    }
}
