//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

static const char *const ExecutionModeNames[] =
{
#define EXECUTION_MODE(name) "" STRINGIZE(name) "",
#include "ExecutionModes.h"
#undef EXECUTION_MODE
};

const char *ExecutionModeName(const ExecutionMode executionMode)
{
    Assert(executionMode < ExecutionMode::Count);

    return ExecutionModeNames[static_cast<size_t>(executionMode)];
}
