#include <stdio.h>
#include "ccvector.h"
#include "ccstack.h"
#include "cchashtable.h"
#include "ccheap.h"
#include "cctree.h"

#include <stdlib.h>

#define _CRTDBG_MAP_ALLOC 
#include <crtdbg.h>



int TestVector();
int TestStack();
int TestHashTable();
int TestHeap();
int TestTree();

void RunTests();

int main(void)
{
    RunTests();
   
    return 0;
}

void RunTests()
{
    /// NOTE: The tests provided here are by no means exhaustive and are only
    /// provided as a starting point (not all functions are tested, not all use cases
    /// and failure scenarios are covered). You are encouraged to expand these tests
    /// to include missing scenarios.
    if (0 == TestVector())
    {
        _CrtDumpMemoryLeaks();
        printf("Vector test passed\n\n");
    }
    else
    {
        printf("Vector test failed\n\n");
    }

    if (0 == TestStack())
    {
        _CrtDumpMemoryLeaks();
        printf("Stack test passed\n\n");
    }
    else
    {
        printf("Stack test failed\n\n");
    }

    if (0 == TestHashTable())
    {
        _CrtDumpMemoryLeaks();
        printf("HashTable test passed\n\n");
    }
    else
    {
        printf("HashTable test failed\n\n");
    }

    if (0 == TestHeap())
    {
        _CrtDumpMemoryLeaks();
        printf("Heap test passed\n\n");
    }
    else
    {
        printf("Heap test failed\n\n");
    }

    if (0 == TestTree())
    {
        _CrtDumpMemoryLeaks();
        printf("Tree test passed\n\n");
    }
    else
    {
        printf("Tree test failed\n\n");
    }
}


int TestTree()
{
    int retVal = -1;
    CC_TREE* usedTree = NULL;

    retVal = TreeCreate(&usedTree);
    if (0 != retVal)
    {
        printf("TreeCreate failed!\n");
        goto cleanup;
    }

    //retVal = TreeInsert(usedTree, 20);

    retVal = TreeInsert(usedTree, 1);
    
    retVal = TreeInsert(usedTree, 5);
  
    retVal = TreeInsert(usedTree, 10);
    
    retVal = TreeInsert(usedTree, 13);
    
    retVal = TreeInsert(usedTree, 20);
    
    retVal = TreeInsert(usedTree, 2);

    retVal = TreeInsert(usedTree, 3);

    if (0 != retVal)
    {
        printf("TreeInsert failed!\n");
        goto cleanup;
    }

    if (1 != TreeContains(usedTree, 20))
    {
        printf("TreeContains invalid return value!\n");
        retVal = -1;
        goto cleanup;
    }

    int k;
    retVal = TreeGetNthPreorder(usedTree, 3, &k);

    retVal = TreeGetNthInorder(usedTree, 3, &k);

    retVal = TreeGetNthPostorder(usedTree, 3, &k);


    retVal = TreeRemove(usedTree, -55);
    retVal = TreeRemove(usedTree, 13);
    if (0 != retVal)
    {
        printf("TreeRemove failed!\n");
        goto cleanup;
    }

    if (1 != TreeContains(usedTree, 20))
    {
        printf("TreeContains invalid return value after remove!\n");
        retVal = -1;
        goto cleanup;
    }

    if (5 != TreeGetCount(usedTree))
    {
        printf("TreeGetCount invalid return value!\n");
        retVal = -1;
        goto cleanup;
    }
 
cleanup:
    if (NULL != usedTree)
    {
        if (0 != TreeDestroy(&usedTree))
        {
           
            printf("TreeDestroy failed!\n");
            retVal = -1;
        }
    }
    return retVal;
}
              
