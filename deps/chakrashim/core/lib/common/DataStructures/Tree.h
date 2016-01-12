//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
///
/// class TreeNode
///
/// General DataStructure for an N-ary tree.
///
///----------------------------------------------------------------------------
///----------------------------------------------------------------------------

template<class T, int N>
class TreeNode
{

// Data
private:
    T                   value;
    TreeNode *          children[N];
    TreeNode<T, N> *    parent;

// Constructor
public:
    TreeNode(TreeNode<T, N> * parent = NULL)
    {
        this->parent    = parent;
        for(int i = 0; i < N; i++)
        {
            this->children[i] = NULL;
        }
    }

// Methods
public:
    bool ChildExistsAt(int i)
    {
        return NULL != this->children[i];
    }

    TreeNode<T, N> * GetChildAt(int i)
    {
        return this->children[i];
    }

    void SetChildAt(int i, TreeNode<T, N> *node)
    {
        this->children[i]   = node;
    }

    TreeNode<T, N> * GetParent()
    {
         return this->parent;
    }

    void SetParent(TreeNode<T, N>* parent)
    {
        this->parent = parent;
    }

    T * GetValue()
    {
        return &this->value;
    }

    void SetValue(const T value)
    {
        this->value = value;
    }

};
