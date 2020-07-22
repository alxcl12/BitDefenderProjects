#include "cctree.h"
#include "common.h"

int TreeCreate(CC_TREE **Tree)
{
    CC_TREE* tree = NULL;
    if (NULL == Tree)
    {
        return -1;
    }

    tree = (CC_TREE*)malloc(sizeof(CC_TREE));

    if (tree == NULL)
    {
        return -1;
    }

    tree->Left = tree->Right = NULL;
    tree->Count = tree->Height = 0;

    *Tree = tree;
    
    return 0;
}

int GetHeight(CC_TREE* Tree)
{
    //gets height of given node, 0 if null
    if (NULL == Tree || Tree->Count == 0)
    {
        return 0;
    }
    else return Tree->Height;
}

int Max(int First, int Second)
{
    //returns max of 2 ints
    if (First > Second) 
    {
        return First;
    }
    else
    {
        return Second;
    }
}

int GetBalance(CC_TREE* Tree)
{
    //returns balance of current node
    if (Tree == NULL)
    {
        return 0;
    }
    else
    {
        return GetHeight(Tree->Left) - GetHeight(Tree->Right);
    }
}

int TreeDestroy(CC_TREE** Tree)
{
    int retValue;

    if (Tree == NULL)
    {
        return -1;
    }
    else if ((*Tree))
    {
        if ((*Tree)->Left)
        {
            retValue = TreeDestroy(&(*Tree)->Left);
            if (retValue == -1)
            {
                return -1;
            }
            free((*Tree)->Left);
            (*Tree)->Left = NULL;
        }
        if ((*Tree)->Right)
        {
            retValue = TreeDestroy(&(*Tree)->Right);
            if (retValue == -1)
            {
                return -1;
            }
            free((*Tree)->Right);
            (*Tree)->Right = NULL;
            return 0;
        }
        return 0;
    }
    else
    {
        return 0;
    }
}

void LeftRotate(CC_TREE* Tree)
{
    // perform left rotate on given node
    CC_TREE *rightRoot = Tree->Right;
    CC_TREE* rightRightRoot = rightRoot->Left;
    CC_TREE* copy = NULL;

    copy = (CC_TREE*)malloc(sizeof(CC_TREE));

    if (copy)
    {
        copy->Height = Tree->Height;
        copy->Left = Tree->Left;
        copy->Count = Tree->Count;
        copy->Data = Tree->Data;
        copy->Right = Tree->Right;
       
        rightRoot->Left = copy;
        copy->Right = rightRightRoot;

        copy->Height = Max(GetHeight(copy->Left), GetHeight(copy->Right)) + 1;
        rightRoot->Height = Max(GetHeight(rightRoot->Left), GetHeight(rightRoot->Right)) + 1;
        
        Tree->Count = rightRoot->Count;
        Tree->Data = rightRoot->Data;
        Tree->Height = rightRoot->Height;
        Tree->Left = rightRoot->Left;
        Tree->Right = rightRoot->Right;
    }  
}

void RightRotate(CC_TREE *Tree)
{
    //perform right rotate on given node
    CC_TREE *leftRoot = Tree->Left;
    CC_TREE *leftLeftRoot = leftRoot->Right;
    CC_TREE* copy = NULL;

    copy = (CC_TREE*)malloc(sizeof(CC_TREE));
    
    if (copy)
    {
        copy->Height = Tree->Height;
        copy->Left = Tree->Left;
        copy->Count = Tree->Count;
        copy->Data = Tree->Data;
        copy->Right = Tree->Right;

        copy->Left = leftLeftRoot;
        leftRoot->Right = copy;

        copy->Height = Max(GetHeight(copy->Left), GetHeight(copy->Right)) + 1;
        leftRoot->Height = Max(GetHeight(leftRoot->Left), GetHeight(leftRoot->Right)) + 1;

        Tree->Count = leftRoot->Count;
        Tree->Data = leftRoot->Data;
        Tree->Height = leftRoot->Height;
        Tree->Left = leftRoot->Left;
        Tree->Right = leftRoot->Right;
    }  
}

