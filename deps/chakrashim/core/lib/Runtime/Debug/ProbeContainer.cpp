//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language\JavascriptStackWalker.h"

namespace Js
{
    ProbeContainer::ProbeContainer() :
        diagProbeList(nullptr),
        pScriptContext(nullptr),
        debugManager(nullptr),
        haltCallbackProbe(nullptr),
        debuggerOptionsCallback(nullptr),
        pAsyncHaltCallback(nullptr),
        jsExceptionObject(nullptr),
        framePointers(nullptr),
        debugSessionNumber(0),
        tmpRegCount(0),
        bytecodeOffset(0),
        IsNextStatementChanged(false),
        isThrowInternal(false),
        forceBypassDebugEngine(false),
        isPrimaryBrokenToDebuggerContext(false),
        isForcedToEnterScriptStart(false),
        registeredFuncContextList(nullptr)
    {
    }

    ProbeContainer::~ProbeContainer()
    {
        this->Close();
    }

    void ProbeContainer::Close()
    {
        // Probe manager instance may go down early.
        if (this->pScriptContext)
        {
            debugManager = this->pScriptContext->GetThreadContext()->GetDebugManager();
        }
        else
        {
            debugManager = nullptr;
        }
        if (debugManager != nullptr && debugManager->stepController.pActivatedContext == pScriptContext)
        {
            debugManager->stepController.Deactivate();
        }
#ifdef ENABLE_MUTATION_BREAKPOINT
        this->RemoveMutationBreakpointListIfNeeded();
#endif
        pScriptContext = nullptr;
        debugManager = nullptr;
    }

    void ProbeContainer::Initialize(ScriptContext* pScriptContext)
    {
        if (!diagProbeList)
        {
            ArenaAllocator* global = pScriptContext->AllocatorForDiagnostics();

            diagProbeList = ProbeList::New(global);

            pendingProbeList = ProbeList::New(global);

            this->pScriptContext = pScriptContext;
            this->debugManager = this->pScriptContext->GetThreadContext()->GetDebugManager();
            this->pinnedPropertyRecords = JsUtil::List<const Js::PropertyRecord*>::New(this->pScriptContext->GetRecycler());
            this->pScriptContext->BindReference((void *)this->pinnedPropertyRecords);
        }
    }

    void ProbeContainer::StartRecordingCall()
    {
        this->debugManager->stepController.StartRecordingCall();
    }

    void ProbeContainer::EndRecordingCall(Js::Var returnValue, Js::JavascriptFunction * function)
    {
        this->debugManager->stepController.EndRecordingCall(returnValue, function);
    }

    ReturnedValueList* ProbeContainer::GetReturnedValueList() const
    {
        return this->debugManager->stepController.GetReturnedValueList();
    }

    void ProbeContainer::ResetReturnedValueList()
    {
        this->debugManager->stepController.ResetReturnedValueList();
    }

    void ProbeContainer::UpdateFramePointers(bool fMatchWithCurrentScriptContext)
    {
        ArenaAllocator* pDiagArena = debugManager->GetDiagnosticArena()->Arena();
        framePointers = Anew(pDiagArena, DiagStack, pDiagArena);

        JavascriptStackWalker walker(pScriptContext, !fMatchWithCurrentScriptContext, nullptr/*returnAddress*/, true/*forceFullWalk*/);
        DiagStack* tempFramePointers = Anew(pDiagArena, DiagStack, pDiagArena);
        const bool isLibraryFrameEnabledDebugger = IsLibraryStackFrameSupportEnabled();

        walker.WalkUntil([&](JavascriptFunction* func, ushort frameIndex) -> bool
        {
            if (isLibraryFrameEnabledDebugger || !func->IsLibraryCode())
            {
                DiagStackFrame* frm = nullptr;
                InterpreterStackFrame *interpreterFrame = walker.GetCurrentInterpreterFrame();
                ScriptContext* frameScriptContext = walker.GetCurrentScriptContext();
                Assert(frameScriptContext);

                if (!fMatchWithCurrentScriptContext && !frameScriptContext->IsInDebugMode() && tempFramePointers->Count() == 0)
                {
                    // this means the top frame is not in the debug mode. We shouldn't be stopping for this break.
                    // This could happen if the exception happens on the diagnosticsScriptEngine.
                    return true;
                }

                // Ignore frames which are not in debug mode, which can happen when diag engine calls into user engine under debugger
                // -- topmost frame is under debugger but some frames could be in non-debug mode as they are from diag engine.
                if (frameScriptContext->IsInDebugMode() &&
                    (!fMatchWithCurrentScriptContext || frameScriptContext == pScriptContext))
                {
                    if (interpreterFrame)
                    {
                        frm = Anew(pDiagArena, DiagInterpreterStackFrame, interpreterFrame, frameIndex);
                    }
                    else
                    {
#if ENABLE_NATIVE_CODEGEN
                        if (func->IsScriptFunction())
                        {
                            frm = Anew(pDiagArena, DiagNativeStackFrame,
                                ScriptFunction::FromVar(walker.GetCurrentFunction()), walker.GetByteCodeOffset(), walker.GetCurrentArgv(), walker.GetCurrentCodeAddr(), frameIndex);
                        }
                        else
#else
                        Assert(!func->IsScriptFunction());
#endif
                        {
                            frm = Anew(pDiagArena, DiagRuntimeStackFrame, func, walker.GetCurrentNativeLibraryEntryName(), walker.GetCurrentArgv(), frameIndex);
                        }
                    }
                }

                if (frm)
                {
                    tempFramePointers->Push(frm);
                }
            }

            return false;
        });
        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::UpdateFramePointers: detected %d frames (this=%p, fMatchWithCurrentScriptContext=%d)\n",
            tempFramePointers->Count(), this, fMatchWithCurrentScriptContext);

