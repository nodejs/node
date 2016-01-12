//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{

#ifdef FAULT_INJECTION
    class FaultInjection
    {
    public:
        static const unsigned int MAX_FRAME_COUNT = 64;

        // Fault types
#define FAULT_TYPE(x) x, \

        enum FaultType
        {
            #include "FaultTypes.h"

            FaultTypeCount,
            InvalidFaultType
        };
#undef FAULT_TYPE

        // use bit array to save the enabled type
        class FaultInjectionTypes
        {
        private:
            char faultTypeBitArray[InvalidFaultType/8+1];
            char getBit(int index) {
                return (faultTypeBitArray[index/8] >> (7-(index & 0x7))) & 0x1;
            }
            void setBit(int index, int value) {
                faultTypeBitArray[index/8] = faultTypeBitArray[index/8] | (value & 0x1) << (7-(index & 0x7));
            }
        public:
            FaultInjectionTypes(){
                memset(&faultTypeBitArray, 0, sizeof(faultTypeBitArray));
            }
            void EnableAll(){
                memset(&faultTypeBitArray, ~0, sizeof(faultTypeBitArray));
            }
            void EnableType(FaultType type);
            void EnableType(int type){
                EnableType((FaultType)type);
            }
            bool IsEnabled(FaultType type);
            bool IsEnabled(const wchar_t* name);
        };

        static wchar_t *FaultTypeNames[];
        void ParseFaultTypes(const wchar_t* szFaultTypes);

    public:
        enum FaultMode
        {
            CountOnly = 0,
            CountEquals = 1,
            CountEqualsOrAbove = 2,
            StackMatch = 3,
            StackMatchCountOnly = 4,
            StackHashCountOnly = 5,
            DisplayAvailableFaultTypes = 6,
        };

        uint countOfInjectionPoints;
        int FaultInjectionCookie;
        enum StackMatchInitializationState
        {
            Uninitialized = 0,
            FailedToInitialize = 1,
            Succeeded = 2
        };
        StackMatchInitializationState stackMatchInitialized; // tri-state: 0-uninitialized, 1-tried to init, 2-initialized

    private:
        HMODULE hDbgHelp;
        bool InitializeSym();
        FaultInjectionTypes* faultInjectionTypes;
        bool IsCurrentStackMatch();
        bool EnsureStackMatchInfraInitialized();
        uint baselineFrameCount;
        wchar_t *baselineStack[MAX_FRAME_COUNT];
        UINT_PTR baselineAddresses[MAX_FRAME_COUNT];
        ULONG_PTR* stackHashOfAllInjectionPoints;
        UINT stackHashOfAllInjectionPointsSize;

    public:
        static FaultInjection Global;
        FaultInjection();
        ~FaultInjection();
        bool IsFaultEnabled(FaultType faultType);
        bool IsFaultInjectionOn(FaultType faultType);
        bool ShouldInjectFault(FaultType fType, LPCWSTR name = nullptr, size_t size = 0);// name and size are used for OOM only

        // sample for customized fault type
        template<class Pred>
        bool ShouldInjectFault(FaultType fType, Pred p) {
            bool shouldInjectionFault = Js::Configuration::Global.flags.FaultInjectionCount == 0
                || ShouldInjectFaultHelper(fType);
            if (shouldInjectionFault && p()) {
                if(IsDebuggerPresent()) {
                    DebugBreak();
                }
                dumpCurrentStackData();
            }
            return shouldInjectionFault;
        }

    private:
        bool ShouldInjectFaultHelper(FaultType fType, LPCWSTR name = nullptr, size_t size = 0);

    private:
        // for reconstruction stack of the fault injection points in postmortem debugging
        struct InjectionRecord{
            void* StackFrames[MAX_FRAME_COUNT];
            UINT_PTR hash;
            WORD FrameCount;
            void* StackData;
            size_t StackDataLength;
            CONTEXT Context;
            WCHAR name[32];
            size_t allocSize;
            InjectionRecord* next;
        };
    public:
        InjectionRecord* InjectionFirstRecord;
        InjectionRecord** InjectionLastRecordRef;
        int InjectionRecordsCount;
        void dumpCurrentStackData(LPCWSTR name = nullptr, size_t size = 0);

        static __declspec(thread) int(*pfnHandleAV)(int, PEXCEPTION_POINTERS);

    private:
        bool symInitialized;
        static PVOID vectoredExceptionHandler;
        static DWORD exceptionFilterRemovalLastError;
        static bool InstallExceptionFilters();
        static void RemoveExceptionFilters();
        static UINT_PTR CalculateStackHash(void* frames[], WORD frameCount, WORD framesToSkip);
        static LONG WINAPI FaultInjectionExceptionFilter(_In_  struct _EXCEPTION_POINTERS *ExceptionInfo);
        void FaultInjetionAnalyzeException(_EXCEPTION_POINTERS *ep);
    };
