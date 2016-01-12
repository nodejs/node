//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

// Method is expected to be implemented to link with codex.lib
// We have separate implementations for Chakra and IE
void CodexAssert(bool condition)
{
    Assert(condition);
}

