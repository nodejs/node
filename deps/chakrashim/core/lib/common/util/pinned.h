//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Only need to be used if the pointer is long live within the function and doesn't escape
// NOTE: Currently, there is no way to tell the compiler a local variable need to be on the stack
// and last for a certain life time. But if we take the address of the local and pass it to a non-LTCG
// function the assignment won't be dead stored and the local will not be stack packed away.

// IMPORTANT!!! LeavePinnedScope MUST be called at the end of the desired lifetime of a local
// that you allocate using LEAVE_PINNED_SCOPE().

void EnterPinnedScope(volatile void** var);
void LeavePinnedScope();

// These MACROs enforce scoping so LEAVE must be called for each instance of ENTER
#define ENTER_PINNED_SCOPE(T, var) \
    T * var; \
    EnterPinnedScope((volatile void**)& ## var); \
    {

#define LEAVE_PINNED_SCOPE() \
    LeavePinnedScope(); \
    }


