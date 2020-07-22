#pragma once

#include "stdlib.h"

#define CC_UNREFERENCED_PARAMETER(X) X

typedef struct _NODE { //struct used for node in simple linked list of ints
    int Data;
    struct _NODE* Next;
}NODE;

typedef struct _NODEH { //struct used for node in hashtable
    int Data;
    struct _NODEH* Next;
    char* Key;
}NODEH;


