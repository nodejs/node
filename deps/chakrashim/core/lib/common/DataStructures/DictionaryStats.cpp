//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"
#if PROFILE_DICTIONARY
#include "DictionaryStats.h"

DictionaryType* DictionaryStats::dictionaryTypes = NULL;
CRITICAL_SECTION DictionaryStats::dictionaryTypesCriticalSection;

DictionaryStats* DictionaryStats::Create(const char* name, uint bucketCount)
{
    if (!Js::Configuration::Global.flags.IsEnabled(Js::ProfileDictionaryFlag) ||
        Js::Configuration::Global.flags.ProfileDictionary < 0)
        return NULL;

    return ::new DictionaryStats(name, bucketCount);
}

DictionaryStats* DictionaryStats::Clone()
{
    DictionaryStats* cloned = NoCheckHeapNew(DictionaryStats, pName, initialSize);
    cloned->finalSize = finalSize;
    cloned->countOfEmptyBuckets = countOfEmptyBuckets;
    cloned->countOfResize = countOfResize;
    cloned->itemCount = itemCount;
    cloned->maxDepth = maxDepth;
    cloned->lookupCount = lookupCount;
    cloned->collisionCount = collisionCount;
    cloned->lookupDepthTotal = lookupDepthTotal;
    cloned->maxLookupDepth = maxLookupDepth;
    cloned->pName = pName;
    return cloned;
}

DictionaryStats::DictionaryStats(const char* name, uint bucketCount)
    :
    initialSize(bucketCount),
    finalSize(bucketCount),
    countOfEmptyBuckets(bucketCount),
    countOfResize(0),
    itemCount(0),
    maxDepth(0),
    lookupCount(0),
    collisionCount(0),
    lookupDepthTotal(0),
    maxLookupDepth(0),
    pNext(NULL),
    pName(NULL)
{
    if(dictionaryTypes == NULL)
    {
        InitializeCriticalSection(&DictionaryStats::dictionaryTypesCriticalSection);
    }
    EnterCriticalSection(&DictionaryStats::dictionaryTypesCriticalSection);
    DictionaryType* type = NULL;
    // See if we already created instance(s) of this type
    DictionaryType* current = dictionaryTypes;
    while(current)
    {
        if (strncmp(name, current->name, _countof(current->name)-1) == 0)
        {
            type = current;
            break;
        }
        current = current->pNext;
    }

    if (!type)
    {
        // We haven't seen this type before so add a new entry for it
        type = NoCheckHeapNew(DictionaryType);
        type->pNext = dictionaryTypes;
        dictionaryTypes = type;
        type->instancesCount = 0;
        strncpy_s(type->name, name, _countof(type->name)-1);
        type->name[sizeof(type->name)-1]='\0';
    }
    LeaveCriticalSection(&dictionaryTypesCriticalSection);
    // keep a pointer to the name in case we are asked to clone ourselves
    pName = type->name;

    // Add ourself in the list
    pNext = type->instances;
    type->instances = this;
    ++(type->instancesCount);
}

void DictionaryStats::Resize(uint newSize, uint emptyBucketCount)
{
    finalSize = newSize;
    countOfEmptyBuckets = emptyBucketCount;
    ++countOfResize;
}

void DictionaryStats::Insert(uint depth)
{
    ++itemCount;
    if (maxDepth < depth)
        maxDepth = depth;
    if (depth == 1 && countOfEmptyBuckets > 0)
        --countOfEmptyBuckets;
}

void DictionaryStats::Remove(bool isBucketEmpty)
{
    if (itemCount > 0)
        --itemCount;
    if (isBucketEmpty)
        ++countOfEmptyBuckets;
}

void DictionaryStats::Lookup(uint depth)
{
    // Note, lookup and collision math only works out if depth is 0-based.
    // I.e., depth of 1 means there was 1 collision and the lookup found key at second item in the bucket
    lookupCount += 1;
    lookupDepthTotal += depth;
    if (depth > 0)
        collisionCount += 1;
    if (maxLookupDepth < depth)
        maxLookupDepth = depth;
}

