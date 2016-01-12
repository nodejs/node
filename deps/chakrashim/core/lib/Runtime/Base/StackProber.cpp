//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

/*
 * The following diagram depicts the layout of the user-mode stack. To the left is what the stack
 * looks like to the OS. To the right are the logical partitions Chakra divides the usable stack into.
 * The goal of SO checks in the runtime is to never touch a guard page. i.e. we always detect
 * that we might run out of stack space (at specific points in code) and raise a stack overflow
 * exception. We thus guarantee that our data structures are consistent even when a SO exception is raised.
 *
 * All stack probes check if the current stack pointer - requested size > current stack limit.
 *
 *  stack start (low)    +----------------------+
 *                       | MEM_RESERVE          |
 *                       +----------------------+
 *                       | Thread stack         |
 *                       ~ guarantee            ~
 *                       | (PAGE_GUARD)         |
 *                       +----------------------+ <--------------> +----------------------+
 *                       | MEM_RESERVE          |                  |                      |
 *                       ~                      ~                  ~ overflow handling    | (B)
 *                       |                      |                  |   buffer             |
 *                       +----------------------+                  +----------------------+ <---- stackLimit
 *                       | PAGE_GUARD           |                  |                      |
 *                       +----------------------+                  |                      |
 *                       | MEM_COMMIT |         |                  | script region        |
 *                       ~ PAGE_READWRITE       ~                  ~                      ~ (A)
 *                       |                      |                  |                      |
 *               (high)  +----------------------+ <--------------> +----------------------+
 */

void
StackProber::Initialize()
{
    // NumGuardPages is 2 on x86/x86-64
    // 1 MEM_RESERVE page at the bottom of the stack
    // 1 PAGE_GUARD | PAGE_READWRITE page that serves as the guard page
    const size_t guardPageSize = Js::Constants::NumGuardPages * AutoSystemInfo::PageSize;
    const size_t stackOverflowBuffer = Js::Constants::StackOverflowHandlingBufferPages * AutoSystemInfo::PageSize;

    PBYTE stackBottom = 0;      // This is the low address limit (here we consider stack growing down).
    ULONG stackGuarantee = 0;

#if defined(_M_IX86)
    stackBottom = (PBYTE)__readfsdword(0xE0C); // points to the DeAllocationStack on the TEB - which turns to be the stack bottom.
#elif defined(_M_AMD64)
    stackBottom = (PBYTE)__readgsqword(0x1478);
#elif defined(_M_ARM)
    ULONG lowLimit, highLimit;
    ::GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    stackBottom = (PBYTE)lowLimit;
#elif defined(_M_ARM64)
    ULONG64 lowLimit, highLimit;
    ::GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    stackBottom = (PBYTE) lowLimit;
#else
    stackBottom = NULL;
    Js::Throw::NotImplemented();
#endif

    Assert(stackBottom);

    // Calling this API with stackGuarantee == 0 *gets* current stack guarantee.
    SetThreadStackGuarantee(&stackGuarantee);
    stackLimit = stackBottom + guardPageSize + stackGuarantee + stackOverflowBuffer;
}
