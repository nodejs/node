//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if !defined(_M_ARM64)
#error Arm64StackFrame is not supported on this architecture.
#endif

// For ARM64, we walk the fp chain (similar to the EBP chain on x86). This allows us to do what the internal
// stack walker needs to do - find and visit non-leaf Javascript frames on the call stack and retrieve return
// addresses and parameter values. Note that we don't need the equivalent of CONTEXT_UNWOUND_TO_CALL here,
// or any PC adjustment to account for return from tail call, because the PC is only used to determine whether
// we're in a Javascript frame or not, not to control unwinding (via pdata). So we only require that Javascript
// functions not end in call instructions.
// We also require that register parameters be homed on entry to a Javascript function, something that jitted
// functions and the ETW interpreter thunk do, and which C++ vararg functions currently do as well. The guidance
// from C++ is that we not rely on this behavior in future. If we have to visit interpreted Javascript frames
// that don't pass through the ETW thunk, we'll have to use some other mechanism to force homing of parameters.

namespace Js
{

bool
Arm64StackFrame::InitializeByFrameId(void * frame, ScriptContext* scriptContext)
{
    return SkipToFrame(frame);
}

// InitializeByReturnAddress.
// Parameters:
//   unwindToAddress: specifies the address we need to unwind the stack before any walks can be done.
//     This is expected to be return address i.e. address of the instruction right after the blx instruction
//     and not the address of blx itself.
bool
Arm64StackFrame::InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext)
{
    this->frame = (void**)arm64_GET_CURRENT_FRAME();
    while (Next())
    {
        if (this->codeAddr == returnAddress)
        {
            return true;
        }
    }
    return false;
}

bool
Arm64StackFrame::Next()
{
    this->addressOfCodeAddr = this->GetAddressOfReturnAddress();
    this->codeAddr = this->GetReturnAddress();
    this->frame = (void **)this->frame[0];
    return frame != nullptr;
}

bool
Arm64StackFrame::SkipToFrame(void * frameAddress)
{
    this->frame = (void **)frameAddress;
    return Next();
}

void *
Arm64StackFrame::GetInstructionPointer()
{
    return codeAddr;
}

void **
Arm64StackFrame::GetArgv(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{
    UNREFERENCED_PARAMETER(isCurrentContextNative);
    UNREFERENCED_PARAMETER(shouldCheckForNativeAddr);
    return this->frame + ArgOffsetFromFramePtr;
}

void *
Arm64StackFrame::GetReturnAddress(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{
    UNREFERENCED_PARAMETER(isCurrentContextNative);
    UNREFERENCED_PARAMETER(shouldCheckForNativeAddr);
    return this->frame[ReturnAddrOffsetFromFramePtr];
}

void *
Arm64StackFrame::GetAddressOfReturnAddress(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{
    UNREFERENCED_PARAMETER(isCurrentContextNative);
    UNREFERENCED_PARAMETER(shouldCheckForNativeAddr);
    return &this->frame[ReturnAddrOffsetFromFramePtr];
}

bool
Arm64StackFrame::IsInStackCheckCode(void *entry, void *codeAddr, size_t stackCheckCodeHeight)
{
    return ((size_t(codeAddr) - size_t(entry)) <= stackCheckCodeHeight);
}

}