void DictionaryStats::OutputStats()
{
    if (!dictionaryTypes)
        return;

    EnterCriticalSection(&DictionaryStats::dictionaryTypesCriticalSection);
    DictionaryType* current = dictionaryTypes;
    Output::Print(L"PROFILE DICTIONARY\n");
    Output::Print(L"%8s  %13s  %13s  %13s  %13s  %13s  %13s  %13s  %14s  %14s  %13s  %13s  %13s    %s\n", L"Metric",L"StartSize", L"EndSize", L"Resizes", L"Items", L"MaxDepth", L"EmptyBuckets", L"Lookups", L"Collisions", L"AvgLookupDepth", L"AvgCollDepth", L"MaxLookupDepth", L"Instances", L"Type");
    while(current)
    {
        DictionaryType *type = current;

        DictionaryStats *instance = type->instances;
        double size = 0, max_size = 0;
        double endSize = 0, max_endSize = 0;
        double resizes = 0, max_resizes = 0;
        double items = 0, max_items = 0;
        double depth = 0, max_depth = 0;
        double empty = 0, max_empty = 0;
        double lookups = 0, max_lookups = 0;
        double collisions = 0, max_collisions = 0;
        double avglookupdepth = 0, max_avglookupdepth = 0;
        double avgcollisiondepth = 0, max_avgcollisiondepth = 0;
        double maxlookupdepth = 0, max_maxlookupdepth = 0;

        bool dumpInstances = false;
        //if(strstr(type->name, "SimpleDictionaryPropertyDescriptor") != nullptr)
        //{
        //    dumpInstances = true;
        //}
        while(instance)
        {
            ComputeStats(instance->initialSize, size, max_size);
            ComputeStats(instance->finalSize, endSize, max_endSize);
            ComputeStats(instance->countOfResize, resizes, max_resizes);
            ComputeStats(instance->itemCount, items, max_items);
            ComputeStats(instance->maxDepth, depth, max_depth);
            ComputeStats(instance->countOfEmptyBuckets, empty, max_empty);
            ComputeStats(instance->lookupCount, lookups, max_lookups);
            ComputeStats(instance->collisionCount, collisions, max_collisions);
            if (instance->lookupCount > 0)
            {
                ComputeStats((double)instance->lookupDepthTotal / (double)instance->lookupCount, avglookupdepth, max_avglookupdepth);
            }
            if (instance->collisionCount > 0)
            {
                ComputeStats((double)instance->lookupDepthTotal / (double)instance->collisionCount, avgcollisiondepth, max_avgcollisiondepth);
            }
            ComputeStats(instance->maxLookupDepth, maxlookupdepth, max_maxlookupdepth);

            if(dumpInstances)
            {
                double avgld = 0.0;
                double avgcd = 0.0;
                if (instance->lookupCount > 0)
                {
                    avgld = (double)instance->lookupDepthTotal / (double)instance->lookupCount;
                    avgcd = (double)instance->lookupDepthTotal / (double)instance->collisionCount;
                }
                Output::Print(L"%8s  %13d  %13d  %13d  %13d  %13d  %13d  %13d  %14d  %14.2f  %13.2f  %13d \n",
                    L"INS:",
                    instance->initialSize, instance->finalSize, instance->countOfResize,
                    instance->itemCount, instance->maxDepth, instance->countOfEmptyBuckets,
                    instance->lookupCount, instance->collisionCount, avgld, avgcd,
                    instance->maxLookupDepth);
            }
            instance = instance->pNext;
        }

        if (max_depth >= Js::Configuration::Global.flags.ProfileDictionary)
        {
            Output::Print(L"%8s  %13.0f  %13.0f  %13.2f  %13.0f  %13.2f  %13.0f  %13.0f  %14.0f  %14.2f  %13.2f  %13.2f  %13d    %S\n", L"AVG:",
                size/type->instancesCount, endSize/type->instancesCount, resizes/type->instancesCount, items/type->instancesCount,
                depth/type->instancesCount, empty/type->instancesCount, lookups/type->instancesCount, collisions/type->instancesCount,
                avglookupdepth/type->instancesCount, avgcollisiondepth/type->instancesCount, maxlookupdepth/type->instancesCount, type->instancesCount, type->name);
            Output::Print(L"%8s  %13.0f  %13.0f  %13.2f  %13.0f  %13.2f  %13.0f  %13.0f  %14.0f  %14.2f  %13.2f  %13.2f  %13d    %S\n\n", L"MAX:",
                max_size, max_endSize, max_resizes, max_items, max_depth, max_empty, max_lookups, max_collisions, max_avglookupdepth,
                max_avgcollisiondepth, max_maxlookupdepth, type->instancesCount, type->name);

        }
        current = current->pNext;
    }
    Output::Print(L"====================================================================================\n");
    ClearStats();
    LeaveCriticalSection(&DictionaryStats::dictionaryTypesCriticalSection);
    DeleteCriticalSection(&DictionaryStats::dictionaryTypesCriticalSection);
}

void DictionaryStats::ComputeStats(uint input, double &total, double &max)
{
    total += input;
    if (input > max)
        max = input;
}

void DictionaryStats::ComputeStats(double input, double &total, double &max)
{
    total += input;
    if (input > max)
        max = input;
}

void DictionaryStats::ClearStats()
{
    // Clear the collection since we already reported on what we already collected
    DictionaryType* current = dictionaryTypes;
    while(current)
    {
        DictionaryType *type = current;
        DictionaryStats *pNext = type->instances;
        while(pNext)
        {
            DictionaryStats *pCurrent = pNext;
            pNext = pNext->pNext;
            delete pCurrent;
        }
        current = current->pNext;
        delete type;
    }
    dictionaryTypes = NULL;
}
#endif
