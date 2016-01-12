//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
namespace Js
{
#ifdef ENABLE_MUTATION_BREAKPOINT
    class MutationBreakpoint;
#endif

    class DebugManager;
    struct Probe;
    typedef JsUtil::List<Probe*, ArenaAllocator> ProbeList;
    class DiagStackFrame;
    typedef JsUtil::Stack<DiagStackFrame*> DiagStack;
    typedef WeakArenaReference<DiagStack> WeakDiagStack;
    struct InterpreterHaltState;

    // This class contains the probes and list of function bodies.
    // The object of this class is maintained by ScriptContext.
    class ProbeContainer
    {
        friend class RecyclableObjectDisplay;
        friend class RecyclableArrayWalker;

    private:
        ProbeList* diagProbeList;
        ProbeList* pendingProbeList;

        ScriptContext* pScriptContext;
        DebugManager *debugManager;

        // Stack for a current scriptcontext
        DiagStack* framePointers;

        HaltCallback* haltCallbackProbe;
        DebuggerOptionsCallback* debuggerOptionsCallback;

        // Refer to the callback which is responsible for making async break
        HaltCallback* pAsyncHaltCallback;

        Var jsExceptionObject;

        // Used for synchronizing with ProbeMananger
        ulong debugSessionNumber;

        uint32  tmpRegCount; // Mentions the temp register count for the current statement (this will be used to determine if SetNextStatement can be applied)

        // Used when SetNextStatement is applied.
        int bytecodeOffset;
        bool IsNextStatementChanged;

        // Used when the throw is internal and engine does not want to be broken at exception.
        bool isThrowInternal;

        // This variabled will be set true when we don't want to check for debug script engine being initialized.
        bool forceBypassDebugEngine;

        bool isPrimaryBrokenToDebuggerContext;

        JsUtil::List<DWORD_PTR, ArenaAllocator> *registeredFuncContextList;
        JsUtil::List<const Js::PropertyRecord*> *pinnedPropertyRecords;

        void UpdateFramePointers(bool fMatchWithCurrentScriptContext);
        bool InitializeLocation(InterpreterHaltState* pHaltState, bool fMatchWithCurrentScriptContext = true);
        void DestroyLocation();

        bool ProbeContainer::GetNextUserStatementOffsetHelper(
            Js::FunctionBody* functionBody, int currentOffset, FunctionBody::StatementAdjustmentType adjType, int* nextStatementOffset);

#ifdef ENABLE_MUTATION_BREAKPOINT
        void InitMutationBreakpointListIfNeeded();
        void ClearMutationBreakpoints();
        void RemoveMutationBreakpointListIfNeeded();
#endif
        static bool FetchTmpRegCount(Js::FunctionBody * functionBody, Js::ByteCodeReader * reader, int atOffset, uint32 *pTmpRegCount, Js::OpCode *pOp);
    public:

        bool isForcedToEnterScriptStart;

        ProbeContainer();
        ~ProbeContainer();

        void StartRecordingCall();
        void EndRecordingCall(Js::Var returnValue, Js::JavascriptFunction * function);
        ReturnedValueList* GetReturnedValueList() const;
        void ResetReturnedValueList();

        void Initialize(ScriptContext* pScriptContext);
        void Close();

        WeakDiagStack* GetFramePointers();

        // A break engine responsible for breaking at iniline statement and error statement.
        void InitializeInlineBreakEngine(HaltCallback* pProbe);
        void InitializeDebuggerScriptOptionCallback(DebuggerOptionsCallback* debuggerOptionsCallback);

        void UninstallInlineBreakpointProbe(HaltCallback* pProbe);
        void UninstallDebuggerScriptOptionCallback();

        void AddProbe(Probe* pProbe);
        void RemoveProbe(Probe* pProbe);

        void RemoveAllProbes();

        bool CanDispatchHalt(InterpreterHaltState* pHaltState);

        // When on breakpoint hit
        void DispatchProbeHandlers(InterpreterHaltState* pHaltState);

        // When on step in, step out and step over
        void DispatchStepHandler(InterpreterHaltState* pHaltState, OpCode* pOriginalOpcode);

        // When on break-all
        void DispatchAsyncBreak(InterpreterHaltState* pHaltState);

        // When executing 'debugger' statement
        void DispatchInlineBreakpoint(InterpreterHaltState* pHaltState);

        // When encountered and exception
        bool DispatchExceptionBreakpoint(InterpreterHaltState* pHaltState);

        // When on mutation breakpoint hit
        void DispatchMutationBreakpoint(InterpreterHaltState* pHaltState);

        void UpdateStep(bool fDuringSetupDebugApp = false);
        void DeactivateStep();

        bool GetNextUserStatementOffsetForSetNext(Js::FunctionBody* functionBody, int currentOffset, int* nextStatementOffset);
        bool GetNextUserStatementOffsetForAdvance(Js::FunctionBody* functionBody, ByteCodeReader* reader, int currentOffset, int* nextStatementOffset);
        bool AdvanceToNextUserStatement(Js::FunctionBody* functionBody, ByteCodeReader* reader);

        void SetNextStatementAt(int bytecodeOffset);
        bool IsSetNextStatementCalled() const { return IsNextStatementChanged; }
        int GetByteCodeOffset() const { Assert(IsNextStatementChanged); return bytecodeOffset; }

        void AsyncActivate(HaltCallback* haltCallback);
        void AsyncDeactivate();

        void PrepDiagForEnterScript();

        void RegisterContextToDiag(DWORD_PTR context, ArenaAllocator *alloc);
        bool IsContextRegistered(DWORD_PTR context);
        FunctionBody * GetGlobalFunc(ScriptContext* scriptContext, DWORD_PTR secondaryHostSourceContext);

        Var GetExceptionObject() { return jsExceptionObject; }

        bool HasAllowedForException(__in JavascriptExceptionObject* exceptionObject);

        void SetThrowIsInternal(bool set) { isThrowInternal = set; }

        bool IsFirstChanceExceptionEnabled();
        bool IsNonUserCodeSupportEnabled();
        bool IsLibraryStackFrameSupportEnabled();

        void SetCurrentTmpRegCount(uint32 set) { tmpRegCount = set; }
        uint32 GetCurrentTmpRegCount() const { return tmpRegCount; }
        void PinPropertyRecord(const Js::PropertyRecord *propertyRecord);

        bool IsPrimaryBrokenToDebuggerContext() const { return isPrimaryBrokenToDebuggerContext; }
        void SetIsPrimaryBrokenToDebuggerContext(bool set) { isPrimaryBrokenToDebuggerContext = set; }

        DebugManager *GetDebugManager() const { return this->debugManager; }

#ifdef ENABLE_MUTATION_BREAKPOINT
        typedef JsUtil::List<RecyclerWeakReference<Js::MutationBreakpoint>*, Recycler, false, Js::WeakRefFreeListedRemovePolicy> MutationBreakpointList;
        RecyclerRootPtr<MutationBreakpointList> mutationBreakpointList;
        bool HasMutationBreakpoints();
        void InsertMutationBreakpoint(MutationBreakpoint *mutationBreakpoint);
#endif
        static bool IsTmpRegCountIncreased(Js::FunctionBody* functionBody, ByteCodeReader* reader, int currentOffset, int nextStmOffset, bool restoreOffset);
    };
} // namespace Js.
