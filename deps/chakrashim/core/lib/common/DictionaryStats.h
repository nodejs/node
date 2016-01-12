//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#if PROFILE_DICTIONARY

class DictionaryStats;

struct DictionaryType
{
    int instancesCount;
    DictionaryType* pNext;
    char name[256];
    DictionaryStats* instances;
};

class DictionaryStats
{
public:
    static DictionaryStats* Create(const char* name, uint initialSize);
    static void OutputStats();
private:
    static void ComputeStats(uint input, double &total, double &max);
    static void ComputeStats(double input, double &total, double &max);
    static void ClearStats();

    static DictionaryType* dictionaryTypes;

    static CRITICAL_SECTION dictionaryTypesCriticalSection;

public:
    void Resize(uint newSize, uint emptyBucketCount);
    void Insert(uint depth);
    void Remove(bool isBucketEmpty);
    void Lookup(uint depth);
    DictionaryStats* Clone();

    DictionaryStats* pNext;
private:
    DictionaryStats(const char* name, uint initialSize);

    uint initialSize;
    uint finalSize;
    uint countOfEmptyBuckets;
    uint countOfResize;
    uint itemCount;
    uint maxDepth;
    uint lookupCount;
    uint collisionCount;
    uint lookupDepthTotal;
    uint maxLookupDepth;
    char* pName;
};
#endif
