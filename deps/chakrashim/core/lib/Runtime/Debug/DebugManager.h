//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct InterpreterHaltState;

    class DebugManager
    {
        friend class RecyclableObjectDisplay;
        friend class RecyclableArrayWalker;
        template <typename TData> friend class RecyclableCollectionObjectWalker;
        template <typename TData> friend class RecyclableCollectionObjectDisplay;
        friend class RecyclableKeyValueDisplay;
        friend class ProbeContainer;

    private:
        InterpreterHaltState* pCurrentInterpreterLocation; // NULL if not Halted at a Probe
        DWORD_PTR secondaryCurrentSourceContext;           // For resolving ambiguity among generated files, e.g. eval, anonymous, etc.
        ulong debugSessionNumber;                          // A unique number, which will be used to sync all probecontainer when on break
        RecyclerRootPtr<Js::DynamicObject> pConsoleScope;
        ThreadContext* pThreadContext;
        bool isAtDispatchHalt;
        PageAllocator diagnosticPageAllocator;

        int evalCodeRegistrationCount;
        int anonymousCodeRegistrationCount;
        int jscriptBlockRegistrationCount;
        bool isDebuggerAttaching;
        DebuggingFlags debuggingFlags;
#if DBG
        void * dispatchHaltFrameAddress;
#endif
    public:
        StepController stepController;
        AsyncBreakController asyncBreakController;
        PropertyId mutationNewValuePid;                    // Holds the property id of $newValue$ property for object mutation breakpoint
        PropertyId mutationPropertyNamePid;                // Holds the property id of $propertyName$ property for object mutation breakpoint
        PropertyId mutationTypePid;                        // Holds the property id of $mutationType$ property for object mutation breakpoint

        DebugManager(ThreadContext* _pThreadContext, AllocationPolicyManager * allocationPolicyManager);
        ~DebugManager();
        void Close();

        DebuggingFlags* GetDebuggingFlags();

        bool IsAtDispatchHalt() const { return this->isAtDispatchHalt; }
        void SetDispatchHalt(bool set) { this->isAtDispatchHalt = set; }

        ReferencedArenaAdapter* GetDiagnosticArena();
        DWORD_PTR AllocateSecondaryHostSourceContext();
        void SetCurrentInterpreterLocation(InterpreterHaltState* pHaltState);
        void UnsetCurrentInterpreterLocation();
        ulong GetDebugSessionNumber() const { return debugSessionNumber; }
#ifdef ENABLE_MUTATION_BREAKPOINT
        MutationBreakpoint* GetActiveMutationBreakpoint() const;
#endif
        DynamicObject* GetConsoleScope(ScriptContext* scriptContext);
        FrameDisplay *GetFrameDisplay(ScriptContext* scriptContext, DynamicObject* scopeAtZero, DynamicObject* scopeAtOne, bool addGlobalThisAtScopeTwo);
        void UpdateConsoleScope(DynamicObject* copyFromScope, ScriptContext* scriptContext);
        PageAllocator * GetDiagnosticPageAllocator() { return &this->diagnosticPageAllocator; }
#if DBG
        void SetDispatchHaltFrameAddress(void * returnAddress) { this->dispatchHaltFrameAddress = returnAddress; }
        void ValidateDebugAPICall();
#endif
        void SetDebuggerAttaching(bool attaching) { this->isDebuggerAttaching = attaching; }
        bool IsDebuggerAttaching() const { return this->isDebuggerAttaching; }

        enum DynamicFunctionType
        {
            DFT_EvalCode,
            DFT_AnonymousCode,
            DFT_JScriptBlock
        };

        int GetNextId(DynamicFunctionType eFunc)
        {
            switch (eFunc)
            {
            case DFT_EvalCode: return ++evalCodeRegistrationCount;
            case DFT_AnonymousCode: return ++anonymousCodeRegistrationCount;
            case DFT_JScriptBlock: return ++jscriptBlockRegistrationCount;
            }

            return -1;
        }
    };
}

