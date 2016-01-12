//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum class PerfHintLevels
{
    L1 = 1, // Critical
    L2,     // Noisy but should give you all
    VERBOSE = L2,
};

enum PerfHints : uint
{
#define PERFHINT_REASON(name, isNotOptimized, level, desc, consequences, suggestion) name,
#include "PerfHintDescriptions.h"
#undef PERFHINT_REASON
};

struct PerfHintItem
{
    LPCWSTR description;
    LPCWSTR consequences;
    LPCWSTR suggestion;
    PerfHintLevels level;
    bool isNotOptimized;
};

extern const PerfHintItem s_perfHintContainer[];

void WritePerfHint(PerfHints hint, Js::FunctionBody * functionBody, uint byteCodeOffset = Js::Constants::NoByteCodeOffset);

