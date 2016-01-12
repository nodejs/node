//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

// strings for builtin names
#define HASH_NAME(name, hashCS, hashCI) \
    const StaticSym g_ssym_##name = \
    { \
        hashCS, \
        sizeof(#name) - 1, \
        OLESTR(#name) \
    };
#include "objnames.h"
#undef HASH_NAME


