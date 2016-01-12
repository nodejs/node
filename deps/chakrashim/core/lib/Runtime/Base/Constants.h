//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef  _M_X64_OR_ARM64
# define WIN64_STACK_FACTOR 3
#else
# define WIN64_STACK_FACTOR 1
#endif

namespace Js
{
    class Constants
    {
    public:
        // Id of the NoContextSourceContextInfo
        static const uint               NoSourceContext             = (uint)-1;
        // Variable indicating no source context cookie was passed in by the host- indicates Dynamic Script
        static const DWORD_PTR          NoHostSourceContext         = (DWORD_PTR)-1;
        static const DWORD_PTR          FunctionBodyUnavailable     = (DWORD_PTR)NULL; // Not a valid Var
        static const LocalFunctionId    NoFunctionId                = (LocalFunctionId)-1;
        static const uint               NoStatementIndex            = (uint)-1;
        static const uint               NoByteCodeOffset            = (uint)-1;
        static const PropertyId         NoProperty                  = -1;
        static const RegSlot            NoRegister                  = (RegSlot)-1;
        static const RegSlot            OneByteRegister             = (RegSlot_OneByte)-1;
        static const int32              InvalidOffset               = -1;
        static const unsigned int       PropertyGroupSize           = 512;
        static const unsigned int       PropertyGroupMask           = PropertyGroupSize-1;
        static const unsigned int       PropertyGroupShift          = 9;
        static const unsigned int       MaxPropertyGroups           = 128;
        static const PropertyIndex      FieldsChunkSize             = 4;
        static const PropertyIndex      NoSlot                      = (PropertyIndex)-1;
        static const PropertyIndex      PropertyIndexMax            = 0xFFFE;
        static const BigPropertyIndex   NoBigSlot                   = (BigPropertyIndex)-1;
        static const int                IntMaxValue                 = 2147483647;
        static const int                Int31MinValue               = -1 << 30;
        static const int                Int31MaxValue               = ~Int31MinValue;
        static const unsigned int       UShortMaxValue              = 0xFFFF;
        static const uint               InvalidSourceIndex          = (uint)-1;
        static const ProfileId          NoProfileId                 = (ProfileId)-1;
        static const int                InvalidByteCodeOffset       = -1;
        static const InlineCacheIndex   NoInlineCacheIndex          = (InlineCacheIndex)-1;
        static const uint               UninitializedValue          = (uint)-1;
        static const ArgSlot            InvalidArgSlot              = (ArgSlot)-1;
        static const uint32             InvalidSymID = (uint32)-1;

        static const int ReservedTypeIds = 2048;

        // 1 MEM_RESERVE page at the bottom of the stack
        // 1 PAGE_GUARD | PAGE_READWRITE page that serves as the guard page
        static const unsigned NumGuardPages = 2;

        // We need to keep some buffer to run our stack overflow
        // handling code which throws an exception
        static const unsigned StackOverflowHandlingBufferPages = 10;

        // Minimum stack required to JIT-compile a function
        static const unsigned MinStackJITCompile = 16 * 0x400 * WIN64_STACK_FACTOR; // 16 KB

        // Maximum number of arguments allowed on an inlinee function for constant argument inlining
        // It is set to 13 to ensure optimum size of callSiteInfo
        static const unsigned MaximumArgumentCountForConstantArgumentInlining = 13;

        //Invalid loop unrolling factor used for memory operations (memset/ memcopy)
        static const byte InvalidLoopUnrollFactor = 0x7F;

        // Minimum stack required to be able to execute a JITted Javascript function.
        static const unsigned MinStackJIT = 0x930 * WIN64_STACK_FACTOR;

#if (defined(_M_ARM32_OR_ARM64) || defined(_M_AMD64))
#if DBG
        static const unsigned MinStackInterpreter = 0x4000; // 16 KB
#else
        // Minimum stack required to be able to enter the interpreter.
        static const unsigned MinStackInterpreter = 0x2000; // 8 KB
#endif
#else
#if DBG
        static const unsigned MinStackInterpreter = 0x1000; // 4 KB
#else
        // Minimum stack required to be able to enter the interpreter.
        static const unsigned MinStackInterpreter = 0xC00; // 3 KB
#endif
#endif
        // Minimum stack required by the byte code compiler.
        static const unsigned MinStackCompile = 0x400 * WIN64_STACK_FACTOR; // 1 KB