int TestHeap()
{
    int retVal = -1;
    int foundVal = -1;
    CC_HEAP* usedHeap = NULL;
    CC_VECTOR* vector = NULL;
    VecCreate(&vector);

    VecInsertTail(vector,25);
    VecInsertTail(vector,3); 
    VecInsertTail(vector,15);
    VecInsertTail(vector,22);
    VecInsertTail(vector, 8);
    VecInsertTail(vector, 32);
    VecInsertTail(vector, 17);
  

    retVal = HpCreateMaxHeap(&usedHeap, vector);

    if (0 != retVal)
    {
        printf("HpCreateMinHeap failed!\n");
        goto cleanup;
    }

    int val = 0;
    retVal = HpGetExtreme(usedHeap, &val);

    if (32 != val || retVal != 0) 
    {
        printf("HpGetExtreme failed!\n");
        goto cleanup;
    }

    retVal = HpGetElementCount(usedHeap);
    if (retVal != 7)
    {
        printf("HpGetElementCount failed!\n");
        goto cleanup;
    }

    retVal = HpInsert(usedHeap, 20);
    if (0 != retVal)
    {
        printf("HpInsert failed!\n");
        goto cleanup;
    }

    retVal = HpInsert(usedHeap, 10);
    if (0 != retVal)
    {
        printf("HpInsert failed!\n");
        goto cleanup;
    }

    retVal = HpRemove(usedHeap, 20);
    
    retVal = HpRemove(usedHeap, 32);
    
    if (7 != HpGetElementCount(usedHeap))
    {
        printf("Invalid element count!\n");
        retVal = -1;
        goto cleanup;
    }

    retVal = HpGetExtreme(usedHeap, &foundVal);
    if (0 != retVal)
    {
        printf("HpGetExtreme failed!\n");
        goto cleanup;
    }

    if (25 != foundVal)
    {
        printf("Invalid minimum value returned!");
        retVal = -1;
        goto cleanup;
    }

    retVal = HpSortToVector(usedHeap, vector);

cleanup:
    if (NULL != usedHeap)
    {
        if (0 != HpDestroy(&usedHeap))
        {
            printf("HpDestroy failed!\n");
            retVal = -1;
        }
    }
    VecDestroy(&vector);
    return retVal;
}

int TestHashTable()
{
    int retVal = -1;
    int foundVal = -1;
    CC_HASH_TABLE* usedTable = NULL;
    
    int x;
    x = EqualStrings("aad","asad");
    printf("%d\n ", x);

    retVal = HtCreate(&usedTable);
    if (0 != retVal)
    {
        printf("HtCreate failed!\n");
        goto cleanup;
    }

    retVal = HtSetKeyValue(usedTable, "mere", 20);

    retVal = HtSetKeyValue(usedTable, "mere1", 25);
    
    retVal = HtSetKeyValue(usedTable, "mere2", 30);
    if (0 != retVal)
    {
        printf("HtSetKeyValue failed!\n");
        goto cleanup;
    }

    retVal = HtGetKeyValue(usedTable, "mere", &foundVal);
    if (0 != retVal)
    {
        printf("HtGetKeyValue failed!\n");
        goto cleanup;
    }
    if (foundVal != 20)
    {
        printf("Invalid value after get!\n");
        retVal = -1;
        goto cleanup;
    }
    
    //retVal = HtRemoveKey(usedTable, "mere2");

    if (1 != HtHasKey(usedTable, "mere"))
    {
        printf("Invalid answer to HtHasKey!\n");
        retVal = -1;
        goto cleanup;
    }

    
    CC_HASH_TABLE_ITERATOR* iterator;
    char* key;
    key = NULL;
    retVal = HtGetFirstKey(usedTable, &iterator, &key);

    retVal = HtGetNextKey(iterator, &key);
    retVal = HtGetNextKey(iterator, &key);
    
    retVal = HtReleaseIterator(&iterator);

    //retVal = HtClear(usedTable);
cleanup:
    if (NULL != usedTable)
    {
        if (0 != HtDestroy(&usedTable))
        {
            printf("HtDestroy failed!\n");
            retVal = -1;
        }
    }
    return retVal;
}

int TestStack()
{
    int retVal = -1;
    int foundVal = -1;
    CC_STACK* usedStack = NULL;

    retVal = StCreate(&usedStack);
    if (0 != retVal)
    {
        printf("StCreate failed!\n");
        goto cleanup;
    }

    retVal = StPush(usedStack, 10);
    if (0 != retVal)
    {
        printf("StPush failed!\n");
        goto cleanup;
    }

    if (0 != StIsEmpty(usedStack))
    {
        printf("Invalid answer to StIsEmpty!\n");
        retVal = -1;
        goto cleanup;
    }
    retVal = StPush(usedStack, 10);
    retVal = StPop(usedStack, &foundVal);
    if (0 != retVal)
    {
        printf("StPop failed!\n");
        goto cleanup;
    }

    if (foundVal != 10)
    {
        printf("Invalid value after pop!\n");
        retVal = -1;
        goto cleanup;
    }

    retVal = StPush(usedStack, 10);
    retVal = StPush(usedStack, 11);
    retVal = StPush(usedStack, 12);
    retVal = StPush(usedStack, 13);
    if (0 != StClear(usedStack)) 
    {
        printf("Invalid answer to StClear!\n");
        retVal = -1;
        goto cleanup;
    }

    CC_STACK* stack1 = NULL;
    CC_STACK* stack2 = NULL;
    retVal = StCreate(&stack1);
    retVal = StCreate(&stack2);

    retVal = StPush(stack1, 10);
    retVal = StPush(stack1, 11);
    retVal = StPush(stack2, 12);
    retVal = StPush(stack2, 13);

    if (0 != StPushStack(stack1, stack2)) 
    {
        printf("StPushStack failed!\n");
        retVal = -1;
        goto cleanup;
    }



cleanup:
    if (NULL != usedStack)
    {
        if (0 != StDestroy(&usedStack))
        {
            printf("StDestroy failed!\n");
            retVal = -1;
        }
    }
    StDestroy(&stack1);
    StDestroy(&stack2);
    return retVal;
}