int TreeInsert(CC_TREE *Tree, int Value)
{
    int retValue1, retValue2;
    retValue1 = retValue2 = 0;
    if (Tree == NULL) 
    {
        return -1;
    }

    if (Tree->Count == 0)
    {
        Tree->Data = Value;
        Tree->Height = 1;
        Tree->Left = Tree->Right = NULL;
        Tree->Count = 1; 
    }
    else if (Value < Tree->Data)
    {
        if (Tree->Left == NULL)
        {
            CC_TREE* tree;
            tree = (CC_TREE*)malloc(sizeof(CC_TREE));
            if (tree == NULL)
            {
                return -1;
            }

            tree->Count = tree->Height = 0;
            tree->Left = tree->Right = NULL;
            Tree->Left = tree;
        }
        
        retValue1 = TreeInsert(Tree->Left, Value);
    }
    else if (Value > Tree->Data)
    {
        if (Tree->Right == NULL) 
        {
            CC_TREE* tree;
            tree = (CC_TREE*)malloc(sizeof(CC_TREE));
            if (tree == NULL)
            {
                return -1;
            }

            tree->Count = tree->Height = 0;
            tree->Left = tree->Right = NULL;
            Tree->Right = tree;
        }
        
        retValue2 = TreeInsert(Tree->Right, Value);
    }
    else if (Value == Tree->Data)
    {
        Tree->Count += 1;
    }

    Tree->Height = 1 + Max(GetHeight(Tree->Left), GetHeight(Tree->Right));
    int balance;
    balance = GetBalance(Tree);

    if (Tree->Left)
    {
        if (balance > 1 && Value < Tree->Left->Data)
        {
            //LL
            RightRotate(Tree);
            retValue1 = 0;
            retValue2 = 0;
        }

        if (balance > 1 && Value > Tree->Left->Data)
        {
            //LR
            LeftRotate(Tree->Left);
            RightRotate(Tree);
            retValue1 = 0;
            retValue2 = 0;
        }
    }
   
    if (Tree->Right)
    {
        if (balance < -1 && Value > Tree->Right->Data)
        {
            //RR
            LeftRotate(Tree);
            retValue1 = 0;
            retValue2 = 0;
        }

        if (balance < -1 && Value < Tree->Right->Data)
        {
            //RL
            RightRotate(Tree->Right);
            LeftRotate(Tree);
            retValue1 = 0;
            retValue2 = 0;
        }
    }
    
    if (retValue1 == retValue2 == 0)
    {
        return 0;
    }
}

CC_TREE* MinValueNode(CC_TREE* Tree)
{
    //returns min value node of subtree with root given
    CC_TREE* temp;
    temp = Tree;

    while (temp->Left != NULL)
    {
        temp = temp->Left;
    }

    return temp;
}