#endif

}  // namespace Js

#ifdef FAULT_INJECTION
#define IS_FAULTINJECT_NO_THROW_ON \
    Js::FaultInjection::Global.IsFaultInjectionOn(Js::FaultInjection::Global.NoThrow)

#define FAULTINJECT_MEMORY_NOTHROW(name, size) \
    if(Js::FaultInjection::Global.ShouldInjectFault(Js::FaultInjection::Global.NoThrow, name, size)) \
        return NULL;

#define FAULTINJECT_MEMORY_THROW(name, size) \
    if(Js::FaultInjection::Global.ShouldInjectFault(Js::FaultInjection::Global.Throw, name, size)) \
        Js::Throw::OutOfMemory();

#define FAULTINJECT_MEMORY_MARK_THROW(name, size) \
    if(Js::FaultInjection::Global.ShouldInjectFault(Js::FaultInjection::Global.MarkThrow, name, size)) { \
        Js::Throw::OutOfMemory(); \
    }

#define FAULTINJECT_MEMORY_MARK_NOTHROW(name, size) \
    if(Js::FaultInjection::Global.ShouldInjectFault(Js::FaultInjection::Global.MarkNoThrow, name, size)) { \
        return false; \
    }

#define FAULTINJECT_SCRIPT_TERMINATION \
    if((this->threadContextFlags & ThreadContextFlagCanDisableExecution) != 0){ \
        if( Js::FaultInjection::Global.ShouldInjectFault(Js::FaultInjection::Global.ScriptTermination)){ \
            this->stackLimitForCurrentThread = Js::Constants::StackLimitForScriptInterrupt; \
        }\
    }

#define FAULTINJECT_STACK_PROBE \
    if( Js::FaultInjection::Global.ShouldInjectFault(Js::FaultInjection::Global.StackProbe)){ \
        stackAvailable = false; \
    }

#define IS_FAULTINJECT_STACK_PROBE_ON \
    Js::FaultInjection::Global.IsFaultInjectionOn(Js::FaultInjection::Global.StackProbe)

#define FAULTINJECT_SCRIPT_TERMINATION_ON_DISPOSE \
    Js::FaultInjection::Global.ShouldInjectFault(Js::FaultInjection::Global.ScriptTerminationOnDispose)

// A general implementation of customized fault type injection
#define INJECT_FAULT(type, condition, execution) \
    do{\
        if(Js::FaultInjection::Global.ShouldInjectFault(type, condition)) {\
            ##execution##\
        };\
    }while(0)

#else
#define IS_FAULTINJECT_NO_THROW_ON false

#define FAULTINJECT_MEMORY_NOTHROW(name, size)
#define FAULTINJECT_MEMORY_THROW(name, size)
#define FAULTINJECT_MEMORY_MARK_THROW(name, size)
#define FAULTINJECT_MEMORY_MARK_NOTHROW(name, size)

#define FAULTINJECT_SCRIPT_TERMINATION
#define FAULTINJECT_SCRIPT_TERMINATION_ON_DISPOSE false
#define FAULTINJECT_STACK_PROBE
#define IS_FAULTINJECT_STACK_PROBE_ON false

#define INJECT_FAULT(type, condition, execution)

#endif

