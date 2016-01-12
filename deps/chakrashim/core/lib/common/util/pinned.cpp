//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "pinned.h"

#define UNREFERENCED_PARAMETER(x) (x)

void EnterPinnedScope(volatile void** var)
{
    UNREFERENCED_PARAMETER(var);
    return;
}

void LeavePinnedScope()
{
    return;
}
