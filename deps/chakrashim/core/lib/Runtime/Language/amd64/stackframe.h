//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

/*
 * Stackwalking on x86-64:
 * ----------------------
 *
 * On x86 the EBP register always points to the current stack frame, which, at
 * offset 0 contains a pointer to its caller's stack frame. Walking the stack
 * is accomplished by walking this linked list. Currently VC++ does not build
 * RBP frames on x86-64. However, the format of a function's stack frame is fairly
 * restricted. This and the fact that each function has metadata makes a stack walk
 * possible.
 *
 * The x86-64 ABI (that VC++ uses) is an x86 "fastcall" like calling convention
 * that uses RCX, RDX, R8 and R9 to pass the first 4 QWORD parameters with stack backing
 * for those registers. The caller is responsible for allocating space for parameters to
 * the callee and *always* allocates 4 extra QWORDs that the callee uses to "home" the
 * argument registers (typically). Home addresses are required for the register arguments
 * so a contiguous area is available in case the callee uses a va_list. However, the callee
 * is not required to save the value in the registers into these slots and generally does
 * not unless the compiler deems it necessary. The caller always cleans the stack.
 *
 * All non-leaf functions (functions that either alloca / call other functions / use exception
 * handling) are annotated with data that describes how non-volatile registers
 * can be restored (say during a stack unwind). This data structure lives in the PDATA section
 * of a PE image. It describes the address limits of the prolog which
 *  - saves argument registers in their home addresses
 *  - pushes non-volatile registers used by the function on the stack
 *  - allocates locals
 *  - establishes a frame pointer (usually only if the function uses alloca)
 * The epilog trims the stack by reversing the effects of the prolog. Epilogs follow a
 * strict set of rules so that the operating system can scan a code stream to identify one.
 * This eliminates the need for more metadata. Epilogs are usually of the form
 *
 *   add RSP, stack_frame_fixed_size
 *   pop Rn
 *   ...
 *   ret
 *
 * Since there is no linked list on the stack to follow, the x86-64 stack walker
 * (the platform specific portions of which are implemented in Amd64StackFrame and Javascript
 * specific portions in JavascriptStackWalker) uses the above mentioned metadata to
 * "unwind" the stack to get to a caller from its callee. Since we don't really want
 * to unwind the stack, we use the RtlVirtualUnwind API that does the unwind using a copy
 * of portions of the real stack using an instance of CONTEXT as the register file.
 *
 * The Javascript stack walker needs to find out what the return address, argv is for a given
 * stack frame.
 *  - return address: to match against one in a script context that serves to identify a
 *    stack frame where a stack walk should start (frameIdOfScriptExitFunction)
 *    or terminate (returnAddrOfScriptEntryFunction).
 *  - argv: used to implement caller args, find the line number (from the function object) when
 *    an exception is thrown etc.
 *
 * During stack unwind, the return address for a given stack frame is the address RIP is
 * pointing to when we unwind to its caller. Similarly argv is the address that RSP is pointing
 * to when we unwind to its caller. We make sure that the arguments passed in registers are
 * homed by making use of the /homeparams switch.
 *
 * A "JavascriptFrame" is a stack frame established either by JITted script code or by the
 * interpreter whose arguments are always:
 *     JavascriptFunction *  <-- instance of the javascript function object that this frame
 *                               corresponds to.
 *     CallInfo
 *     this                  <-- first script argument (hidden "this" pointer).
 *     ...                   <-- rest of the script arguments.
 */

namespace Js {
    class ScriptContext;

    class Amd64StackFrame {
    public:
        Amd64StackFrame();
        ~Amd64StackFrame();

        bool InitializeByFrameId(void * returnAddress, ScriptContext* scriptContext) { return InitializeByReturnAddress(returnAddress, scriptContext); }
        bool InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext);

        bool Next();

        void *GetInstructionPointer();
        void **GetArgv(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true);
        void *GetReturnAddress(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true);
        void *GetAddressOfReturnAddress(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true);
        void *GetAddressOfInstructionPointer() { return this->addressOfCodeAddr; }
        bool SkipToFrame(void * returnAddress);
        void *GetFrame() const;
        size_t GetStackCheckCodeHeight() { return this->stackCheckCodeHeight; }
        static bool IsInStackCheckCode(void *entry, void * codeAddr, size_t stackCheckCodeHeight);

    private:
        void*            addressOfCodeAddr;
        ScriptContext    *scriptContext;
        ULONG64           imageBase;
        RUNTIME_FUNCTION *functionEntry;
        CONTEXT          *currentContext;

        // We store the context of the caller the first time we retrieve it so that
        // consecutive operations that need the caller context don't have to retrieve
        // it again. The callerContext is only valid if hasCallerContext is true. When
        // currentContext is changed, callerContext must be invalidated by calling
        // OnCurrentContextUpdated().
        bool              hasCallerContext;
        CONTEXT          *callerContext;
        size_t            stackCheckCodeHeight;

        __inline void EnsureFunctionEntry();
        __inline bool EnsureCallerContext(bool isCurrentContextNative);
        __inline void OnCurrentContextUpdated();

        static bool NextFromNativeAddress(CONTEXT * context);
        static bool Next(CONTEXT *context, ULONG64 imageBase, RUNTIME_FUNCTION *runtimeFunction);
        static const size_t stackCheckCodeHeightThreadBound = StackFrameConstants::StackCheckCodeHeightThreadBound;
        static const size_t stackCheckCodeHeightNotThreadBound = StackFrameConstants::StackCheckCodeHeightNotThreadBound;
        static const size_t stackCheckCodeHeightWithInterruptProbe = StackFrameConstants::StackCheckCodeHeightWithInterruptProbe;
    };

    class Amd64ContextsManager
    {
    private:
        static const int CONTEXT_PAIR_COUNT = 2;

        enum
        {
            GENERAL_CONTEXT = 0,
            OOM_CONTEXT = 1,
            NUM_CONTEXTS = 2
        };
        typedef int ContextsIndex;

        CONTEXT contexts[CONTEXT_PAIR_COUNT * NUM_CONTEXTS];

        _Field_range_(GENERAL_CONTEXT, NUM_CONTEXTS)
        ContextsIndex curIndex;

        _Ret_writes_(CONTEXT_PAIR_COUNT) CONTEXT* InternalGet(
            _In_range_(GENERAL_CONTEXT, OOM_CONTEXT) ContextsIndex index);

    public:
        Amd64ContextsManager();

    private:
        friend class Amd64StackFrame;

        _Ret_writes_(CONTEXT_PAIR_COUNT) CONTEXT* Allocate();
        void Release(_In_ CONTEXT* contexts);
    };
};
