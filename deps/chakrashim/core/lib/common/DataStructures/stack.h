//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <
        class T,
        class Allocator = ArenaAllocator,
        bool isLeaf = false,
        template <typename Value> class TComparer = DefaultComparer>
    class Stack
    {
    private:
        List<T, Allocator, isLeaf, Js::CopyRemovePolicy, TComparer> list;

    public:
        Stack(Allocator* alloc) : list(alloc)
        {
        }

        int Count() const { return list.Count(); }
        bool Empty() const { return Count() == 0; }

        void Clear()
        {
            list.Clear();
        }

        bool Contains(const T& item) const
        {
            return list.Contains(item);
        }

        const T& Top() const
        {
            return list.Item(list.Count() - 1);
        }

        const T& Peek(int stepsBack = 0) const
        {
            return list.Item(list.Count() - 1 - stepsBack);
        }

        T Pop()
        {
            T item = list.Item(list.Count() - 1);
            list.RemoveAt(list.Count() - 1);
            return item;
        }

        T Pop(int count)
        {
            T item = T();
            while (count-- > 0)
            {
                item = Pop();
            }
            return item;
        }

        void Push(const T& item)
        {
            list.Add(item);
        }
    };
}