        while (tempFramePointers->Count())
        {
            framePointers->Push(tempFramePointers->Pop());
        }
    }

    WeakDiagStack * ProbeContainer::GetFramePointers()
    {
        if (framePointers == nullptr || this->debugSessionNumber < debugManager->GetDebugSessionNumber())
        {
            UpdateFramePointers(/*fMatchWithCurrentScriptContext*/true);
            this->debugSessionNumber = debugManager->GetDebugSessionNumber();
        }

        ReferencedArenaAdapter* pRefArena = debugManager->GetDiagnosticArena();
        return HeapNew(WeakDiagStack,pRefArena,framePointers);
    }

    bool ProbeContainer::InitializeLocation(InterpreterHaltState* pHaltState, bool fMatchWithCurrentScriptContext)
    {
        Assert(debugManager);
        debugManager->SetCurrentInterpreterLocation(pHaltState);

        ArenaAllocator* pDiagArena = debugManager->GetDiagnosticArena()->Arena();

        UpdateFramePointers(fMatchWithCurrentScriptContext);
        pHaltState->framePointers = framePointers;
        pHaltState->stringBuilder = Anew(pDiagArena, StringBuilder<ArenaAllocator>, pDiagArena);

        if (pHaltState->framePointers->Count() > 0)
        {
            pHaltState->topFrame = pHaltState->framePointers->Peek(0);
        }

        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::InitializeLocation (end): this=%p, pHaltState=%p, fMatch=%d, topFrame=%p\n",
            this, pHaltState, fMatchWithCurrentScriptContext, pHaltState->topFrame);

        return true;
    }

    void ProbeContainer::DestroyLocation()
    {
        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DestroyLocation (start): this=%p, IsNextStatementChanged=%d, haltCallbackProbe=%p\n",
            this, this->IsNextStatementChanged, haltCallbackProbe);

        if (IsNextStatementChanged)
        {
            Assert(bytecodeOffset != debugManager->stepController.byteOffset);
            // Note: when we dispatching an exception bytecodeOffset would be same as pProbeManager->pCurrentInterpreterLocation->GetCurrentOffset().

            debugManager->pCurrentInterpreterLocation->SetCurrentOffset(bytecodeOffset);
            IsNextStatementChanged = false;
        }

        framePointers = nullptr;

        // Reset the exception object.

        jsExceptionObject = nullptr;

        Assert(debugManager);
        debugManager->UnsetCurrentInterpreterLocation();

        pinnedPropertyRecords->Reset();

        // Guarding if the probe engine goes away when we are sitting at breakpoint.
        if (haltCallbackProbe)
        {
            // The clean up is called here to scriptengine's object to remove all DebugStackFrames
            haltCallbackProbe->CleanupHalt();
        }
    }

    bool ProbeContainer::CanDispatchHalt(InterpreterHaltState* pHaltState)
    {
        if (!haltCallbackProbe || haltCallbackProbe->IsInClosedState() || debugManager->IsAtDispatchHalt())
        {
            OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, L"ProbeContainer::CanDispatchHalt: Not in break mode. pHaltState = %p\n", pHaltState);
            return false;
        }
        return true;
    }

    void ProbeContainer::DispatchStepHandler(InterpreterHaltState* pHaltState, OpCode* pOriginalOpcode)
    {
        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchStepHandler: start: this=%p, pHaltState=%p, pOriginalOpcode=0x%x\n", this, pHaltState, pOriginalOpcode);

        if (!CanDispatchHalt(pHaltState))
        {
            return;
        }

        __try
        {
            InitializeLocation(pHaltState);
            OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchStepHandler: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n",
                pHaltState, pHaltState->IsValid());

            if (pHaltState->IsValid()) // Only proceed if we find a valid top frame and that is the executing function
            {
                if (debugManager->stepController.IsStepComplete(pHaltState, haltCallbackProbe, *pOriginalOpcode))
                {
                    OpCode oldOpcode = *pOriginalOpcode;
                    pHaltState->GetFunction()->ProbeAtOffset(pHaltState->GetCurrentOffset(), pOriginalOpcode);
                    pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                    debugManager->stepController.Deactivate(pHaltState);
                    haltCallbackProbe->DispatchHalt(pHaltState);

                    if (oldOpcode == OpCode::Break && debugManager->stepController.stepType == STEP_DOCUMENT)
                    {
                        // That means we have delivered the stepping to the debugger, where we had the breakpoint
                        // already, however it is possible that debugger can initiate the step_document. In that
                        // case debugger did not break due to break. So we have break as a breakpoint reason.
                        *pOriginalOpcode = OpCode::Break;
                    }
                    else if (OpCode::Break == *pOriginalOpcode)
                    {
                        debugManager->stepController.stepCompleteOnInlineBreakpoint = true;
                    }
                }
            }
        }
        __finally
        {
            DestroyLocation();
        }

        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchStepHandler: end: pHaltState=%p\n", pHaltState);
    }

    void ProbeContainer::DispatchAsyncBreak(InterpreterHaltState* pHaltState)
    {
        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchAsyncBreak: start: this=%p, pHaltState=%p\n", this, pHaltState);

        if (!this->pAsyncHaltCallback || !CanDispatchHalt(pHaltState))
        {
            return;
        }

        __try
        {
            InitializeLocation(pHaltState, /* We don't need to match script context, stop at any available script function */ false);
            OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchAsyncBreak: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n",
                pHaltState, pHaltState->IsValid());

            if (pHaltState->IsValid())
            {
                // Activate the current haltCallback with asyncStepController.
                debugManager->asyncBreakController.Activate(this->pAsyncHaltCallback);
                if (debugManager->asyncBreakController.IsAtStoppingLocation(pHaltState))
                {
                    OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchAsyncBreak: IsAtStoppingLocation: pHaltState=%p\n", pHaltState);

                    pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                    debugManager->stepController.Deactivate(pHaltState);
                    debugManager->asyncBreakController.DispatchAndReset(pHaltState);
                }
            }
        }
        __finally
        {
            DestroyLocation();
        }

        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchAsyncBreak: end: pHaltState=%p\n", pHaltState);
    }

    void ProbeContainer::DispatchInlineBreakpoint(InterpreterHaltState* pHaltState)
    {
        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchInlineBreakpoint: start: this=%p, pHaltState=%p\n", this, pHaltState);

        if (!CanDispatchHalt(pHaltState))
        {
            return;
        }

        Assert(pHaltState->stopType == STOP_INLINEBREAKPOINT);

        __try
        {
            InitializeLocation(pHaltState);
            OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchInlineBreakpoint: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n",
                pHaltState, pHaltState->IsValid());

            Assert(pHaltState->IsValid());

            // The ByteCodeReader should be available at this point, but because of possibility of garbled frame, we shouldn't hit AV
            if (pHaltState->IsValid())
            {
#if DBG
                pHaltState->GetFunction()->MustBeInDebugMode();
#endif

                // an inline breakpoint is being dispatched deactivate other stopping controllers
                debugManager->stepController.Deactivate(pHaltState);
                debugManager->asyncBreakController.Deactivate();

                pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                haltCallbackProbe->DispatchHalt(pHaltState);
            }
        }
        __finally
        {
            DestroyLocation();
        }
        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchInlineBreakpoint: end: pHaltState=%p\n", pHaltState);
    }

    bool ProbeContainer::DispatchExceptionBreakpoint(InterpreterHaltState* pHaltState)
    {
        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchExceptionBreakpoint: start: this=%p, pHaltState=%p\n", this, pHaltState);
        bool fSuccess = false;
        if (!haltCallbackProbe || haltCallbackProbe->IsInClosedState() || debugManager->IsAtDispatchHalt())
        {
            OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchExceptionBreakpoint: not in break mode: pHaltState=%p\n", pHaltState);
            // Will not be able to handle multiple break-hits.
            return fSuccess;
        }

        Assert(pHaltState->stopType == STOP_EXCEPTIONTHROW);

        jsExceptionObject = pHaltState->exceptionObject->GetThrownObject(nullptr);

        // Will store current offset of the bytecode block.
        int currentOffset = -1;

        __try
        {
            InitializeLocation(pHaltState, false);
            OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchExceptionBreakpoint: initialized location: pHaltState=%p, IsInterpreterFrame=%d\n",
                pHaltState, pHaltState->IsValid(), pHaltState->topFrame && pHaltState->topFrame->IsInterpreterFrame());

            // The ByteCodeReader should be available at this point, but because of possibility of garbled frame, we shouldn't hit AV
            if (pHaltState->IsValid() && pHaltState->GetFunction()->GetScriptContext()->IsInDebugMode())
            {
#if DBG
                pHaltState->GetFunction()->MustBeInDebugMode();
#endif

                // For interpreter frames, change the current location pointer of bytecode block, as it might be pointing to the next statement on the body.
                // In order to generated proper binding of break on exception to the statement, the bytecode offset needed to be on the same span
                // of the statement.
                // For native frames the offset is always current.
                // Move back a single byte to ensure that it falls under on the same statement.
                if (pHaltState->topFrame->IsInterpreterFrame())
                {
                    currentOffset = pHaltState->GetCurrentOffset();
                    Assert(currentOffset > 0);
                    pHaltState->SetCurrentOffset(currentOffset - 1);
                }

                // an inline breakpoint is being dispatched deactivate other stopping controllers
                debugManager->stepController.Deactivate(pHaltState);
                debugManager->asyncBreakController.Deactivate();

                pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                ScriptContext *pTopFuncContext = pHaltState->GetFunction()->GetScriptContext();

                // If the top function's context is different from the current context, that means current frame is not alive anymore and breaking here cannot not happen.
                // So in that case we will consider the top function's context and break on that context.
                if (pTopFuncContext != pScriptContext)
                {
                    OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchExceptionBreakpoint: top function's context is different from the current context: pHaltState=%p, haltCallbackProbe=%p\n",
                        pHaltState, pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe);
                    if (pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe)
                    {
                        pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe->DispatchHalt(pHaltState);
                        fSuccess = true;
                    }
                }
                else
                {
                    haltCallbackProbe->DispatchHalt(pHaltState);
                    fSuccess = true;
                }
            }
        }
        __finally
        {
            // If the next statement has changed, we need to log that to exception object so that it will not try to advance to next statement again.
            pHaltState->exceptionObject->SetIgnoreAdvanceToNextStatement(IsNextStatementChanged);

            // Restore the current offset;
            if (currentOffset != -1 && pHaltState->topFrame->IsInterpreterFrame())
            {
                pHaltState->SetCurrentOffset(currentOffset);
            }

            DestroyLocation();
        }

        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchExceptionBreakpoint: end: pHaltState=%p, fSuccess=%d\n", pHaltState, fSuccess);
        return fSuccess;
    }

    void ProbeContainer::DispatchMutationBreakpoint(InterpreterHaltState* pHaltState)
    {
        Assert(pHaltState->stopType == STOP_MUTATIONBREAKPOINT);

        OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchMutationBreakpoint: start: this=%p, pHaltState=%p\n", this, pHaltState);
        if (!CanDispatchHalt(pHaltState))
        {
            return;
        }

        // will store Current offset of the bytecode block.
        int currentOffset = -1;

        __try
        {
            InitializeLocation(pHaltState);
            OUTPUT_TRACE(Js::DebuggerPhase, L"ProbeContainer::DispatchMutationBreakpoint: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n",
                pHaltState, pHaltState->IsValid());

            if (pHaltState->IsValid())
            {
                // For interpreter frames, change the current location pointer of bytecode block, as it might be pointing to the next statement on the body.
                // In order to generated proper binding of mutation statement, the bytecode offset needed to be on the same span of the statement.
                // For native frames the offset is always current.
                // Move back a single byte to ensure that it falls under on the same statement.
                if (pHaltState->topFrame->IsInterpreterFrame())
                {
                    currentOffset = pHaltState->GetCurrentOffset();
                    Assert(currentOffset > 0);
                    pHaltState->SetCurrentOffset(currentOffset - 1);
                }
                debugManager->stepController.Deactivate(pHaltState);
                debugManager->asyncBreakController.Deactivate();

                pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                Assert(pHaltState->GetFunction()->GetScriptContext() == pScriptContext);

                haltCallbackProbe->DispatchHalt(pHaltState);
            }
        }
        __finally
        {
            // Restore the current offset;
            if (currentOffset != -1 && pHaltState->topFrame->IsInterpreterFrame())
            {
                pHaltState->SetCurrentOffset(currentOffset);
            }
            DestroyLocation();
        }

    }

    void ProbeContainer::DispatchProbeHandlers(InterpreterHaltState* pHaltState)
    {
        if (!CanDispatchHalt(pHaltState))
        {
            return;
        }

         __try
        {
            InitializeLocation(pHaltState);

            if (pHaltState->IsValid())
            {
                Js::ProbeList * localPendingProbeList = this->pendingProbeList;
                diagProbeList->Map([pHaltState, localPendingProbeList](int index, Probe * probe)
                {
                    if (probe->CanHalt(pHaltState))
                    {
                        localPendingProbeList->Add(probe);
                    }
                });

                if (localPendingProbeList->Count() == 0)
                {
                    // The breakpoint could have been initiated by hybrid debugging
                    if (Js::Configuration::Global.IsHybridDebugging())
                    {
                        debugManager->stepController.Deactivate(pHaltState);
                        debugManager->asyncBreakController.Deactivate();
                        haltCallbackProbe->DispatchHalt(pHaltState);
                    }
                }
                else
                {
                    localPendingProbeList->MapUntil([&](int index, Probe * probe)
                    {
                        if (haltCallbackProbe && !haltCallbackProbe->IsInClosedState())
                        {
                            debugManager->stepController.Deactivate(pHaltState);
                            debugManager->asyncBreakController.Deactivate();
                            haltCallbackProbe->DispatchHalt(pHaltState);
                        }
                        // If SetNextStatement happened between multiple BPs on same location, IP changed so rest of dispatch are not valid.
                        return this->IsSetNextStatementCalled();
                    });
                }
            }
        }
        __finally
        {
            pendingProbeList->Clear();
            DestroyLocation();
        }
    }

    void ProbeContainer::UpdateStep(bool fDuringSetupDebugApp/*= false*/)
    {
        // This function indicate that when the page is being refreshed and the last action we have done was stepping.
        // so update the state of the current stepController.
        if (debugManager)
        {
            // Usually we need to be in debug mode to UpdateStep. But during setting up new engine to debug mode we have an
            // ordering issue and the new engine will enter debug mode after this. So allow non-debug mode if fDuringSetupDebugApp.
            AssertMsg(fDuringSetupDebugApp || (pScriptContext && pScriptContext->IsInDebugMode()), "Why UpdateStep when we are not in debug mode?");
            debugManager->stepController.stepType = STEP_IN;
        }
    }

    void ProbeContainer::DeactivateStep()
    {
        if (debugManager)
        {
            debugManager->stepController.stepType = STEP_NONE;
        }
    }

    void ProbeContainer::InitializeInlineBreakEngine(HaltCallback* probe)
    {
        AssertMsg(!haltCallbackProbe || probe == haltCallbackProbe, "Overwrite of Inline bp probe with different probe");
        haltCallbackProbe = probe;
    }

    void ProbeContainer::UninstallInlineBreakpointProbe(HaltCallback* probe)
    {
        haltCallbackProbe = nullptr;
    }

    void ProbeContainer::InitializeDebuggerScriptOptionCallback(DebuggerOptionsCallback* debuggerOptionsCallback)
    {
        Assert(this->debuggerOptionsCallback == nullptr);
        this->debuggerOptionsCallback = debuggerOptionsCallback;
    }

    void ProbeContainer::UninstallDebuggerScriptOptionCallback()
    {
        this->debuggerOptionsCallback = nullptr;
    }

    void ProbeContainer::AddProbe(Probe* pProbe)
    {
        if (pProbe->Install(nullptr))
        {
            diagProbeList->Add(pProbe);
        }
    }

    void ProbeContainer::RemoveProbe(Probe* pProbe)
    {
        if (pProbe->Uninstall(nullptr))
        {
            diagProbeList->Remove(pProbe);
        }
    }

    void ProbeContainer::RemoveAllProbes()
    {
#ifdef ENABLE_MUTATION_BREAKPOINT
        if (HasMutationBreakpoints())
        {
            ClearMutationBreakpoints();
        }
#endif
        for (int i = 0; i < diagProbeList->Count(); i++)
        {
            diagProbeList->Item(i)->Uninstall(nullptr);
        }
        diagProbeList->Clear();
    }

    // Retrieves the offset of next statement in JavaScript user code for advancing from current statement
    // (normal flow-control is respected).
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetForAdvance(Js::FunctionBody* functionBody, ByteCodeReader* reader, int currentOffset, int* nextStatementOffset)
    {
        int originalCurrentOffset = currentOffset;
        while (GetNextUserStatementOffsetHelper(functionBody, currentOffset, FunctionBody::SAT_FromCurrentToNext, nextStatementOffset))
        {
            Js::DebuggerScope *debuggerScope = functionBody->GetDiagCatchScopeObjectAt(currentOffset);
            if (debuggerScope != nullptr && !debuggerScope->IsOffsetInScope(*nextStatementOffset))
            {
                // Our next statement is not within this catch block, So we cannot just jump to it, we need to return false so the stack unwind will happen.
                return false;
            }

            Assert(currentOffset < *nextStatementOffset);

            if (IsTmpRegCountIncreased(functionBody, reader, originalCurrentOffset, *nextStatementOffset, true /*restoreOffset*/))
            {
                currentOffset = *nextStatementOffset;
            }
            else
            {
                return true;
            }
        }

        return false;
    }

    // Retrieves the offset of beginning of next statement in JavaScript user code for explicit set next statement
    // (normal flow-control is not respected, just get start next statement).
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetForSetNext(Js::FunctionBody* functionBody, int currentOffset, int* nextStatementOffset)
    {
        return GetNextUserStatementOffsetHelper(functionBody, currentOffset, FunctionBody::SAT_NextStatementStart, nextStatementOffset);
    }

    // Retrieves the offset of beginning of next statement in JavaScript user code for scenario specified by adjType.
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetHelper(
        Js::FunctionBody* functionBody, int currentOffset, FunctionBody::StatementAdjustmentType adjType, int* nextStatementOffset)
    {
        Assert(functionBody);
        Assert(nextStatementOffset);

        FunctionBody::StatementMapList* pStatementMaps = functionBody->GetStatementMaps();
        if (pStatementMaps && pStatementMaps->Count() > 1)
        {
            for (int index = 0; index < pStatementMaps->Count() - 1; index++)
            {
                FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);

                if (!pStatementMap->isSubexpression && pStatementMap->byteCodeSpan.Includes(currentOffset))
                {
                    int nextMapIndex = index;
                    FunctionBody::StatementMap* pNextStatementMap = Js::FunctionBody::GetNextNonSubexpressionStatementMap(pStatementMaps, ++nextMapIndex);
                    if (!pNextStatementMap)
                    {
                        break;
                    }

                    // We are trying to find out the Branch opcode, between current and next statement. Skipping that would give use incorrect execution order.
                    FunctionBody::StatementAdjustmentRecord adjRecord;
                    if (pNextStatementMap->byteCodeSpan.begin > pStatementMap->byteCodeSpan.end &&
                        functionBody->GetBranchOffsetWithin(pStatementMap->byteCodeSpan.end, pNextStatementMap->byteCodeSpan.begin, &adjRecord) &&
                        (adjRecord.GetAdjustmentType() & adjType))
                    {
                        Assert(adjRecord.GetByteCodeOffset() > (uint)pStatementMap->byteCodeSpan.end);
                        *nextStatementOffset = adjRecord.GetByteCodeOffset();
                    }
                    else
                    {
                        *nextStatementOffset = pNextStatementMap->byteCodeSpan.begin;
                    }
                    return true;
                }
            }
        }

        *nextStatementOffset = -1;
        return false;
    }

    bool ProbeContainer::FetchTmpRegCount(Js::FunctionBody * functionBody, Js::ByteCodeReader * reader, int atOffset, uint32 *pTmpRegCount, Js::OpCode *pOp)
    {
        Assert(pTmpRegCount);
        Assert(pOp);

        Js::LayoutSize layoutSize;
        reader->SetCurrentOffset(atOffset);
        *pOp = reader->ReadOp(layoutSize);

        if (*pOp == Js::OpCode::Break)
        {
            // User might have put breakpoint on the skipped or target statement, get the original opcode;
            if (functionBody->ProbeAtOffset(atOffset, pOp))
            {
                if (Js::OpCodeUtil::IsPrefixOpcode(*pOp))
                {
                    *pOp = reader->ReadPrefixedOp(layoutSize, *pOp);
                }
            }
        }

        if (*pOp == Js::OpCode::EmitTmpRegCount)
        {
            switch (layoutSize)
            {
            case Js::SmallLayout:
            {
                const unaligned Js::OpLayoutReg1_Small * playout = reader->Reg1_Small();
                *pTmpRegCount = (uint32)playout->R0;
            }
                break;
            case Js::MediumLayout:
            {
                const unaligned Js::OpLayoutReg1_Medium * playout = reader->Reg1_Medium();
                *pTmpRegCount = (uint32)playout->R0;
            }
                break;
            case Js::LargeLayout:
            {
                const unaligned Js::OpLayoutReg1_Large * playout = reader->Reg1_Large();
                *pTmpRegCount = (uint32)playout->R0;
            }
                break;
            default:
                Assert(false);
                __assume(false);
            }
            return true;
        }
        return false;
    }

    // The logic below makes use of number of tmp (temp) registers of A and B.
    // Set next statement is not allowed.
    // if numberOfTmpReg(A) < numberOfTmpReg(B)
    // or if any statement between A and B has number of tmpReg more than the lowest found.
    //
    // Get the temp register count for the A
    // This is a base and will store the lowest tmp reg count we have got yet, while walking the skipped statements.
    bool ProbeContainer::IsTmpRegCountIncreased(Js::FunctionBody* functionBody, ByteCodeReader* reader, int currentOffset, int nextStmOffset, bool restoreOffset)
    {
        Js::FunctionBody::StatementMapList* pStatementMaps = functionBody->GetStatementMaps();
        Assert(pStatementMaps && pStatementMaps->Count() > 0);

        int direction = currentOffset < nextStmOffset ? 1 : -1;
        int startIndex = functionBody->GetEnclosingStatementIndexFromByteCode(currentOffset, true);
        uint32 tmpRegCountLowest = 0;

        // In the native code-gen (or interpreter which created from bailout points) the EmitTmpRegCount is not handled,
        // so lets calculate it by going through all statements backward from the current offset
        int index = startIndex;
        for (; index > 0; index--)
        {
            Js::FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);
            Js::OpCode op;
            if (!pStatementMap->isSubexpression && FetchTmpRegCount(functionBody, reader, pStatementMap->byteCodeSpan.begin, &tmpRegCountLowest, &op))
            {
                break;
            }
        }

        // Reset to the current offset.
        reader->SetCurrentOffset(currentOffset);

        uint32 tmpRegCountOnNext = tmpRegCountLowest; // Will fetch the tmp reg count till the B and skipped statements.
        Assert(startIndex != -1);
        index = startIndex + direction;
        while (index > 0 && index < pStatementMaps->Count())
        {
            Js::FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);
            if (pStatementMap->isSubexpression)
            {
                index += direction;
                continue;
            }

            if (direction == 1) // NOTE: Direction & corresponding condition
            {
                if (nextStmOffset < pStatementMap->byteCodeSpan.begin) // check only till nextstatement offset
                {
                    break;
                }
            }

            Js::OpCode op;
            FetchTmpRegCount(functionBody, reader, pStatementMap->byteCodeSpan.begin, &tmpRegCountOnNext, &op);

            if (tmpRegCountOnNext < tmpRegCountLowest)
            {
                tmpRegCountLowest = tmpRegCountOnNext;
            }

            // On the reverse direction stop only when we find the tmpRegCount info for the setnext or below.
            if (direction == -1 && (op == Js::OpCode::EmitTmpRegCount))
            {
                if (nextStmOffset >= pStatementMap->byteCodeSpan.begin)
                {
                    break;
                }
            }
            index += direction;
        }

        // On the reverse way if we have reached the first statement, then our tmpRegCountOnNext is 0.
        if (direction == -1 && index == 0)
        {
            tmpRegCountOnNext = 0;
        }

        if (restoreOffset)
        {
            // Restore back the original IP.
            reader->SetCurrentOffset(currentOffset);
        }

        return (tmpRegCountOnNext > tmpRegCountLowest);
    }

    bool ProbeContainer::AdvanceToNextUserStatement(Js::FunctionBody* functionBody, ByteCodeReader* reader)
    {
        // Move back a byte to make sure we are within the bounds of
        // our current statement (See DispatchExceptionBreakpoint)
        int currentOffset = reader->GetCurrentOffset() - 1;
        int nextStatementOffset;

        if (this->GetNextUserStatementOffsetForAdvance(functionBody, reader, currentOffset, &nextStatementOffset))
        {
            reader->SetCurrentOffset(nextStatementOffset);
            return true;
        }
        return false;
    }

    void ProbeContainer::SetNextStatementAt(int _bytecodeOffset)
    {
        Assert(_bytecodeOffset != debugManager->pCurrentInterpreterLocation->GetCurrentOffset());
        this->bytecodeOffset = _bytecodeOffset;

        Assert(IsNextStatementChanged == false);
        this->IsNextStatementChanged = true;
    }

    void ProbeContainer::AsyncActivate(HaltCallback* haltCallback)
    {
        OUTPUT_TRACE(Js::DebuggerPhase, L"Async break activated\n");
        InterlockedExchangePointer((PVOID*)&this->pAsyncHaltCallback, haltCallback);

        Assert(debugManager);
        debugManager->asyncBreakController.Activate(haltCallback);
    }

    void ProbeContainer::AsyncDeactivate()
    {
        InterlockedExchangePointer((PVOID*)&this->pAsyncHaltCallback, nullptr);

        Assert(debugManager);
        debugManager->asyncBreakController.Deactivate();
    }

    void ProbeContainer::PrepDiagForEnterScript()
    {
        // This will be called from ParseScriptText.
        // This is to ensure the every script will call EnterScript back to host once, in-order to synchronize PDM with document.
        Assert(this->pScriptContext);
        if (this->pScriptContext->IsInDebugMode())
        {
            isForcedToEnterScriptStart = true;
        }
    }

    void ProbeContainer::RegisterContextToDiag(DWORD_PTR context, ArenaAllocator *alloc)
    {
        Assert(this->pScriptContext->IsInSourceRundownMode() || this->pScriptContext->IsInDebugMode());
        Assert(alloc);

        if (registeredFuncContextList == nullptr)
        {
            registeredFuncContextList = JsUtil::List<DWORD_PTR, ArenaAllocator>::New(alloc);
        }

        registeredFuncContextList->Add(context);
    }

    bool ProbeContainer::IsContextRegistered(DWORD_PTR context)
    {
        return registeredFuncContextList != nullptr && registeredFuncContextList->Contains(context);
    }

    FunctionBody * ProbeContainer::GetGlobalFunc(ScriptContext* scriptContext, DWORD_PTR secondaryHostSourceContext)
    {
        return scriptContext->FindFunction([&secondaryHostSourceContext] (FunctionBody* pFunc) {
            return ((pFunc->GetSecondaryHostSourceContext() == secondaryHostSourceContext) &&
                     pFunc->GetIsGlobalFunc());
        });
    }

    bool ProbeContainer::HasAllowedForException(__in JavascriptExceptionObject* exceptionObject)
    {
        // We do not want to break on internal exception.
        if (isThrowInternal)
        {
            return false;
        }

        bool fIsFirstChance = false;
        bool fHasAllowed = false;
        bool fIsInNonUserCode = false;

        if (debugManager != nullptr)
        {
            fHasAllowed = !debugManager->pThreadContext->HasCatchHandler();
            if (!fHasAllowed)
            {
                if (IsFirstChanceExceptionEnabled())
                {
                    fHasAllowed = fIsFirstChance = true;
                }

                // We must determine if the exception is in user code AND if it's first chance as some debuggers
                // ask for both and filter later.

                // first validate if the throwing function is NonUserCode function, if not then verify if the exception is being caught in nonuser code.
                if (exceptionObject && exceptionObject->GetFunctionBody() != nullptr && !exceptionObject->GetFunctionBody()->IsNonUserCode())
                {
                    fIsInNonUserCode = IsNonUserCodeSupportEnabled() && !debugManager->pThreadContext->IsUserCode();
                }

                if (!fHasAllowed)
                {
                    fHasAllowed = fIsInNonUserCode;
                }
            }
        }

        if (exceptionObject)
        {
            exceptionObject->SetIsFirstChance(fIsFirstChance);
            exceptionObject->SetIsExceptionCaughtInNonUserCode(fIsInNonUserCode);
        }

        return fHasAllowed;
    }

    bool ProbeContainer::IsFirstChanceExceptionEnabled()
    {
        return this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsFirstChanceExceptionEnabled();
    }

    // Mentions if the debugger has enabled the support to differentiate the exception kind.
    bool ProbeContainer::IsNonUserCodeSupportEnabled()
    {
        return this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsNonUserCodeSupportEnabled();
    }

    // Mentions if the debugger has enabled the support to display library stack frame.
    bool ProbeContainer::IsLibraryStackFrameSupportEnabled()
    {
        return CONFIG_FLAG(LibraryStackFrameDebugger) || (this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsLibraryStackFrameSupportEnabled());
    }

    void ProbeContainer::PinPropertyRecord(const Js::PropertyRecord *propertyRecord)
    {
        Assert(propertyRecord);
        this->pinnedPropertyRecords->Add(propertyRecord);
    }
