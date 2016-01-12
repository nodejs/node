//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace regex
{
    template <class T, bool partitionFromMiddle> class QuickSort
    {
    public:
        static void Sort(T* low, T* high, Comparer<T> * comp)
        {
            if ((low == NULL)||(high == NULL))
            {
                return;
            }
            if (high > low)
            {
                T* pivot = Partition(low, high, comp);
                Sort(low, pivot-1, comp);
                Sort(pivot+1, high, comp);
            }
        }

    private:
        static T* Partition(T* l, T* r, Comparer<T> * comp)
        {
            if (partitionFromMiddle)
            {
                // Swap middle value to end to use as partition value
                T* mid = l + ((r - l) / 2);
                swap(*mid, *r);

            }

            T* i = l-1;
            T* j = r;
            T v = *r;
            for (;;)
            {
                while (comp->Compare(*(++i), v) < 0) ;
                while (comp->Compare(v, *(--j)) < 0) if (j == l) break;
                if (i >= j) break;
                swap(*i, *j);
            }
            swap(*i, *r);
            return i;
        }

        inline static void swap(T& x, T& y)
        {
            T temp = x;
            x = y;
            y = temp;
        }
    };
}

namespace JsUtil
{
    template <class T, class TComparer> class QuickSort
    {
    public:
        static void Sort(T* low, T* high)
        {
            if ((low == NULL)||(high == NULL))
            {
                return;
            }
            if (high > low)
            {
                T* pivot = Partition(low, high);
                Sort(low, pivot-1);
                Sort(pivot+1, high);
            }
        }

    private:
        static T* Partition(T* l, T* r)
        {
            T* i = l-1;
            T* j = r;
            T v = *r;
            for (;;)
            {
                while (TComparer::Compare(*(++i), v) < 0) ;
                while (TComparer::Compare(v, *(--j)) < 0) if (j == l) break;
                if (i >= j) break;
                swap(*i, *j);
            }
            swap(*i, *r);
            return i;
        }

        inline static void swap(T& x, T& y)
        {
            T temp = x;
            x = y;
            y = temp;
        }
    };
}
