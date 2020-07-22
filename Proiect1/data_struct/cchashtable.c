#include "cchashtable.h"
#include "common.h"
#include <stdio.h>

int HashFunction(char* Key)
{
    int hash = 5381;
    int itr;

    while (itr = *Key++)
    {
        hash = ((hash << 5) + hash) + itr;
    }
    return hash % SIZE_OF_HASH_TABLE;
}

int EqualStrings(char* String1, char* String2)
{
    int iter1, iter2;
    iter1 = iter2 = 0;
    while (String1[iter1] != NULL && String2[iter2] != NULL && String1[iter1] == String2[iter2])
    {
        iter1++;
        iter2++;
    }
    if (String1[iter1] || String2[iter2])
    {
        return 0;
    }
    return 1;
}

int HtCreate(CC_HASH_TABLE** HashTable)
{
    if (HashTable == NULL)
    {
        return -1;
    }
    CC_HASH_TABLE* hash;
    hash = NULL;
    hash = (CC_HASH_TABLE*)malloc(sizeof(CC_HASH_TABLE));

    if (hash == NULL)
    {
        return -1;
    }
    hash->Count = 0;


    for (int i = 0; i < SIZE_OF_HASH_TABLE; i++)
    {
        hash->Table[i] = NULL;
    }
    *HashTable = hash;
    return 0;
}

int HtDestroy(CC_HASH_TABLE** HashTable)
{
    int retValue = 0;
    if (HashTable == NULL)
    {
        return -1;
    }
    for (int i = 0; i < SIZE_OF_HASH_TABLE; i++)
    {
        if ((*HashTable)->Table[i])
        {
            retValue = DestroyNodes((*HashTable)->Table[i]);
            if (retValue == -1)
            {
                return -1;
            }
        }
    }
    free(*HashTable);
    return 0;
}

int HtSetKeyValue(CC_HASH_TABLE* HashTable, char* Key, int Value)
{
    if (HashTable == NULL || Key == NULL)
    {
        return -1;
    }
    for (int i = 0; i < SIZE_OF_HASH_TABLE; i++)
    {
        if (HashTable->Table[i])
        {
            NODEH* temp;
            temp = HashTable->Table[i];

            while (temp)
            {
                if (EqualStrings(temp->Key, Key) == 1)
                {
                    return -1;
                }
                temp = temp->Next;
            }
        }
    }

    int hashKey;
    hashKey = HashFunction(Key);

    if (HashTable->Table[hashKey] == NULL)
    {
        NODEH* temp;
        temp = (NODEH*)malloc(sizeof(NODEH));
        if (temp == NULL)
        {
            return -1;
        }
        temp->Data = Value;
        temp->Next = NULL;
        temp->Key = Key;
        HashTable->Table[hashKey] = temp;
        HashTable->Count += 1;
        return 0;
    }
    else
    {
        NODEH* temp;
        NODEH* iterate;
        temp = (NODEH*)malloc(sizeof(NODEH));
        if (temp == NULL)
        {
            return -1;
        }
        iterate = HashTable->Table[hashKey];
        while (iterate->Next)
        {
            iterate = iterate->Next;
        }
        temp->Data = Value;
        temp->Next = NULL;
        temp->Key = Key;
        iterate->Next = temp;
        HashTable->Count += 1;

        return 0;
    }
}

int HtGetKeyValue(CC_HASH_TABLE* HashTable, char* Key, int* Value)
{
    if (HashTable == NULL || Key == NULL)
    {
        return -1;
    }
    int hashResult;
    hashResult = HashFunction(Key);

    if (HashTable->Table[hashResult] == NULL)
    {
        //not in table
        return -1;
    }
    else
    {
        NODEH* iterator;
        iterator = HashTable->Table[hashResult];
        while (iterator)
        {
            if (EqualStrings(iterator->Key, Key) == 1)
            {
                *Value = iterator->Data;
                return 0;
            }
            iterator = iterator->Next;
        }
        return -1;
    }
}

