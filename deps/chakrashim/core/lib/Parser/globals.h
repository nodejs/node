//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct StaticSym
{
    ulong luHash;
    ulong cch;
    OLECHAR sz[];
};

// Builtin symbols.
#define HASH_NAME(name, hashCS, hashCI) extern const StaticSym g_ssym_##name;
#include "objnames.h"
#undef HASH_NAME

