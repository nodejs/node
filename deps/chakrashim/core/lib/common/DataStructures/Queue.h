//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <class T, class Allocator>
    class Queue
    {
    private:
        DList<T, Allocator> list;

    public:
        Queue(Allocator* alloc) : list(alloc)
        {
        }

        bool Empty() const
        {
            return list.Empty();
        }

        void Enqueue(const T& item)
        {
            list.Append(item);
        }

        T Dequeue()
        {
            T item = list.Head();
            list.RemoveHead();
            return item;
        }

        void Clear()
        {
            list.Clear();
        }
    };
}