CC_TREE* DeleteNode(CC_TREE* Tree, int Value)
{
    if (Tree == NULL)
    {
        Tree = (CC_TREE*)malloc(sizeof(CC_TREE));
        Tree->Count = -512; //dummy value used to signal that node was not in tree
        return Tree;
    }
    else if (Value < Tree->Data)
    {
        Tree->Left = DeleteNode(Tree->Left, Value);
        if (Tree->Left)
        {
            if (Tree->Left->Count == -512)
            {
                //value was not in tree
                free(Tree->Left);
                Tree->Left = NULL;
                return Tree;
            }
        }
    }
    else if (Value > Tree->Data)
    {
        Tree->Right = DeleteNode(Tree->Right, Value);
        if (Tree->Right)
        {
            if (Tree->Right->Count == -512)
            {
                //value was not in tree
                free(Tree->Right);
                Tree->Right = NULL;
                return Tree;
            }
        }
    }
    else if (Value == Tree->Data)
    {
        if (Tree->Right == NULL || Tree->Left == NULL)
        {
            if (Tree->Count == 1)
            {
                //1 or none child
                CC_TREE* temp;
                if (Tree->Right)
                {
                    temp = Tree->Right;
                }
                else
                {
                    temp = Tree->Left;
                }

                if (temp)
                {
                    Tree->Count = temp->Count;
                    Tree->Data = temp->Data;
                    Tree->Height = temp->Height;
                    Tree->Left = Tree->Right = NULL;
                    free(temp);
                }
                else
                {
                    free(Tree);
                    Tree = NULL;
                }
            }
            else
            {
                Tree->Count -= 1;
            }
        }
        else
        {
            if (Tree->Count == 1)
            {
                CC_TREE* temp = MinValueNode(Tree->Right);
                Tree->Data = temp->Data;
                Tree->Count = temp->Count;

                Tree->Right = DeleteNode(Tree->Right, temp->Data);
                if (Tree->Right)
                {
                    if (Tree->Right->Count == -512)
                    {
                        //value was not in tree
                        free(Tree->Right);
                        Tree->Right = NULL;
                        return Tree;
                    }
                }
            }
            else
            {
                Tree->Count -= 1;
            }
        }
    }
    if (Tree)
    {
        Tree->Height = 1 + Max(GetHeight(Tree->Left), GetHeight(Tree->Right));

        int balance;
        balance = GetBalance(Tree);

        if (Tree->Left)
        {
            if (balance > 1 && GetBalance(Tree->Left) >= 0)
            {
                RightRotate(Tree);
                return Tree;
            }
            if (balance > 1 && GetBalance(Tree->Left) < 0)
            {
                LeftRotate(Tree->Left);
                RightRotate(Tree);
                return Tree;
            }
        }

        if (Tree->Right)
        {
            if (balance < -1 && GetBalance(Tree->Right) <= 0)
            {
                LeftRotate(Tree);
                return Tree;
            }
            if (balance < -1 && GetBalance(Tree->Right) > 0)
            {
                RightRotate(Tree->Right);
                LeftRotate(Tree);
                return Tree;
            }
        }
    }
    return Tree;
}

