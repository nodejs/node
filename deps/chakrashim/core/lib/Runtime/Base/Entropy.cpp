//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

const uint32 Entropy::kInitIterationCount = 3;

void Entropy::BeginAdd()
{
    previousValue = u.value;
}

void Entropy::AddCurrentTime()
{
    LARGE_INTEGER time = {0};
    QueryPerformanceCounter(&time);

    if (time.LowPart)
        u.value ^= time.LowPart;
}

void Entropy::Add(const char byteValue)
{
    if (byteValue)
    {
        u.array[currentIndex++] ^= byteValue;
        currentIndex %= sizeof(unsigned __int32);
    }
}

/* public API */

void Entropy::Initialize()
{
    for (uint32 times = 0; times < Entropy::kInitIterationCount; times++)
    {
        AddIoCounters();
        AddThreadCycleTime();
    }

    AddCurrentTime();
}

void Entropy::Add(const char *buffer, size_t size)
{
    BeginAdd();

    for (size_t index = 0; index < size; index++)
    {
        Add(buffer[index]);
    }
}

void Entropy::AddIoCounters()
{
    IO_COUNTERS ioc = {0};
    if (GetProcessIoCounters(GetCurrentProcess(), &ioc))
    {
        Add((char *)&ioc.ReadOperationCount,  sizeof(ioc.ReadOperationCount));
        Add((char *)&ioc.WriteOperationCount, sizeof(ioc.WriteOperationCount));
        Add((char *)&ioc.OtherOperationCount, sizeof(ioc.OtherOperationCount));
        Add((char *)&ioc.ReadTransferCount,   sizeof(ioc.ReadTransferCount));
        Add((char *)&ioc.WriteTransferCount,  sizeof(ioc.WriteTransferCount));
        Add((char *)&ioc.OtherTransferCount,  sizeof(ioc.OtherTransferCount));
    }

    AddCurrentTime();
}

void Entropy::AddThreadCycleTime()
{
    LARGE_INTEGER threadCycleTime = {0};
    QueryThreadCycleTime(GetCurrentThread(), (PULONG64)&threadCycleTime);
    Add((char *)&threadCycleTime.LowPart, sizeof(threadCycleTime.LowPart));

    AddCurrentTime();
}


unsigned __int64 Entropy::GetRand() const
{
    return (((unsigned __int64)previousValue) << 32) | u.value;
}