int TestVector()
{
    int retVal = -1;
    int retVal2 = -1;
    int foundVal = 0;
    CC_VECTOR* usedVector = NULL;
    CC_VECTOR* usedVector2 = NULL;
    
    retVal = VecCreate(&usedVector);
    retVal2 = VecCreate(&usedVector2);

    if (0 != retVal)
    {
        printf("VecCreate failed!\n");
        goto cleanup;
    }

    if (0 != retVal2)
    {
        printf("VecCreate failed!\n");
        goto cleanup;
    }

    retVal = VecInsertTail(usedVector, 10);
    if (0 != retVal)
    {
        printf("VecInsertTail failed!\n");
        goto cleanup;
    }

    retVal = VecInsertHead(usedVector, 16);
    if (0 != retVal)
    {
        printf("VecInsertHead failed!\n");
        goto cleanup;
    }

    if (VecGetCount(usedVector) != 2)
    {
        printf("Invalid count returned!\n");
        retVal = -1;
        goto cleanup;
    }

    retVal = VecInsertAfterIndex(usedVector, 0, 20);
    if (0 != retVal)
    {
        printf("VecInsertAfterIndex failed!\n");
        goto cleanup;
    }

    retVal = VecRemoveByIndex(usedVector, 0);
    if (0 != retVal)
    {
        printf("VecRemoveByIndex failed!\n");
        goto cleanup;
    }

    retVal = VecGetValueByIndex(usedVector, 0, &foundVal);
    if (0 != retVal)
    {
        printf("VecGetValueByIndex failed!\n");
        goto cleanup;
    }

    if (foundVal != 20)
    {
        printf("Invalid value found at position 0\n");
        retVal = -1;
        goto cleanup;
    }

    retVal = VecClear(usedVector);
    if (0 != retVal)
    {
        printf("VecClear failed!\n");
        goto cleanup;
    }

    if (0 != VecGetCount(usedVector))
    {
        printf("Invalid count after clear\n");
        retVal = -1;
        goto cleanup;
    }

    retVal = VecInsertTail(usedVector, 15);
    retVal = VecInsertTail(usedVector, 150);
    retVal = VecInsertTail(usedVector, 425);
    retVal = VecInsertTail(usedVector, 65465);
    retVal = VecInsertTail(usedVector, 2156);
    retVal = VecInsertTail(usedVector, 65186);
    retVal = VecInsertTail(usedVector, 166);
    retVal = VecInsertTail(usedVector, 1023);
    retVal = VecInsertTail(usedVector, 5465);

    if (0 != VecSort(usedVector))
    {
        printf("Invalid sort return value\n");
        retVal = -1;
        goto cleanup;
    }

    for (int i = 0; i < usedVector->Count-1; i++)
    {
        if (usedVector->Array[i] - usedVector->Array[i + 1] < 0)
        {
            printf("Invalid sort!\n");
            retVal = -1;
            goto cleanup;
        }
    }

    retVal = VecClear(usedVector);
    if (0 != retVal)
    {
        printf("VecClear failed!\n");
        goto cleanup;
    }

    retVal = VecInsertTail(usedVector, 15);
    retVal = VecInsertTail(usedVector, 150);
    retVal = VecInsertTail(usedVector, 425);
    retVal = VecInsertTail(usedVector2, 65465);
    retVal = VecInsertTail(usedVector2, 2156);
    
    retVal = VecAppend(usedVector2, usedVector);
    if (0 != retVal)
    {
        printf("VecAppend failed!\n");
        goto cleanup;
    }

    if (usedVector->Array[3] != 65465 || usedVector->Array[4] != 2156)
    {
        printf("VecAppend failed!\n");
        goto cleanup;
    }

cleanup:
    retVal = VecDestroy(&usedVector2);
    if (NULL != usedVector)
    {
        if (0 != VecDestroy(&usedVector))
        {
            printf("VecDestroy failed!\n");
            retVal = -1;
        }
    }
    return retVal;
}