//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template<class T, int N>
class FixedStack
{
private:
    T itemList[N];
    int curIndex;

public:
    FixedStack(): curIndex(-1)
    {
    }

    void Push(T item)
    {
        AssertMsg(curIndex < N - 1, "Stack overflow");
        if (curIndex >= N - 1)
        {
            Js::Throw::FatalInternalError();
        }
        this->itemList[++this->curIndex] = item;
    }

    T* Pop()
    {
        AssertMsg(curIndex >= 0, "Stack Underflow");
        if (curIndex < 0)
        {
            Js::Throw::FatalInternalError();
        }
        return &this->itemList[this->curIndex--];
    }

    T* Peek()
    {
        AssertMsg(curIndex >= 0, "No element present");
        if (curIndex < 0)
        {
            Js::Throw::FatalInternalError();
        }
        return & this->itemList[this->curIndex];
    }

    int Count()
    {
        return 1 + this->curIndex;
    }
};