#ifdef ENABLE_MUTATION_BREAKPOINT
    bool ProbeContainer::HasMutationBreakpoints()
    {
        return mutationBreakpointList && !mutationBreakpointList->Empty();
    }

    void ProbeContainer::InsertMutationBreakpoint(MutationBreakpoint *mutationBreakpoint)
    {
        Assert(mutationBreakpoint);

        RecyclerWeakReference<Js::MutationBreakpoint>* weakBp = nullptr;
        pScriptContext->GetRecycler()->FindOrCreateWeakReferenceHandle(mutationBreakpoint, &weakBp);
        Assert(weakBp);

        // Make sure list is created prior to insertion
        InitMutationBreakpointListIfNeeded();
        if (mutationBreakpointList->Contains(weakBp))
        {
            return;
        }
        mutationBreakpointList->Add(weakBp);
    }

    void ProbeContainer::ClearMutationBreakpoints()
    {
        mutationBreakpointList->Map([=](uint i, RecyclerWeakReference<Js::MutationBreakpoint>* weakBp) {
            if (mutationBreakpointList->IsItemValid(i))
            {
                Js::MutationBreakpoint* mutationBreakpoint = weakBp->Get();
                if (mutationBreakpoint)
                {
                    mutationBreakpoint->Reset();
                }
            }
        });
        mutationBreakpointList->ClearAndZero();
    }

    void ProbeContainer::InitMutationBreakpointListIfNeeded()
    {
        if (!mutationBreakpointList && Js::MutationBreakpoint::IsFeatureEnabled(pScriptContext))
        {
            Recycler *recycler = pScriptContext->GetRecycler();
            mutationBreakpointList.Root(RecyclerNew(recycler, MutationBreakpointList, recycler), recycler);
        }
    }

    void ProbeContainer::RemoveMutationBreakpointListIfNeeded()
    {
        if (mutationBreakpointList)
        {
            if (HasMutationBreakpoints())
            {
                ClearMutationBreakpoints();
            }
            else
            {
                mutationBreakpointList->ClearAndZero();
            }
            mutationBreakpointList.Unroot(pScriptContext->GetRecycler());
        }
    }
#endif
} // namespace Js.
