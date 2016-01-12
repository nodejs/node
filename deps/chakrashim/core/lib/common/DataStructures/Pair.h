//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template<class TFirst, class TSecond, template<class TValue> class Comparer = DefaultComparer>
    class Pair
    {
    private:
        TFirst first;
        TSecond second;
    #if DBG
        bool initialized;
    #endif

    public:
        Pair()
        #if DBG
            : initialized(false)
        #endif
        {
            Assert(!IsValid());
        }

        Pair(const TFirst &first, const TSecond &second)
            : first(first),
            second(second)
        #if DBG
            ,
            initialized(true)
        #endif
        {
            Assert(IsValid());
        }

    #if DBG
    private:
        bool IsValid() const
        {
            return initialized;
        }
    #endif

    public:
        const TFirst &First() const
        {
            Assert(IsValid());
            return first;
        }

        const TSecond &Second() const
        {
            Assert(IsValid());
            return second;
        }

    public:
        bool operator ==(const Pair &other) const
        {
            return Comparer<TFirst>::Equals(first, other.first) && Comparer<TSecond>::Equals(second, other.second);
        }

        operator hash_t() const
        {
            return Comparer<TFirst>::GetHashCode(first) + Comparer<TSecond>::GetHashCode(second);
        }
    };
}
