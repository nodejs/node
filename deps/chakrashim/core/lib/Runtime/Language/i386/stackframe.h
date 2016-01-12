//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


namespace Js {

#ifndef _M_IX86
#error This is only for x86
#endif

    class X86StackFrame {
    public:
        X86StackFrame() : frame(nullptr), codeAddr(nullptr), stackCheckCodeHeight(0), addressOfCodeAddr(nullptr) {};

        bool InitializeByFrameId(void * frameAddress, ScriptContext* scriptContext);
        bool InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext);

        bool Next();

        void *  GetInstructionPointer() { return codeAddr; }
        void ** GetArgv(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) { return frame + 2; } // parameters unused for x86, arm and arm64
        void *  GetReturnAddress(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) { return frame[1]; } // parameters unused for x86, arm and arm64
        void *  GetAddressOfReturnAddress(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) { return &frame[1]; } // parameters unused for x86, arm and arm64
        void *  GetAddressOfInstructionPointer() const { return addressOfCodeAddr; }
        void *  GetFrame() const { return (void *)frame; }

        void SetReturnAddress(void * address) { frame[1] = address; }
        bool SkipToFrame(void * frameAddress);

        size_t GetStackCheckCodeHeight() { return this->stackCheckCodeHeight; }
        static bool IsInStackCheckCode(void *entry, void *codeAddr, size_t stackCheckCodeHeight);

    private:
        void ** frame;      // ebp
        void * codeAddr;    // eip
        void * addressOfCodeAddr;
        size_t stackCheckCodeHeight;

        static const size_t stackCheckCodeHeightThreadBound = StackFrameConstants::StackCheckCodeHeightThreadBound;
        static const size_t stackCheckCodeHeightNotThreadBound = StackFrameConstants::StackCheckCodeHeightNotThreadBound;
        static const size_t stackCheckCodeHeightWithInterruptProbe = StackFrameConstants::StackCheckCodeHeightWithInterruptProbe;
    };

};
