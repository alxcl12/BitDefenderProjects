#include "ccstack.h"
#include "common.h"

int StCreate(CC_STACK **Stack)
{
    CC_STACK* curStack = NULL;
    if (NULL == Stack) 
    {
        return -1;
    }

    curStack = (CC_STACK*)malloc(sizeof(CC_STACK));
    if (NULL == curStack)
    {
        return -1;
    }

    NODE* temp;
    temp = (NODE*)malloc(sizeof(NODE));
    if (NULL == temp) 
    {
        //alloc err
        free(curStack);
        return -1;
    }

    temp->Next = NULL;
    curStack->Count = 0;
    curStack->Top = temp;

    *Stack = curStack;

    return 0;
}

int DeallocNodes(NODE* Start)
{
    int result;
    if (Start == NULL)
    {
        return 0;
    }
    result = DeallocNodes(Start->Next);
    free(Start);
    return result;
}

int StDestroy(CC_STACK **Stack)
{
    if (NULL == Stack || *Stack == NULL) 
    {
        return -1;
    }

    int result;
    result = DeallocNodes((*Stack)->Top);

    if (result != 0)
    {
        return -1;
    }

    free(*Stack);

    *Stack = NULL;
    return 0;

}

int StPush(CC_STACK *Stack, int Value)
{
    //empty stack case treated 
    CC_STACK* curStack = Stack;
    if (NULL == Stack) 
    {
        return -1;
    }

    if (curStack->Count != 0) 
    {
        NODE* temp;
        temp = (NODE*)malloc(sizeof(NODE));
        if (temp == NULL) 
        {
            return -1;
        }

        temp->Data = Value;
        temp->Next = curStack->Top;
        curStack->Top = temp;
        curStack->Count += 1;

        return 0;
    }
    else 
    {
        curStack->Top->Data = Value;
        curStack->Count += 1;
        return 0;
    }
}

int StPop(CC_STACK *Stack, int *Value)
{
    if (NULL == Stack) 
    {
        return -1;
    }
    if (NULL == Value) 
    {
        return -1;
    }
    CC_STACK* curStack = Stack;
    if (curStack->Count < 1) 
    {
        //empty stack
        return -1;
    }
    
    (*Value) = curStack->Top->Data;
    if (curStack->Count > 1) 
    {
        NODE* temp;
        temp = curStack->Top;
        curStack->Top = temp->Next;
        curStack->Count -= 1;
        free(temp);
        return 0;
    }
    else 
    {
        curStack->Top->Data = -128; //dummy value, stack is empty
        curStack->Count -= 1;
        return 0;
    }
}

int StPeek(CC_STACK* Stack, int* Value)
{
    if (NULL == Stack || NULL == Value) 
    {
        return -1;
    }

    CC_STACK* curStack = Stack;
    if (curStack->Count > 0) 
    {
        (*Value) = curStack->Top->Data;
        return 0;
    }
    //empty stack
    return -1;
}

int StIsEmpty(CC_STACK *Stack)
{
    if (NULL == Stack) 
    {
        return -1;
    }
    
    if (Stack->Count == 0) 
    {
        return 1;
    }
    //stack not empty
    return 0;
}

int StGetCount(CC_STACK *Stack)
{
    if (NULL == Stack) 
    {
        return -1;
    }

    if (Stack->Count == 0) 
    {
        return 0;
    }
    else 
    {
        return Stack->Count - 1;
    }
}

int StClear(CC_STACK *Stack)
{
    if (NULL == Stack) 
    {
        return -1;
    }

    CC_STACK* curStack = Stack;
    NODE* temp;

    if (curStack->Top != NULL)
    {
        while (curStack->Top->Next != NULL)
        {
            temp = curStack->Top;
            curStack->Top = temp->Next;
            free(temp);
            curStack->Count -= 1;
        }
        curStack->Count -= 1;
        curStack->Top->Data = -128; //dummy data, stack is empty
        return 0;
    }
    return -1;
}

int StPushStack(CC_STACK *Stack, CC_STACK *StackToPush)
{
    //clear StackToPush and keep all elements in auxiliary stack
    //pop all elements from auxStack and push them to Stack
    if (NULL == Stack || NULL == StackToPush) 
    {
        return -1;
    }
    int retValue;
    CC_STACK* auxStack = NULL;
    CC_STACK* copyStack = NULL;
    copyStack = StackToPush;

    retValue = StCreate(&auxStack);
    if (-1 == retValue) 
    {
        return -1;
    }

    int auxInt = 0;
    while (1 != StIsEmpty(copyStack))
    {
        retValue = StPop(copyStack, &auxInt);
        if (-1 == retValue) 
        {
            return -1;
        }

        retValue = StPush(auxStack, auxInt);
        if (-1 == retValue) 
        {
            return -1;
        }
    }
    //now StackToPush is empty
    //auxStack has all elements in reverse order, now need to push them all to Stack

    while (1 != StIsEmpty(auxStack)) 
    {
        retValue = StPop(auxStack, &auxInt);
        if (-1 == retValue) 
        {
            return -1;
        }

        retValue = StPush(Stack, auxInt);
        if (-1 == retValue) 
        {
            return -1;
        }
    }
    retValue = StDestroy(&auxStack);
    if (-1 == retValue) 
    {
        return -1;
    }
    return 0;
}