int TreeRemove(CC_TREE *Tree, int Value)
{
    int retVal;
    retVal = 0;
    if (Tree == NULL)
    {
        return -1;
    }
    else if (Value < Tree->Data)
    {
        Tree->Left = DeleteNode(Tree->Left, Value);
        if (Tree->Left)
        {
            if (Tree->Left->Count == -512)
            {
                //value was not in tree
                free(Tree->Left);
                Tree->Left = NULL;
                return -1;
            }
        }
    }
    else if (Value > Tree->Data)
    {
        Tree->Right = DeleteNode(Tree->Right, Value);
        if (Tree->Right)
        {
            if (Tree->Right->Count == -512)
            {
                //value was not in tree
                free(Tree->Right);
                Tree->Right = NULL;
                return -1;
            }
        }
    }
    else if (Value == Tree->Data)
    {
        if (Tree->Right == NULL || Tree->Left == NULL)
        {
            if (Tree->Count == 1)
            {
                //1 or none child
                CC_TREE* temp;
                if (Tree->Right)
                {
                    temp = Tree->Right;
                }
                else
                {
                    temp = Tree->Left;
                }

                if (temp)
                {
                    Tree->Count = temp->Count;
                    Tree->Data = temp->Data;
                    Tree->Height = temp->Height;
                    Tree->Left = Tree->Right = NULL;
                    free(temp);
                }
                else
                {
                    free(Tree);
                    Tree = NULL;
                }
                return 0;
            }
            else
            {
                Tree->Count -= 1;
                return 0;
            }            
        }
        else
        {
            if (Tree->Count == 1)
            {
                CC_TREE* temp = MinValueNode(Tree->Right);
                Tree->Data = temp->Data;
                Tree->Count = temp->Count;

                Tree->Right = DeleteNode(Tree->Right, temp->Data);
                if (Tree->Right)
                {
                    if (Tree->Right->Count == -512)
                    {
                        //value was not in tree
                        free(Tree->Right);
                        Tree->Right = NULL;
                        return -1;
                    }
                }
            }
            else
            {
                Tree->Count -= 1;
                return 0;
            }
        }
    }

    Tree->Height = 1 + Max(GetHeight(Tree->Left), GetHeight(Tree->Right));

    int balance;
    balance = GetBalance(Tree);

    if (Tree->Left)
    {
        if (balance > 1 && GetBalance(Tree->Left) >= 0)
        {
            RightRotate(Tree);
            retVal = 0;
        }
        if (balance > 1 && GetBalance(Tree->Left) < 0)
        {
            LeftRotate(Tree->Left);
            RightRotate(Tree);
            retVal = 0;
        }
    }
    
    if (Tree->Right)
    {  
        if (balance < -1 && GetBalance(Tree->Right) <= 0)
        {
            LeftRotate(Tree);
            retVal = 0;
        }  
        if (balance < -1 && GetBalance(Tree->Right) > 0)
        {
            RightRotate(Tree->Right);
            LeftRotate(Tree);
            retVal = 0;
        }
    }

    if (retVal == 0)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int TreeContains(CC_TREE *Tree, int Value)
{ 
    if (Tree == NULL)
    {
        return 0;
    }
    if (Tree->Data == Value)
    {
        return 1;
    }
    else if (Tree->Data > Value)
    {
        return TreeContains(Tree->Left, Value);
    }
    else if (Tree->Data < Value)
    {
        return TreeContains(Tree->Right, Value);
    }
}

int TreeGetCount(CC_TREE *Tree)
{
    int totalCount = 0;
    if (Tree == NULL)
    {
        return -1;
    }

    if (Tree->Left)
    {
        totalCount += Tree->Count;
        totalCount += TreeGetCount(Tree->Left);
    }
    if (Tree->Right)
    {
        totalCount += Tree->Count;
        totalCount += TreeGetCount(Tree->Right);
    }

    return totalCount;
}

int TreeGetHeight(CC_TREE *Tree)
{
    if (Tree == NULL)
    {
        return -1;
    }

    return Tree->Height - 1; 
}

int TreeClear(CC_TREE *Tree)
{
    int retValue;
    if (Tree == NULL)
    {
        return 0;
    }
    else
    {
        retValue = TreeClear(Tree->Left);
        retValue = TreeClear(Tree->Right);
        Tree->Count = Tree->Data = Tree->Height = 0;
        return 0;
    }
}

int TreeGetNthPreorder(CC_TREE *Tree, int Index, int *Value)
{
    static int indexCurr = 0;
    int retValue;
    retValue = 0;

    if (Tree == NULL)
    {
        return -1;
    }
    else
    {
        if (indexCurr <= Index) 
        {
            indexCurr += 1;

            if (indexCurr == Index)
            {
                *Value = Tree->Data;
            }

            if (Tree->Left)
            {
                retValue = TreeGetNthPreorder(Tree->Left, Index, Value);
            }
            
            if (Tree->Right)
            {
                retValue = TreeGetNthPreorder(Tree->Right, Index, Value);
            }           
        }
        return retValue;
    }
}

int TreeGetNthInorder(CC_TREE *Tree, int Index, int *Value)
{
    static int indexCurr = 0;
    int retValue1, retValue2;
    retValue1 = retValue2 = 0;

    if (Tree == NULL)
    {
        return -1;
    }
    else
    {
        if (indexCurr <= Index)
        {
            if (Tree->Left)
            {
                retValue1 = TreeGetNthInorder(Tree->Left, Index, Value);           
            }

            indexCurr += 1;

            if (indexCurr == Index)
            {
                *Value = Tree->Data;
            }

            if (Tree->Right)
            {
                retValue2 = TreeGetNthInorder(Tree->Right, Index, Value);
            }            
        }

        return retValue1 * retValue2;
    }
}

int TreeGetNthPostorder(CC_TREE *Tree, int Index, int *Value)
{
    static int indexCurr = 0;
    int retValue;
    retValue = 0;

    if (Tree == NULL)
    {
        return -1;
    }
    else
    {
        if (indexCurr <= Index)
        {
            if (Tree->Left)
            {
                retValue = TreeGetNthPostorder(Tree->Left, Index, Value);
            }

            if (Tree->Right)
            {
                retValue = TreeGetNthPostorder(Tree->Right, Index, Value);
            }
            indexCurr += 1;

            if (indexCurr == Index)
            {
                *Value = Tree->Data;
            }
        }
        return retValue;
    }
}