int HtRemoveKey(CC_HASH_TABLE* HashTable, char* Key)
{
    if (HashTable == NULL || Key == NULL)
    {
        return -1;
    }

    for (int i = 0; i < SIZE_OF_HASH_TABLE; i++)
    {
        if (HashTable->Table[i])
        {
            NODEH* temp;
            NODEH* precedent;
            temp = HashTable->Table[i];
            precedent = NULL;
            while (temp)
            {
                if (EqualStrings(temp->Key, Key) == 1)
                {
                    if (precedent)
                    {
                        precedent->Next = temp->Next;
                        HashTable->Count -= 1;
                        free(temp);
                        return 0;
                    }
                    else if (precedent == NULL && temp->Next == NULL)
                    {
                        HashTable->Count -= 1;
                        free(HashTable->Table[i]);
                        HashTable->Table[i] = NULL;
                        return 0;
                    }
                    else if (precedent == NULL && temp->Next != NULL)
                    {
                        temp = temp->Next;
                        free(HashTable->Table[i]);
                        HashTable->Table[i] = temp;
                        HashTable->Count -= 1;
                        return 0;
                    }

                }
                precedent = temp;
                temp = temp->Next;
            }
        }
    }
    return -1;
}

int HtHasKey(CC_HASH_TABLE* HashTable, char* Key)
{
    if (HashTable == NULL || Key == NULL)
    {
        return -1;
    }

    for (int i = 0; i < SIZE_OF_HASH_TABLE; i++)
    {
        if (HashTable->Table[i])
        {
            NODEH* temp;
            temp = HashTable->Table[i];
            while (temp)
            {
                if (EqualStrings(temp->Key, Key) == 1)
                {
                    return 1;
                }
                temp = temp->Next;
            }
        }
    }
    return 0;
}

int HtGetFirstKey(CC_HASH_TABLE* HashTable, CC_HASH_TABLE_ITERATOR** Iterator, char** Key)
{
    CC_HASH_TABLE_ITERATOR* iterator = NULL;

    CC_UNREFERENCED_PARAMETER(Key);

    if (NULL == HashTable)
    {
        return -1;
    }
    if (NULL == Iterator)
    {
        return -1;
    }
    if (NULL == Key)
    {
        return -1;
    }

    iterator = (CC_HASH_TABLE_ITERATOR*)malloc(sizeof(CC_HASH_TABLE_ITERATOR));
    if (NULL == iterator)
    {
        return -1;
    }

    memset(iterator, 0, sizeof(*iterator));

    iterator->HashTable = HashTable;
    for (int i = 0; i < SIZE_OF_HASH_TABLE; i++)
    {
        if (HashTable->Table[i])
        {
            iterator->Index = i;
            iterator->Current = HashTable->Table[i];
            *Iterator = iterator;
            *Key = HashTable->Table[i]->Key;
            return 0;
        }
    }

    *Iterator = iterator;
    return -2;
}

int HtGetNextKey(CC_HASH_TABLE_ITERATOR* Iterator, char** Key)
{
    if (Iterator == NULL || Key == NULL)
    {
        return -1;
    }

    if (Iterator->Current->Next)
    {
        //next in hashtable from collision
        Iterator->Current = Iterator->Current->Next;
        *Key = Iterator->Current->Key;
        return 0;
    }
    else
    {
        for (int i = Iterator->Index + 1; i < SIZE_OF_HASH_TABLE; i++)
        {
            if (Iterator->HashTable->Table[i])
            {
                Iterator->Current = Iterator->HashTable->Table[i];
                Iterator->Index = i;
                *Key = Iterator->Current->Key;
                return 0;
            }
        }
    }
    return -2;
}

int HtReleaseIterator(CC_HASH_TABLE_ITERATOR** Iterator)
{
    if (Iterator == NULL)
    {
        return -1;
    }
    free(*Iterator);
    *Iterator = NULL;
    return 0;
}

int DestroyNodes(NODEH* Start)
{
    int result;
    if (Start == NULL)
    {
        return 0;
    }
    result = DestroyNodes(Start->Next);
    free(Start);
    Start = NULL;
    return result;
}

int HtClear(CC_HASH_TABLE* HashTable)
{
    if (HashTable == NULL)
    {
        return -1;
    }
    HashTable->Count = 0;

    for (int i = 0; i < SIZE_OF_HASH_TABLE; i++)
    {
        if (HashTable->Table[i])
        {
            int result;
            result = DestroyNodes(HashTable->Table[i]);
            HashTable->Table[i] = NULL;
        }
    }
    return 0;
}

int HtGetKeyCount(CC_HASH_TABLE* HashTable)
{
    if (HashTable == NULL)
    {
        return -1;
    }
    return HashTable->Count;
}