        static const unsigned MinStackByteCodeVisitor = 0xC00 * WIN64_STACK_FACTOR; // 3 KB
        static const unsigned MinStackRegex = 8 * 0x400 * WIN64_STACK_FACTOR; // 8 KB

        static const unsigned MinStackRuntime = 32 * 0x0400 * WIN64_STACK_FACTOR;
        static const unsigned MinStackHost = 40 * 0x0400 * WIN64_STACK_FACTOR;
        static const unsigned MinStackCallout = 32 * 0x0400 * WIN64_STACK_FACTOR;

        static const unsigned MinStackParseOneTerm = 0x100 * WIN64_STACK_FACTOR;

        // MinStackDefault is the minimum amount of stack space required to
        // call RaiseException to report a stack overflow.
        static const unsigned MinStackDefault = 1 * 0x0400 * WIN64_STACK_FACTOR;
        static const unsigned ExtraStack = 2 * 0x0400 * WIN64_STACK_FACTOR;

#if _M_IX86_OR_ARM32
        static const unsigned MaxThreadJITCodeHeapSize = 28 * 1024 * 1024;
        static const unsigned MaxProcessJITCodeHeapSize = 55 * 1024 * 1024;
#elif _M_X64_OR_ARM64
        // larger address space means we can keep this higher on 64 bit architectures
        static const unsigned MaxThreadJITCodeHeapSize = 800 * 1024 * 1024;
        static const unsigned MaxProcessJITCodeHeapSize = 1024 * 1024 * 1024;
#endif

#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
        static const unsigned MaxThreadContextsWithPreReserveSegment = 6;
#endif

        static const PBYTE StackLimitForScriptInterrupt;


        // Arguments object created on the fly is 1 slot before the frame
        static const int ArgumentLocationOnFrame = 1;
        static const int StackNestedFuncList = 2;
        static const int StackFrameDisplay = 3;
        static const int StackScopeSlots = 4;
#if _M_IX86 || _M_AMD64
        static const int StackNestedFuncListWithNoArg = 1;
        static const int StackFrameDisplayWithNoArg = 2;
        static const int StackScopeSlotsWithNoArg = 3;
#endif

        static const DWORD NonWebWorkerContextId = 0;

        // Inlinee constants shared between the backend and the stack walker.
        static const uint InlineeMetaArgIndex_Argc            = 0;
        static const uint InlineeMetaArgIndex_FunctionObject  = 1;
        static const uint InlineeMetaArgIndex_ArgumentsObject = 2;
        static const uint InlineeMetaArgIndex_Argv            = 3;
        static const uint InlineeMetaArgCount                 = 3;

        static const  wchar_t AnonymousFunction[];
        static const  wchar_t AnonymousCode[];
        static const  wchar_t Anonymous[];            // Used in the function created due to new Function
        static const  wchar_t Empty[];
        static const  wchar_t FunctionCode[];
        static const  wchar_t GlobalCode[];
        static const  wchar_t EvalCode[];
        static const  wchar_t GlobalFunction[];
        static const  wchar_t UnknownScriptCode[];

        static const size_t AnonymousFunctionLength = _countof(L"Anonymous function") - 1;
        static const size_t AnonymousLength         = _countof(L"anonymous") - 1;
        static const size_t AnonymousClassLength    = _countof(L"Anonymous class") - 1;
        static const size_t FunctionCodeLength      = _countof(L"Function code") - 1;
        static const size_t GlobalFunctionLength    = _countof(L"glo") - 1;
        static const size_t GlobalCodeLength        = _countof(L"Global code") - 1;
        static const size_t EvalCodeLength          = _countof(L"eval code") - 1;
        static const size_t UnknownScriptCodeLength = _countof(L"Unknown script code") - 1;
    };

    extern const FrameDisplay NullFrameDisplay;
    extern const FrameDisplay StrictNullFrameDisplay;
}
