//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


struct PrimePolicy
{
    __inline static uint GetBucket(hash_t hashCode, int size)
    {
        uint targetBucket = hashCode % size;
        return targetBucket;
    }

    __inline static uint GetSize(uint capacity)
    {
        return GetPrime(capacity);
    }

private:
    static bool IsPrime(uint candidate);
    static uint GetPrime(uint min);
};

struct PowerOf2Policy
{
    __inline static uint GetBucket(hash_t hashCode, int size)
    {
        AssertMsg(Math::IsPow2(size), "Size is not a power of 2.");
        uint targetBucket = hashCode & (size-1);
        return targetBucket;
    }

    /// Returns a size that is power of 2 and
    /// greater than specified capacity.
    __inline static uint GetSize(size_t minCapacity_t)
    {
        AssertMsg(minCapacity_t <= MAXINT32, "the next higher power of 2  must fit in uint32");
        uint minCapacity = static_cast<uint>(minCapacity_t);

        if(minCapacity <= 0)
        {
            return 4;
        }

        if (Math::IsPow2(minCapacity))
        {
            return minCapacity;
        }
        else
        {
            return 1 << (Math::Log2(minCapacity) + 1);
        }
    }
};

#ifndef JD_PRIVATE
template <class SizePolicy, uint averageChainLength = 2, uint growthRateNumerator = 2, uint growthRateDenominator = 1, uint minBucket = 4>
struct DictionarySizePolicy
{
    CompileAssert(growthRateNumerator > growthRateDenominator);
    CompileAssert(growthRateDenominator != 0);
    __inline static uint GetBucket(hash_t hashCode, uint bucketCount)
    {
        return SizePolicy::GetBucket(hashCode, bucketCount);
    }
    __inline static uint GetNextSize(uint minCapacity)
    {
        uint nextSize = minCapacity * growthRateNumerator / growthRateDenominator;
        return (growthRateDenominator != 1 && nextSize <= minCapacity)? minCapacity + 1 : nextSize;
    }
    __inline static uint GetBucketSize(uint size)
    {
        if (minBucket * averageChainLength >= size)
        {
            return SizePolicy::GetSize(minBucket);
        }
        return SizePolicy::GetSize((size + (averageChainLength - 1)) / averageChainLength);
    }
};

typedef DictionarySizePolicy<PrimePolicy> PrimeSizePolicy;
typedef DictionarySizePolicy<PowerOf2Policy> PowerOf2SizePolicy;
#endif
