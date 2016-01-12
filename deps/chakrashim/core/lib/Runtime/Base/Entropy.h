//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

/*
 * This class accumulates entropy from sources such as the process' IO counts
 * and the thread's cycle counter (i.e. number of cycles spent in user + kernel
 * mode) and is used to improve the entropy of the seed used by Math.random.
 */
class Entropy {

public:
    Entropy()
    {
        previousValue = 0;
        u.value = 0;
        currentIndex = 0;
    }

    void Initialize();
    void Add(const char *buffer, size_t size);
    void AddIoCounters();
    void AddThreadCycleTime();
    unsigned __int64 GetRand() const;

private:
    unsigned __int32 previousValue;
    union
    {
        unsigned __int32 value;
        char             array[sizeof(unsigned __int32)];
    } u;
    size_t currentIndex;

    static const uint32 kInitIterationCount;

    void BeginAdd();
    void Add(const char byteValue);
    void AddCurrentTime();
};
