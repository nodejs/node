//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "Base\ScriptContextProfiler.h"

#if DBG
Js::JavascriptMethod checkCodeGenThunk;
#endif

#ifdef ENABLE_PREJIT
#define IS_PREJIT_ON() (Js::Configuration::Global.flags.Prejit)
#else
#define IS_PREJIT_ON() (DEFAULT_CONFIG_Prejit)
#endif

#define ASSERT_THREAD() AssertMsg(mainThreadId == GetCurrentThreadContextId(), \
    "Cannot use this member of native code generator from thread other than the creating context's current thread")

NativeCodeGenerator::NativeCodeGenerator(Js::ScriptContext * scriptContext)
:   JsUtil::WaitableJobManager(scriptContext->GetThreadContext()->GetJobProcessor()),
    scriptContext(scriptContext),
    pendingCodeGenWorkItems(0),
    queuedFullJitWorkItemCount(0),
    foregroundAllocators(nullptr),
    backgroundAllocators(nullptr),
    byteCodeSizeGenerated(0),
    isClosed(false),
    isOptimizedForManyInstances(scriptContext->GetThreadContext()->IsOptimizedForManyInstances()),
    SetNativeEntryPoint(Js::FunctionBody::DefaultSetNativeEntryPoint),
    freeLoopBodyManager(scriptContext->GetThreadContext()->GetJobProcessor()),
    hasUpdatedQForDebugMode(false)
#ifdef PROFILE_EXEC
    , foregroundCodeGenProfiler(nullptr)
    , backgroundCodeGenProfiler(nullptr)
#endif
{
    freeLoopBodyManager.SetNativeCodeGen(this);

#if DBG_DUMP
    if (Js::Configuration::Global.flags.IsEnabled(Js::AsmDumpModeFlag)
        && (Js::Configuration::Global.flags.AsmDumpMode != nullptr))
    {
        bool fileOpened = false;
        fileOpened = (0 == _wfopen_s(&this->asmFile, Js::Configuration::Global.flags.AsmDumpMode, L"wt"));
        if (!fileOpened)
        {
            size_t len = wcslen(Js::Configuration::Global.flags.AsmDumpMode);
            if (len < _MAX_PATH - 5)
            {
                wchar_t filename[_MAX_PATH];
                wcscpy_s(filename, _MAX_PATH, Js::Configuration::Global.flags.AsmDumpMode);
                wchar_t * number = filename + len;
                for (int i = 0; i < 1000; i++)
                {
                    _itow_s(i, number, 5, 10);
                    fileOpened = (0 == _wfopen_s(&this->asmFile, filename, L"wt"));
                    if (fileOpened)
                    {
                        break;
                    }
                }
            }

            if (!fileOpened)
            {
                this->asmFile = nullptr;
                AssertMsg(0, "Could not open file for AsmDump. The output will goto standard console");
            }
        }

    }
    else
    {
        this->asmFile = nullptr;
    }
#endif

#if DBG
    this->mainThreadId = GetCurrentThreadContextId();
#endif

    Processor()->AddManager(this);
    this->freeLoopBodyManager.SetAutoClose(false);
}

NativeCodeGenerator::~NativeCodeGenerator()
{
    Assert(this->IsClosed());

#ifdef PROFILE_EXEC
    if (this->foregroundCodeGenProfiler != nullptr)
    {
        this->foregroundCodeGenProfiler->Release();
    }
#endif

    if(this->foregroundAllocators != nullptr)
    {
        HeapDelete(this->foregroundAllocators);
    }


    if (this->backgroundAllocators)
    {
#if DBG
        // PageAllocator is thread agile. This destructor can be called from background GC thread.
        // We have already removed this manager from the job queue and hence its fine to set the threadId to -1.
        // We can't DissociatePageAllocator here as its allocated ui thread.
        //this->Processor()->DissociatePageAllocator(allocator->GetPageAllocator());
        this->backgroundAllocators->emitBufferManager.GetHeapPageAllocator()->ClearConcurrentThreadId();
        this->backgroundAllocators->emitBufferManager.GetPreReservedHeapPageAllocator()->ClearConcurrentThreadId();
        this->backgroundAllocators->GetPageAllocator()->ClearConcurrentThreadId();
#endif
        // The native code generator may be deleted after Close was called on the job processor. In that case, the
        // background thread is no longer running, so clean things up in the foreground.
        HeapDelete(this->backgroundAllocators);
    }

#ifdef PROFILE_EXEC
    if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
    {
        while (this->backgroundCodeGenProfiler)
        {
            Js::ScriptContextProfiler *codegenProfiler = this->backgroundCodeGenProfiler;
            this->backgroundCodeGenProfiler = this->backgroundCodeGenProfiler->next;
            codegenProfiler->Release();
        }
    }
    else
    {
        Assert(this->backgroundCodeGenProfiler == nullptr);
    }
#endif

}

void NativeCodeGenerator::Close()
{
    Assert(!this->IsClosed());

    // Close FreeLoopBodyJobManager first, as it depends on NativeCodeGenerator to be open before it's removed
    this->freeLoopBodyManager.Close();

    // Remove only if it is not updated in the debug mode (and which goes to interpreter mode).
    if (!hasUpdatedQForDebugMode || Js::Configuration::Global.EnableJitInDebugMode())
    {
        Processor()->RemoveManager(this);
    }

    this->isClosed = true;

    Assert(!queuedFullJitWorkItems.Head());
    Assert(queuedFullJitWorkItemCount == 0);

    for(JsUtil::Job *job = workItems.Head(); job;)
    {
        JsUtil::Job *const next = job->Next();
        JobProcessed(job, /*succeeded*/ false);
        job = next;
    }
    workItems.Clear();

    // Only decommit here instead of releasing the memory, so we retain control over these addresses
    // Mitigate against the case the entry point is called after the script site is closed
    if (this->backgroundAllocators)
    {
        this->backgroundAllocators->emitBufferManager.Decommit();
    }

    if (this->foregroundAllocators)
    {
        this->foregroundAllocators->emitBufferManager.Decommit();
    }

#if DBG_DUMP
    if (this->asmFile != nullptr)
    {
        if(0 != fclose(this->asmFile))
        {
            AssertMsg(0, "Could not close file for AsmDump. You may ignore this warning.");
        }
    }
#endif
}

#if DBG_DUMP
extern Func *CurrentFunc;
#endif

JsFunctionCodeGen *
NativeCodeGenerator::NewFunctionCodeGen(Js::FunctionBody *functionBody, Js::EntryPointInfo* info)
{
    return HeapNewNoThrow(JsFunctionCodeGen, this, functionBody, info, this->IsInDebugMode());
}

JsLoopBodyCodeGen *
NativeCodeGenerator::NewLoopBodyCodeGen(Js::FunctionBody *functionBody, Js::EntryPointInfo* info)
{
    return HeapNewNoThrow(JsLoopBodyCodeGen, this, functionBody, info, this->IsInDebugMode());
}

#ifdef ENABLE_PREJIT
bool
NativeCodeGenerator::DoBackEnd(Js::FunctionBody *fn)
{
    if (PHASE_OFF(Js::BackEndPhase, fn))
    {
        return false;
    }
    if (fn->IsAsmJSModule() || fn->IsGeneratorAndJitIsDisabled())
    {
        return false;
    }
    return true;
}

void
NativeCodeGenerator::GenerateAllFunctions(Js::FunctionBody * fn)
{
    Assert(IS_PREJIT_ON());
    Assert(fn->GetDefaultFunctionEntryPointInfo()->entryPointIndex == 0);

    // Make sure this isn't a deferred function
    Assert(fn->GetFunctionBody() == fn);
    Assert(!fn->IsDeferred());

    if (DoBackEnd(fn))
    {
        if (fn->GetLoopCount() != 0 && fn->ForceJITLoopBody() && !IsInDebugMode())
        {
            // Only jit the loop body with /force:JITLoopBody

            for (uint i = 0; i < fn->GetLoopCount(); i++)
            {
                Js::LoopHeader * loopHeader = fn->GetLoopHeader(i);
                Js::EntryPointInfo * entryPointInfo = loopHeader->GetCurrentEntryPointInfo();
                this->GenerateLoopBody(fn, loopHeader, entryPointInfo);
            }
        }
        else
        {
            // A JIT attempt should have already been made through GenerateFunction
            Assert(!fn->GetDefaultFunctionEntryPointInfo()->IsNotScheduled());
        }
    }
    for (uint i = 0; i < fn->GetNestedCount(); i++)
    {
        Js::FunctionBody* functionToJIT = fn->GetNestedFunctionForExecution(i)->GetFunctionBody();
        GenerateAllFunctions(functionToJIT);
    }
}
#endif

#if _M_ARM
USHORT ArmExtractThumbImmediate16(PUSHORT address)
{
    return ((address[0] << 12) & 0xf000) |  // bits[15:12] in OP0[3:0]
           ((address[0] <<  1) & 0x0800) |  // bits[11]    in OP0[10]
           ((address[1] >>  4) & 0x0700) |  // bits[10:8]  in OP1[14:12]
           ((address[1] >>  0) & 0x00ff);   // bits[7:0]   in OP1[7:0]
}

void ArmInsertThumbImmediate16(PUSHORT address, USHORT immediate)
{
    USHORT opcode0;
    USHORT opcode1;

    opcode0 = address[0];
    opcode1 = address[1];
    opcode0 &= ~((0xf000 >> 12) | (0x0800 >> 1));
    opcode1 &= ~((0x0700 <<  4) | (0x00ff << 0));
    opcode0 |= (immediate & 0xf000) >> 12;   // bits[15:12] in OP0[3:0]
    opcode0 |= (immediate & 0x0800) >>  1;   // bits[11]    in OP0[10]
    opcode1 |= (immediate & 0x0700) <<  4;   // bits[10:8]  in OP1[14:12]
    opcode1 |= (immediate & 0x00ff) <<  0;   // bits[7:0]   in OP1[7:0]
    address[0] = opcode0;
    address[1] = opcode1;
}
#endif

void DoFunctionRelocations(BYTE *function, DWORD functionOffset, DWORD functionSize, BYTE *module, size_t imageBase, IMAGE_SECTION_HEADER *textHeader, IMAGE_SECTION_HEADER *relocHeader)
{
    PIMAGE_BASE_RELOCATION relocationBlock = (PIMAGE_BASE_RELOCATION)(module + relocHeader->PointerToRawData);
    for (; relocationBlock->VirtualAddress > 0 && ((BYTE *)relocationBlock < (module + relocHeader->PointerToRawData + relocHeader->SizeOfRawData)); )
    {
        DWORD blockOffset = relocationBlock->VirtualAddress - textHeader->VirtualAddress;

        // Skip relocation blocks that are before the function
        if ((blockOffset + 0x1000) > functionOffset)
        {
            unsigned short *relocation = (unsigned short *)((unsigned char *)relocationBlock + sizeof(IMAGE_BASE_RELOCATION));
            for (uint index = 0; index < ((relocationBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2); index++, relocation++)
            {
                int type = *relocation >> 12;
                int offset = *relocation & 0xfff;

                // If we are past the end of the function, we can stop.
                if ((blockOffset + offset) >= (functionOffset + functionSize))
                {
                    break;
                }

                if ((blockOffset + offset) < functionOffset)
                {
                    continue;
                }

                switch (type)
                {
                case IMAGE_REL_BASED_ABSOLUTE:
                    break;

#if _M_IX86
                case IMAGE_REL_BASED_HIGHLOW:
                    {
                        DWORD *patchAddrHL = (DWORD *) (function + blockOffset + offset - functionOffset);
                        DWORD patchAddrHLOffset = *patchAddrHL - imageBase - textHeader->VirtualAddress;
                        Assert((patchAddrHLOffset > functionOffset) && (patchAddrHLOffset < (functionOffset + functionSize)));
                        *patchAddrHL = patchAddrHLOffset - functionOffset + (DWORD)function;
                    }
                    break;

#elif defined(_M_X64_OR_ARM64)
                case IMAGE_REL_BASED_DIR64:
                    {
                        ULONGLONG *patchAddr64 = (ULONGLONG *) (function + blockOffset + offset - functionOffset);
                        ULONGLONG patchAddr64Offset = *patchAddr64 - imageBase - textHeader->VirtualAddress;
                        Assert((patchAddr64Offset > functionOffset) && (patchAddr64Offset < (functionOffset + functionSize)));
                        *patchAddr64 = patchAddr64Offset - functionOffset + (ULONGLONG)function;
                    }
                    break;
#else
                case IMAGE_REL_BASED_THUMB_MOV32:
                    {
                        USHORT *patchAddr = (USHORT *) (function + blockOffset + offset - functionOffset);
                        DWORD address = ArmExtractThumbImmediate16(patchAddr) | (ArmExtractThumbImmediate16(patchAddr + 2) << 16);
                        address = address - imageBase - textHeader->VirtualAddress - functionOffset + (DWORD)function;
                        ArmInsertThumbImmediate16(patchAddr, (USHORT)(address & 0xFFFF));
                        ArmInsertThumbImmediate16(patchAddr + 2, (USHORT)(address >> 16));
                    }
                    break;
#endif

                default:
                    Assert(false);
                    break;
                }
            }
        }

        relocationBlock = (PIMAGE_BASE_RELOCATION) (((BYTE *) relocationBlock) + relocationBlock->SizeOfBlock);
    }
}

class AutoRestoreDefaultEntryPoint
{
public:
    AutoRestoreDefaultEntryPoint(Js::FunctionBody* functionBody):
        functionBody(functionBody)
    {
        this->oldDefaultEntryPoint = functionBody->GetDefaultFunctionEntryPointInfo();
        this->oldOriginalEntryPoint = functionBody->GetOriginalEntryPoint();

        this->newEntryPoint = functionBody->CreateNewDefaultEntryPoint();
    }

    ~AutoRestoreDefaultEntryPoint()
    {
        if (newEntryPoint && !newEntryPoint->IsCodeGenDone())
        {
            functionBody->RestoreOldDefaultEntryPoint(oldDefaultEntryPoint, oldOriginalEntryPoint, newEntryPoint);
        }
    }

private:
    Js::FunctionBody* functionBody;
    Js::FunctionEntryPointInfo* oldDefaultEntryPoint;
    Js::JavascriptMethod oldOriginalEntryPoint;
    Js::FunctionEntryPointInfo* newEntryPoint;
};

//static
void NativeCodeGenerator::Jit_TransitionFromSimpleJit(void *const framePointer)
{
    TransitionFromSimpleJit(
        Js::ScriptFunction::FromVar(Js::JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject));
}

//static
void NativeCodeGenerator::TransitionFromSimpleJit(Js::ScriptFunction *const function)
{
    Assert(function);

    Js::FunctionBody *const functionBody = function->GetFunctionBody();
    Js::FunctionEntryPointInfo *const defaultEntryPointInfo = functionBody->GetDefaultFunctionEntryPointInfo();
    if(defaultEntryPointInfo == functionBody->GetSimpleJitEntryPointInfo())
    {
        Assert(functionBody->GetExecutionMode() == ExecutionMode::SimpleJit);
        Assert(function->GetFunctionEntryPointInfo() == defaultEntryPointInfo);

        // The latest entry point is the simple JIT, transition to the next execution mode and schedule a full JIT
        bool functionEntryPointUpdated = functionBody->GetScriptContext()->GetNativeCodeGenerator()->GenerateFunction(functionBody, function);

        if (functionEntryPointUpdated)
        {
            // Transition to the next execution mode after scheduling a full JIT, in case of OOM before the entry point is changed
            const bool transitioned = functionBody->TryTransitionToNextExecutionMode();
            Assert(transitioned);

            if (PHASE_TRACE(Js::SimpleJitPhase, functionBody))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(
                    L"SimpleJit (TransitionFromSimpleJit): function: %s (%s)",
                    functionBody->GetDisplayName(),
                    functionBody->GetDebugNumberSet(debugStringBuffer));
                Output::Flush();
            }
        }
        return;
    }

    if(function->GetFunctionEntryPointInfo() != defaultEntryPointInfo)
    {
        // A full JIT may have already been scheduled, or some entry point info got expired before the simple JIT entry point
        // was ready. In any case, the function's entry point info is not the latest, so update it.
        function->UpdateThunkEntryPoint(defaultEntryPointInfo, functionBody->GetDirectEntryPoint(defaultEntryPointInfo));
    }
}

#ifdef IR_VIEWER
Js::Var
NativeCodeGenerator::RejitIRViewerFunction(Js::FunctionBody *fn, Js::ScriptContext *requestContext)
{
    /* Note: adapted from NativeCodeGenerator::GenerateFunction (NativeCodeGenerator.cpp) */

    Js::ScriptContext *scriptContext = fn->GetScriptContext();
    PageAllocator *pageAllocator = scriptContext->GetThreadContext()->GetPageAllocator();
    NativeCodeGenerator *nativeCodeGenerator = scriptContext->GetNativeCodeGenerator();

    AutoRestoreDefaultEntryPoint autoRestore(fn);
    Js::FunctionEntryPointInfo * entryPoint = fn->GetDefaultFunctionEntryPointInfo();

    JsFunctionCodeGen workitem(this, fn, entryPoint, this->IsInDebugMode());
    workitem.isRejitIRViewerFunction = true;
    workitem.irViewerRequestContext = scriptContext;

    workitem.SetJitMode(ExecutionMode::FullJit);

    entryPoint->SetCodeGenPendingWithStackAllocatedWorkItem();
    entryPoint->SetCodeGenQueued();
    const auto recyclableData = GatherCodeGenData(fn, fn, entryPoint, &workitem);
    workitem.SetRecyclableData(recyclableData);

    nativeCodeGenerator->CodeGen(pageAllocator, &workitem, true);

    return Js::CrossSite::MarshalVar(requestContext, workitem.GetIRViewerOutput(scriptContext));
}
#endif /* IR_VIEWER */

///----------------------------------------------------------------------------
///
/// NativeCodeGenerator::GenerateFunction
///
///     This is the main entry point for the runtime to call the native code
///     generator.
///
///----------------------------------------------------------------------------
bool
NativeCodeGenerator::GenerateFunction(Js::FunctionBody *fn, Js::ScriptFunction * function)
{
    ASSERT_THREAD();
    Assert(!fn->GetIsFromNativeCodeModule());
    Assert(fn->GetScriptContext()->GetNativeCodeGenerator() == this);
    Assert(fn->GetFunctionBody() == fn);
    Assert(!fn->IsDeferred());

#if !defined(_M_ARM64)

    if (fn->IsGeneratorAndJitIsDisabled())
    {
        // JITing generator functions is not complete nor stable yet so it is off by default.
        // Also try/catch JIT support in generator functions is not a goal for threshold
        // release so JITing generators containing try blocks is disabled for now.
        return false;
    }

    if (IsInDebugMode() && fn->GetHasTry())
    {
        // Under debug mode disable JIT for functions that:
        // - have try
        return false;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.Interpret &&
        fn->GetDisplayName() &&
        ::wcsstr(Js::Configuration::Global.flags.Interpret, fn->GetDisplayName()))
    {
        return false;
    }
#endif

    if (fn->GetLoopCount() != 0 && fn->ForceJITLoopBody() && !IsInDebugMode())
    {
        // Don't code gen the function if the function has loop, ForceJITLoopBody is on,
        // unless we are in debug mode in which case JIT loop body is disabled, even if it's forced.
        return false;
    }

    // Create a work item with null entry point- we'll set it once its allocated
    AutoPtr<JsFunctionCodeGen> workItemAutoPtr(this->NewFunctionCodeGen(fn, nullptr));
    if ((JsFunctionCodeGen*) workItemAutoPtr == nullptr)
    {
        // OOM, just skip this work item and return.
        return false;
    }

    Js::FunctionEntryPointInfo* entryPointInfo = nullptr;

    if (function != nullptr)
    {
        entryPointInfo = fn->CreateNewDefaultEntryPoint();
    }
    else
    {
        entryPointInfo = fn->GetDefaultFunctionEntryPointInfo();
        Assert(fn->IsInterpreterThunk() || fn->IsSimpleJitOriginalEntryPoint());
    }
#ifdef ASMJS_PLAT
    if (fn->GetIsAsmjsMode())
    {
        AnalysisAssert(function != nullptr);
        Js::FunctionEntryPointInfo* oldFuncObjEntryPointInfo = (Js::FunctionEntryPointInfo*)function->GetEntryPointInfo();
        Assert(oldFuncObjEntryPointInfo->GetIsAsmJSFunction()); // should be asmjs entrypoint info

        // Set asmjs to be true in entrypoint
        entryPointInfo->SetIsAsmJSFunction(true);
        // Move the ModuleAddress from old Entrypoint to new entry point
        entryPointInfo->SetModuleAddress(oldFuncObjEntryPointInfo->GetModuleAddress());

        // Update the native address of the older entry point - this should be either the TJ entrypoint or the Interpreter Entry point
        entryPointInfo->SetNativeAddress(oldFuncObjEntryPointInfo->address);
        // have a reference to TJ entrypointInfo, this will be queued for collection in checkcodegen
        entryPointInfo->SetOldFunctionEntryPointInfo(oldFuncObjEntryPointInfo);
        Assert(PHASE_ON1(Js::AsmJsJITTemplatePhase) || (!oldFuncObjEntryPointInfo->GetIsTJMode() && !entryPointInfo->GetIsTJMode()));
        // this changes the address in the entrypointinfo to be the AsmJsCodgenThunk
        function->UpdateThunkEntryPoint(entryPointInfo, NativeCodeGenerator::CheckAsmJsCodeGenThunk);

        if (PHASE_TRACE1(Js::AsmjsEntryPointInfoPhase))
            Output::Print(L"New Entrypoint is CheckAsmJsCodeGenThunk for function: %s\n", fn->GetDisplayName());
    }
    else
#endif
    {
        fn->SetCheckCodeGenEntryPoint(entryPointInfo, NativeCodeGenerator::CheckCodeGenThunk);
        if (function != nullptr)
        {
            function->UpdateThunkEntryPoint(entryPointInfo, NativeCodeGenerator::CheckCodeGenThunk);
        }
    }

    JsFunctionCodeGen * workitem = workItemAutoPtr.Detach();
    workitem->SetEntryPointInfo(entryPointInfo);

    entryPointInfo->SetCodeGenPending(workitem);
    InterlockedIncrement(&pendingCodeGenWorkItems);

    if(!IS_PREJIT_ON())
    {
        workItems.LinkToEnd(workitem);
        return true;
    }

    const ExecutionMode prejitJitMode = PrejitJitMode(fn);
    workitem->SetJitMode(prejitJitMode);
    try
    {
        AddToJitQueue(workitem, /*prioritize*/ true, /*lock*/ true, function);
    }
    catch (...)
    {
        // Add the item back to the list if AddToJitQueue throws. The position in the list is not important.
        workitem->ResetJitMode();
        workItems.LinkToEnd(workitem);
        throw;
    }
    fn->TraceExecutionMode("Prejit (before)");
    if(prejitJitMode == ExecutionMode::SimpleJit)
    {
        fn->TransitionToSimpleJitExecutionMode();
    }
    else
    {
        Assert(prejitJitMode == ExecutionMode::FullJit);
        fn->TransitionToFullJitExecutionMode();
    }
    fn->TraceExecutionMode("Prejit");
    Processor()->PrioritizeJobAndWait(this, entryPointInfo, function);
    CheckCodeGenDone(fn, entryPointInfo, function);
    return true;
#else
    return false;
#endif
}

void NativeCodeGenerator::GenerateLoopBody(Js::FunctionBody * fn, Js::LoopHeader * loopHeader, Js::EntryPointInfo* entryPoint, uint localCount, Js::Var localSlots[])
{
    ASSERT_THREAD();
    Assert(fn->GetScriptContext()->GetNativeCodeGenerator() == this);
    Assert(entryPoint->address == nullptr);

#if DBG_DUMP
    if (PHASE_TRACE1(Js::JITLoopBodyPhase))
    {
        fn->DumpFunctionId(true);
        Output::Print(L": %-20s LoopBody Start    Loop: %2d ByteCode: %4d (%4d,%4d)\n", fn->GetDisplayName(), fn->GetLoopNumber(loopHeader),
            loopHeader->endOffset - loopHeader->startOffset, loopHeader->startOffset, loopHeader->endOffset);
        Output::Flush();
    }
#endif

    // If the parent function is JITted, no need to JIT this loop
    // CanReleaseLoopHeaders is a quick and dirty way of checking if the
    // function is currently being interpreted. If it is being interpreted,
    // We'd still like to jit the loop body.
    // We reset the interpretCount to 0 in case we switch back to the interpreter
    if (fn->GetNativeEntryPointUsed() && fn->GetCanReleaseLoopHeaders() && (!fn->GetIsAsmJsFunction() || !(loopHeader->GetCurrentEntryPointInfo()->GetIsTJMode())))
    {
        loopHeader->ResetInterpreterCount();
        return;
    }
    if (fn->GetIsAsmJsFunction())
    {
        Js::FunctionEntryPointInfo* functionEntryPointInfo = (Js::FunctionEntryPointInfo*) fn->GetDefaultEntryPointInfo();
        Js::LoopEntryPointInfo* loopEntryPointInfo = (Js::LoopEntryPointInfo*)entryPoint;
        loopEntryPointInfo->SetIsAsmJSFunction(true);
        loopEntryPointInfo->SetModuleAddress(functionEntryPointInfo->GetModuleAddress());
    }
    JsLoopBodyCodeGen * workitem = this->NewLoopBodyCodeGen(fn, entryPoint);
    if (!workitem)
    {
        // OOM, just skip this work item and return.
        return;
    }

    entryPoint->SetCodeGenPending(workitem);

    workitem->loopHeader = loopHeader;

    try
    {
        if (!fn->GetIsAsmJsFunction()) // not needed for asmjs as we don't profile in asm mode
        {
            const uint profiledRegBegin = fn->GetConstantCount();
            const uint profiledRegEnd = localCount;
            if (profiledRegBegin < profiledRegEnd)
            {
                workitem->symIdToValueTypeMap =
                    HeapNew(JsLoopBodyCodeGen::SymIdToValueTypeMap, &HeapAllocator::Instance, profiledRegEnd - profiledRegBegin);
                Recycler *recycler = fn->GetScriptContext()->GetRecycler();
                for (uint i = profiledRegBegin; i < profiledRegEnd; i++)
                {
                    if (localSlots[i] && IsValidVar(localSlots[i], recycler))
                    {
                        workitem->symIdToValueTypeMap->Add(i, ValueType::Uninitialized.Merge(localSlots[i]));
                    }
                }
            }
        }

        workitem->SetJitMode(ExecutionMode::FullJit);
        AddToJitQueue(workitem, /*prioritize*/ true, /*lock*/ true);
    }
    catch (...)
    {
        // If adding to the JIT queue fails we need to revert the state of the entry point
        // and delete the work item
        entryPoint->RevertToNotScheduled();
        workitem->Delete();

        throw;
    }

    if (!Processor()->ProcessesInBackground() || fn->ForceJITLoopBody())
    {
        Processor()->PrioritizeJobAndWait(this, entryPoint);
    }
}

bool
NativeCodeGenerator::IsValidVar(const Js::Var var, Recycler *const recycler)
{
    using namespace Js;
    Assert(var);
    Assert(recycler);

    // We may be handling uninitialized memory here, need to ensure that each recycler-allocated object is valid before it is
    // read. Virtual functions shouldn't be called because the type ID may match by coincidence but the vtable can still be
    // invalid, even if it is deemed to be a "valid" object, since that only validates that the memory is still owned by the
    // recycler. This function validates the memory that ValueType::Merge(Var) reads.

    if(TaggedInt::Is(var))
    {
        return true;
    }

#if FLOATVAR
    if(JavascriptNumber::Is_NoTaggedIntCheck(var))
    {
        return true;
    }
#endif

    RecyclableObject *const recyclableObject = RecyclableObject::FromVar(var);
    if(!recycler->IsValidObject(recyclableObject, sizeof(*recyclableObject)))
    {
        return false;
    }

    INT_PTR vtable = VirtualTableInfoBase::GetVirtualTable(var);
    if (vtable <= USHRT_MAX || (vtable & 1))
    {
        // Don't have a vtable, is it not a var, may be a frame display?
        return false;
    }

    Type *const type = recyclableObject->GetType();
    if(!recycler->IsValidObject(type, sizeof(*type)))
    {
        return false;
    }

#if !FLOATVAR
    if(JavascriptNumber::Is_NoTaggedIntCheck(var))
    {
        return true;
    }
#endif

    const TypeId typeId = type->GetTypeId();
    if(typeId < static_cast<TypeId>(0))
    {
        return false;
    }
    if(!DynamicType::Is(typeId))
    {
        return true;
    }

    DynamicType *const dynamicType = static_cast<DynamicType *>(type);
    if(!recycler->IsValidObject(dynamicType, sizeof(*dynamicType)))
    {
        return false;
    }

    DynamicTypeHandler *const typeHandler = dynamicType->GetTypeHandler();
    if(!recycler->IsValidObject(typeHandler, sizeof(*typeHandler)))
    {
        return false;
    }

    // Not using DynamicObject::FromVar since there's a virtual call in there
    DynamicObject *const object = static_cast<DynamicObject *>(recyclableObject);
    if(!recycler->IsValidObject(object, sizeof(*object)))
    {
        return false;
    }

    if(typeId != TypeIds_Array)
    {
        ArrayObject* const objectArray = object->GetObjectArrayUnchecked();
        return objectArray == nullptr || recycler->IsValidObject(objectArray, sizeof(*objectArray));
    }

    // Not using JavascriptArray::FromVar since there's a virtual call in there
    JavascriptArray *const array = static_cast<JavascriptArray *>(object);
    if(!recycler->IsValidObject(array, sizeof(*array)))
    {
        return false;
    }

    return true;
}

#if ENABLE_DEBUG_CONFIG_OPTIONS
volatile UINT_PTR NativeCodeGenerator::CodegenFailureSeed = 0;
#endif

void
NativeCodeGenerator::CodeGen(PageAllocator * pageAllocator, CodeGenWorkItem* workItem, const bool foreground)
{
    if(foreground)
    {
        // Func::Codegen has a lot of things on the stack, so probe the stack here instead
        PROBE_STACK(scriptContext, Js::Constants::MinStackJITCompile);
    }

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (!foreground && Js::Configuration::Global.flags.IsEnabled(Js::InduceCodeGenFailureFlag))
    {
        if (NativeCodeGenerator::CodegenFailureSeed == 0)
        {
            // Initialize the seed
            NativeCodeGenerator::CodegenFailureSeed = Js::Configuration::Global.flags.InduceCodeGenFailureSeed;
            if (NativeCodeGenerator::CodegenFailureSeed == 0)
            {
                LARGE_INTEGER ctr;
                ::QueryPerformanceCounter(&ctr);

                NativeCodeGenerator::CodegenFailureSeed = ctr.HighPart ^ ctr.LowPart;
                srand((uint)NativeCodeGenerator::CodegenFailureSeed);
            }
        }

        int v = Math::Rand() % 100;
        if (v < Js::Configuration::Global.flags.InduceCodeGenFailure)
        {
            switch (v % 3)
            {
            case 0: Js::Throw::OutOfMemory(); break;
            case 1: throw Js::StackOverflowException(); break;
            case 2: throw Js::OperationAbortedException(); break;
            default:
                Assert(false);
            }
        }
    }
#endif

    bool irviewerInstance = false;
#ifdef IR_VIEWER
    irviewerInstance = true;
#endif
    Assert(
        workItem->Type() != JsFunctionType ||
        irviewerInstance ||
        IsThunk(workItem->GetFunctionBody()->GetDirectEntryPoint(workItem->GetEntryPoint())) ||
        IsAsmJsCodeGenThunk(workItem->GetFunctionBody()->GetDirectEntryPoint(workItem->GetEntryPoint())));

    InterlockedExchangeAdd(&this->byteCodeSizeGenerated, workItem->GetByteCodeCount()); // must be interlocked because this data may be modified in the foreground and background thread concurrently
    Js::FunctionBody* body = workItem->GetFunctionBody();
    int nRegs = body->GetLocalsCount();
    AssertMsg((nRegs + 1) == (int)(SymID)(nRegs + 1), "SymID too small...");

    CodeGenAllocators *const allocators =
        foreground ? EnsureForegroundAllocators(pageAllocator) : GetBackgroundAllocator(pageAllocator); // okay to do outside lock since the respective function is called only from one thread

    Js::ScriptContextProfiler *const codeGenProfiler =
#ifdef PROFILE_EXEC
        foreground ? EnsureForegroundCodeGenProfiler() : GetBackgroundCodeGenProfiler(pageAllocator); // okay to do outside lock since the respective function is called only from one thread
#else
        nullptr;
#endif

    NoRecoverMemoryJitArenaAllocator funcAlloc(L"BE-FuncAlloc", pageAllocator, Js::Throw::OutOfMemory);
    Js::ReadOnlyDynamicProfileInfo profileInfo(
        body->HasDynamicProfileInfo() ? body->GetAnyDynamicProfileInfo() : nullptr,
        foreground ? nullptr : &funcAlloc);
    bool rejit;
    ThreadContext *threadContext = scriptContext->GetThreadContext();
    double startTime = threadContext->JITTelemetry.Now();

    do
    {
        // the number allocator needs to be on the stack so that if we are doing foreground JIT
        // the chunk allocated from the recycler will be stacked pinned
        CodeGenNumberAllocator numberAllocator(
            foreground? nullptr : scriptContext->GetThreadContext()->GetCodeGenNumberThreadAllocator(),
            scriptContext->GetRecycler());

        Func *func =
            JitAnew(
                (&funcAlloc),
                Func,
                (&funcAlloc),
                workItem,
                nullptr,
                workItem->GetEntryPoint()->GetPolymorphicInlineCacheInfo()->GetSelfInfo(),
                allocators,
                &numberAllocator,
                &profileInfo,
                codeGenProfiler,
                !foreground);
#if DBG_DUMP
        CurrentFunc = func;
#endif
        func->m_symTable->SetStartingID(static_cast<SymID>(nRegs + 1));

        try
        {
            // Although we don't need to release the Arena memory, we need to invoke Func destructor.
            // Use an auto object for it. Put it here to ensure "func" is cleared whenever we have an
            // exception (RejitException or AbortException).
            AutoAllocatorObjectPtr<Func, JitArenaAllocator> autoFunc(func, &funcAlloc);

            func->Codegen();
            rejit = false;
        }
        catch(Js::RejitException ex)
        {
            // The work item needs to be rejitted, likely due to some optimization that was too aggressive
            if(ex.Reason() == RejitReason::AggressiveIntTypeSpecDisabled)
            {
                const bool isJitLoopBody = workItem->Type() == JsLoopBodyWorkItemType;
                profileInfo.DisableAggressiveIntTypeSpec(isJitLoopBody);
                if (body->HasDynamicProfileInfo())
                {
                    body->GetAnyDynamicProfileInfo()->DisableAggressiveIntTypeSpec(isJitLoopBody);
                }
            }
            else if(ex.Reason() == RejitReason::InlineApplyDisabled)
            {
                body->SetDisableInlineApply(true);
            }
            else if(ex.Reason() == RejitReason::InlineSpreadDisabled)
            {
                body->SetDisableInlineSpread(true);
            }
            else if(ex.Reason() == RejitReason::DisableSwitchOptExpectingInteger ||
                ex.Reason() == RejitReason::DisableSwitchOptExpectingString)
            {
                profileInfo.DisableSwitchOpt();
                if(body->HasDynamicProfileInfo())
                {
                    body->GetAnyDynamicProfileInfo()->DisableSwitchOpt();
                }
            }
            else
            {
                Assert(ex.Reason() == RejitReason::TrackIntOverflowDisabled);
                profileInfo.DisableTrackCompoundedIntOverflow();
                if(body->HasDynamicProfileInfo())
                {
                    body->GetAnyDynamicProfileInfo()->DisableTrackCompoundedIntOverflow();
                }
            }

            if(PHASE_TRACE(Js::ReJITPhase, body))
            {
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(
                    L"Rejit (compile-time): function: %s (%s) reason: %S\n",
                    body->GetDisplayName(),
                    body->GetDebugNumberSet(debugStringBuffer),
                    ex.ReasonName());
            }

            rejit = true;
            funcAlloc.Reset();
            if(!foreground)
            {
                profileInfo.OnBackgroundAllocatorReset();
            }
        }

        // Either the entry point has a reference to the number now, or we failed to code gen and we
        // don't need to numbers, we can flush the completed page now.
        //
        // If the number allocator is NULL then we are shutting down the thread context and so too the
        // code generator. The number allocator must be freed before the recycler (and thus before the
        // code generator) so we can't and don't need to flush it.
        CodeGenNumberThreadAllocator * threadNumberAllocator = this->scriptContext->GetThreadContext()->GetCodeGenNumberThreadAllocator();
        if (threadNumberAllocator != nullptr)
        {
            threadNumberAllocator->FlushAllocations();
        }
    } while(rejit);

    threadContext->JITTelemetry.LogTime(threadContext->JITTelemetry.Now() - startTime);

#ifdef BGJIT_STATS
    // Must be interlocked because the following data may be modified from the background and foreground threads concurrently
    Js::ScriptContext *scriptContext = workItem->GetScriptContext();
    if (workItem->Type() == JsFunctionType)
    {
        InterlockedExchangeAdd(&scriptContext->bytecodeJITCount, workItem->GetByteCodeCount());
        InterlockedIncrement(&scriptContext->funcJITCount);
    }
    else if(workItem->Type() == JsLoopBodyWorkItemType)
    {
        InterlockedIncrement(&scriptContext->loopJITCount);
    }
#endif
}

void NativeCodeGenerator::SetProfileMode(BOOL fSet)
{
    this->SetNativeEntryPoint = fSet? Js::FunctionBody::ProfileSetNativeEntryPoint : Js::FunctionBody::DefaultSetNativeEntryPoint;
}

#if _M_IX86
__declspec(naked)
Js::Var
NativeCodeGenerator::CheckAsmJsCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...)
{
    __asm
    {
            push ebp
            mov ebp, esp
            push function
            call NativeCodeGenerator::CheckAsmJsCodeGen
#ifdef _CONTROL_FLOW_GUARD
            // verify that the call target is valid
            push eax
            mov  ecx, eax
            call[__guard_check_icall_fptr]
            pop eax
#endif
            pop ebp
            jmp  eax
    }
}
#elif _M_X64 || _M_ARM || _M_ARM64
    // Do nothing: the implementation of NativeCodeGenerator::CheckCodeGenThunk is declared (appropriately decorated) in
    // Backend\amd64\Thunks.asm and Backend\arm\Thunks.asm and Backend\arm64\Thunks.asm respectively.
#else
    #error Not implemented.
#endif

#if _M_IX86
__declspec(naked)
Js::Var
NativeCodeGenerator::CheckCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push [esp+8]
        call NativeCodeGenerator::CheckCodeGen
#ifdef _CONTROL_FLOW_GUARD
        // verify that the call target is valid
        push eax
        mov  ecx, eax
        call[__guard_check_icall_fptr]
        pop eax
#endif
        pop ebp
        jmp  eax
    }
}
#elif _M_X64 || _M_ARM || _M_ARM64
    // Do nothing: the implementation of NativeCodeGenerator::CheckCodeGenThunk is declared (appropriately decorated) in
    // Backend\amd64\Thunks.asm and Backend\arm\Thunks.asm and Backend\arm64\Thunks.asm respectively.
#else
    #error Not implemented.
#endif

bool
NativeCodeGenerator::IsThunk(Js::JavascriptMethod codeAddress)
{
    return codeAddress == NativeCodeGenerator::CheckCodeGenThunk;
}

bool
NativeCodeGenerator::IsAsmJsCodeGenThunk(Js::JavascriptMethod codeAddress)
{
#ifdef ASMJS_PLAT
    return codeAddress == NativeCodeGenerator::CheckAsmJsCodeGenThunk;
#else
    return false;
#endif

}

CheckCodeGenFunction
NativeCodeGenerator::GetCheckCodeGenFunction(Js::JavascriptMethod codeAddress)
{
    if (codeAddress == NativeCodeGenerator::CheckCodeGenThunk)
    {
        return NativeCodeGenerator::CheckCodeGen;
    }
    return nullptr;
}


Js::Var
NativeCodeGenerator::CheckAsmJsCodeGen(Js::ScriptFunction * function)
{
    Assert(function);
    Js::FunctionBody *functionBody = function->GetFunctionBody();
    Js::ScriptContext *scriptContext = functionBody->GetScriptContext();

    NativeCodeGenerator *nativeCodeGen = scriptContext->GetNativeCodeGenerator();
    Assert(scriptContext->GetThreadContext()->IsScriptActive());
    Assert(scriptContext->GetThreadContext()->IsInScript());

    // Load the entry point here to validate it got changed afterwards

    Js::FunctionEntryPointInfo* entryPoint = function->GetFunctionEntryPointInfo();

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if ((PHASE_ON1(Js::AsmJsJITTemplatePhase) && CONFIG_FLAG(MaxTemplatizedJitRunCount) >= 0) || (!PHASE_ON1(Js::AsmJsJITTemplatePhase) && CONFIG_FLAG(MaxAsmJsInterpreterRunCount) >= 0))
    {
        nativeCodeGen->Processor()->PrioritizeJobAndWait(nativeCodeGen, entryPoint, function);
    } else
#endif
    if (!nativeCodeGen->Processor()->PrioritizeJob(nativeCodeGen, entryPoint, function))
    {
        if (PHASE_TRACE1(Js::AsmjsEntryPointInfoPhase))
        {
            Output::Print(L"Codegen not done yet for function: %s, Entrypoint is CheckAsmJsCodeGenThunk\n", function->GetFunctionBody()->GetDisplayName());
        }
        return reinterpret_cast<Js::JavascriptMethod>(entryPoint->GetNativeAddress());
    }
    if (PHASE_TRACE1(Js::AsmjsEntryPointInfoPhase))
    {
        Output::Print(L"CodeGen Done for function: %s, Changing Entrypoint to Full JIT\n", function->GetFunctionBody()->GetDisplayName());
    }
    // we will need to set the functionbody external and asmjs entrypoint to the fulljit entrypoint
    return CheckCodeGenDone(functionBody, entryPoint, function);
}
Js::JavascriptMethod
NativeCodeGenerator::CheckCodeGen(Js::ScriptFunction * function)
{
    Assert(function);
    Assert(function->GetEntryPoint() == NativeCodeGenerator::CheckCodeGenThunk
        || Js::CrossSite::IsThunk(function->GetEntryPoint()));
    // We are not expecting non-deserialized functions here; Error if it hasn't been deserialized by this point
    Js::FunctionBody *functionBody = function->GetFunctionBody();
    Js::ScriptContext *scriptContext = functionBody->GetScriptContext();

    NativeCodeGenerator *nativeCodeGen = scriptContext->GetNativeCodeGenerator();
    Assert(scriptContext->GetThreadContext()->IsScriptActive());
    Assert(scriptContext->GetThreadContext()->IsInScript());

    // Load the entry point here to validate it got changed afterwards
    Js::JavascriptMethod originalEntryPoint = functionBody->GetOriginalEntryPoint();
    Js::FunctionEntryPointInfo* entryPoint = function->GetFunctionEntryPointInfo();

    Js::FunctionEntryPointInfo *const defaultEntryPointInfo = functionBody->GetDefaultFunctionEntryPointInfo();
    if(entryPoint != defaultEntryPointInfo)
    {
        // Switch to the latest entry point info
        function->UpdateThunkEntryPoint(defaultEntryPointInfo, functionBody->GetDirectEntryPoint(defaultEntryPointInfo));

        const Js::JavascriptMethod defaultDirectEntryPoint = functionBody->GetDirectEntryPoint(defaultEntryPointInfo);
        if(!IsThunk(defaultDirectEntryPoint))
        {
            return defaultDirectEntryPoint;
        }
        entryPoint = defaultEntryPointInfo;
    }

    // If a transition to JIT needs to be forced, JIT right away
    if(Js::Configuration::Global.flags.EnforceExecutionModeLimits &&
        functionBody->GetExecutionMode() != ExecutionMode::SimpleJit &&
        functionBody->TryTransitionToJitExecutionMode())
    {
        nativeCodeGen->Processor()->PrioritizeJobAndWait(nativeCodeGen, entryPoint, function);
        return CheckCodeGenDone(functionBody, entryPoint, function);
    }

    if(!nativeCodeGen->Processor()->PrioritizeJob(nativeCodeGen, entryPoint, function))
    {
        // Job was not yet processed
        // originalEntryPoint is the last known good entry point for the function body. Here we verify that
        // it either corresponds with this codegen episode (identified by function->entryPointIndex) of the function body
        // or one that was scheduled after. In the latter case orginalEntryPoint will get updated if and when
        // that last episode completes successfully.
        Assert(functionBody->GetDefaultEntryPointInfo() == function->GetEntryPointInfo() &&
            (
                originalEntryPoint == DefaultEntryThunk
             || scriptContext->IsDynamicInterpreterThunk(originalEntryPoint)
             || originalEntryPoint == ProfileDeferredParsingThunk
             || originalEntryPoint == DefaultDeferredParsingThunk
             || (
                    functionBody->GetSimpleJitEntryPointInfo() &&
                    originalEntryPoint ==
                        reinterpret_cast<Js::JavascriptMethod>(functionBody->GetSimpleJitEntryPointInfo()->GetNativeAddress())
                )
            ) ||
            functionBody->GetDefaultFunctionEntryPointInfo()->entryPointIndex > function->GetFunctionEntryPointInfo()->entryPointIndex);
        return (scriptContext->CurrentThunk == ProfileEntryThunk) ? ProfileEntryThunk : originalEntryPoint;
    }

    return CheckCodeGenDone(functionBody, entryPoint, function);
}

Js::JavascriptMethod
NativeCodeGenerator::CheckCodeGenDone(
    Js::FunctionBody *const functionBody,
    Js::FunctionEntryPointInfo *const entryPointInfo,
    Js::ScriptFunction * function)
{
    Assert(!function || function->GetFunctionBody() == functionBody);
    Assert(!function || function->GetFunctionEntryPointInfo() == entryPointInfo);
    // Job was processed or failed and cleaned up
    // We won't call CheckCodeGenDone if the job is still pending since
    // PrioritizeJob will return false
    Assert(entryPointInfo->IsCodeGenDone() || entryPointInfo->IsCleanedUp() || entryPointInfo->IsPendingCleanup());

    if (!functionBody->GetHasBailoutInstrInJittedCode() && functionBody->GetHasAllocatedLoopHeaders() && (!functionBody->GetIsAsmJsFunction() || !(((Js::FunctionEntryPointInfo*)functionBody->GetDefaultEntryPointInfo())->GetIsTJMode())))
    {
        if (functionBody->GetCanReleaseLoopHeaders())
        {
            functionBody->ReleaseLoopHeaders();
        }
        else
        {
            functionBody->SetPendingLoopHeaderRelease(true);
        }
    }

    Js::ScriptContext *scriptContext = functionBody->GetScriptContext();
    if (!functionBody->GetNativeEntryPointUsed())
    {
#ifdef BGJIT_STATS
        scriptContext->jitCodeUsed += functionBody->GetByteCodeCount();
        scriptContext->funcJitCodeUsed++;
#endif

        functionBody->SetNativeEntryPointUsed(true);
    }

    // Replace the entry point
    Js::JavascriptMethod address;
    if (!entryPointInfo->IsCodeGenDone())
    {
        if (entryPointInfo->IsPendingCleanup())
        {
            entryPointInfo->Cleanup(false /* isShutdown */, true /* capture cleanup stack */);
        }
        address = functionBody->GetScriptContext()->CurrentThunk == ProfileEntryThunk ? ProfileEntryThunk : functionBody->GetOriginalEntryPoint();
        entryPointInfo->address = address;
    }
    else
    {
        scriptContext->GetNativeCodeGenerator()->SetNativeEntryPoint(
            entryPointInfo,
            functionBody,
            reinterpret_cast<Js::JavascriptMethod>(entryPointInfo->GetNativeAddress()));
        address = (Js::JavascriptMethod) entryPointInfo->address;

        Assert(!functionBody->NeedEnsureDynamicProfileInfo() || address == Js::DynamicProfileInfo::EnsureDynamicProfileInfoThunk);
    }

    Assert(!IsThunk(address));

    if(function)
    {
        function->UpdateThunkEntryPoint(entryPointInfo, address);
    }

    // call the direct entry point, which will ensure dynamic profile info if necessary
    return address;
}

CodeGenWorkItem *
NativeCodeGenerator::GetJob(Js::EntryPointInfo * const entryPoint) const
{
    ASSERT_THREAD();
    Assert(entryPoint);

    return entryPoint->GetWorkItem();
}

bool
NativeCodeGenerator::WasAddedToJobProcessor(JsUtil::Job *const job) const
{
    // This function is called from inside the lock

    ASSERT_THREAD();
    Assert(job);

    return static_cast<CodeGenWorkItem *>(job)->IsInJitQueue();
}

bool
NativeCodeGenerator::ShouldProcessInForeground(const bool willWaitForJob, const unsigned int numJobsInQueue) const
{
    // This function is called from inside the lock

    ASSERT_THREAD();

    // Process the job synchronously in the foreground thread if we're waiting for the job to be processed, or if the background
    // job queue is long enough and this native code generator is optimized for many instances (web workers)
    return
        willWaitForJob ||
        (numJobsInQueue > (uint)CONFIG_FLAG(HybridFgJitBgQueueLengthThreshold) &&
            (CONFIG_FLAG(HybridFgJit) || isOptimizedForManyInstances));
}

void
NativeCodeGenerator::PrioritizedButNotYetProcessed(JsUtil::Job *const job)
{
    // This function is called from inside the lock

    ASSERT_THREAD();
    Assert(job);

#ifdef BGJIT_STATS
    CodeGenWorkItem *const codeGenWorkItem = static_cast<CodeGenWorkItem *>(job);
    if(codeGenWorkItem->Type() == JsFunctionType && codeGenWorkItem->IsInJitQueue())
    {
        codeGenWorkItem->GetScriptContext()->interpretedCallsHighPri++;

        if(codeGenWorkItem->GetJitMode() == ExecutionMode::FullJit)
        {
            QueuedFullJitWorkItem *const queuedFullJitWorkItem = codeGenWorkItem->GetQueuedFullJitWorkItem();
            if(queuedFullJitWorkItem)
            {
                queuedFullJitWorkItems.MoveToBeginning(queuedFullJitWorkItem);
            }
        }
    }
#endif
}


void
NativeCodeGenerator::BeforeWaitForJob(Js::EntryPointInfo *const entryPoint) const
{
    ASSERT_THREAD();
    Assert(entryPoint);

#ifdef PROFILE_EXEC
    ProfileBegin(this->foregroundCodeGenProfiler, Js::DelayPhase);
#endif
}

void
NativeCodeGenerator::AfterWaitForJob(Js::EntryPointInfo *const entryPoint) const
{
    ASSERT_THREAD();
    Assert(entryPoint);

#ifdef PROFILE_EXEC
    ProfileEnd(this->foregroundCodeGenProfiler, Js::DelayPhase);
#endif
}

/*
* A workitem exceeds JIT limits if we've already generated MaxThreadJITCodeHeapSize
* (currently 7 MB) of code on this thread or MaxProcessJITCodeHeapSize (currently 55 MB)
* in the process. In real world websites we rarely (if at all) hit this limit.
* Also, if this workitem's byte code size is in excess of MaxJITFunctionBytecodeSize instructions,
* it exceeds the JIT limits
*/
bool
NativeCodeGenerator::WorkItemExceedsJITLimits(CodeGenWorkItem *const codeGenWork)
{
    return
        (codeGenWork->GetScriptContext()->GetThreadContext()->GetCodeSize() >= Js::Constants::MaxThreadJITCodeHeapSize) ||
        (ThreadContext::GetProcessCodeSize() >= Js::Constants::MaxProcessJITCodeHeapSize) ||
        (codeGenWork->GetByteCodeCount() >= (uint)CONFIG_FLAG(MaxJITFunctionBytecodeSize));
}
bool
NativeCodeGenerator::Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData)
{
    const bool foreground = !threadData;
    PageAllocator *pageAllocator;
    if (foreground)
    {
        pageAllocator = scriptContext->GetThreadContext()->GetPageAllocator();
    }
    else
    {
        pageAllocator = threadData->GetPageAllocator();
    }

    CodeGenWorkItem *const codeGenWork = static_cast<CodeGenWorkItem *>(job);

    switch (codeGenWork->Type())
    {
    case JsLoopBodyWorkItemType:
    {
        JsLoopBodyCodeGen* loopBodyCodeGenWorkItem = (JsLoopBodyCodeGen*)codeGenWork;
        Js::FunctionBody* fn = loopBodyCodeGenWorkItem->GetFunctionBody();

        if (fn->GetNativeEntryPointUsed() && fn->GetCanReleaseLoopHeaders() && (!fn->GetIsAsmJsFunction() || !(loopBodyCodeGenWorkItem->loopHeader->GetCurrentEntryPointInfo()->GetIsTJMode())))
        {
            loopBodyCodeGenWorkItem->loopHeader->ResetInterpreterCount();
            return false;
        }
        // Unless we're in a ForceNative configuration, ignore this workitem if it exceeds JIT limits
        if (fn->ForceJITLoopBody() || !WorkItemExceedsJITLimits(codeGenWork))
        {
            CodeGen(pageAllocator, codeGenWork, foreground);
            return true;
        }
        Js::EntryPointInfo * entryPoint = loopBodyCodeGenWorkItem->GetEntryPoint();
        entryPoint->SetJITCapReached();
        return false;
    }
    case JsFunctionType:
    {
        // Unless we're in a ForceNative configuration, ignore this workitem if it exceeds JIT limits
        if (IS_PREJIT_ON() || Js::Configuration::Global.flags.ForceNative || !WorkItemExceedsJITLimits(codeGenWork))
        {
            CodeGen(pageAllocator, codeGenWork, foreground);
            return true;
        }
        return false;
    }

    default:
        Assume(UNREACHED);
    }
    return false;
}

void
NativeCodeGenerator::Prioritize(JsUtil::Job *const job, const bool forceAddJobToProcessor, void* function)
{
    // This function is called from inside the lock

    ASSERT_THREAD();
    Assert(job);
    Assert(static_cast<const CodeGenWorkItem *>(job)->Type() == CodeGenWorkItemType::JsFunctionType);
    Assert(!WasAddedToJobProcessor(job));

    JsFunctionCodeGen *const workItem = static_cast<JsFunctionCodeGen *>(job);
    Js::FunctionBody *const functionBody = workItem->GetFunctionBody();
    Assert(workItem->GetEntryPoint() == functionBody->GetDefaultFunctionEntryPointInfo());

    ExecutionMode jitMode;
    if (functionBody->GetIsAsmjsMode())
    {
        jitMode = ExecutionMode::FullJit;
        functionBody->SetExecutionMode(ExecutionMode::FullJit);
    }
    else
    {
        if(!forceAddJobToProcessor && !functionBody->TryTransitionToJitExecutionMode())
        {
            return;
        }

        jitMode = functionBody->GetExecutionMode();
        Assert(jitMode == ExecutionMode::SimpleJit || jitMode == ExecutionMode::FullJit);
    }

    workItems.Unlink(workItem);
    workItem->SetJitMode(jitMode);
    try
    {
        // Prioritize full JIT work items over simple JIT work items. This simple solution seems sufficient for now, but it
        // might be better to use a priority queue if it becomes necessary to prioritize recent simple JIT work items relative
        // to the older simple JIT work items.
        AddToJitQueue(
            workItem,
            jitMode == ExecutionMode::FullJit || queuedFullJitWorkItemCount == 0 /* prioritize */,
            false /* lock */,
            function);
    }
    catch (...)
    {
        // Add the item back to the list if AddToJitQueue throws. The position in the list is not important.
        workItem->ResetJitMode();
        workItems.LinkToEnd(workItem);
        throw;
    }
}

bool
NativeCodeGenerator::IsInDebugMode() const
{
    return this->scriptContext->IsInDebugMode();
}

ExecutionMode NativeCodeGenerator::PrejitJitMode(Js::FunctionBody *const functionBody)
{
    Assert(IS_PREJIT_ON());
    Assert(functionBody->DoSimpleJit() || functionBody->DoFullJit());

    // Prefer full JIT for prejitting unless it's off or simple JIT is forced
    return
        functionBody->DoFullJit() && !(PHASE_FORCE(Js::Phase::SimpleJitPhase, functionBody) && functionBody->DoSimpleJit())
            ? ExecutionMode::FullJit
            : ExecutionMode::SimpleJit;
}

void
NativeCodeGenerator::UpdateQueueForDebugMode()
{
    Assert(!this->hasUpdatedQForDebugMode);

    // If we're going to debug mode, drain the job processors queue of
    // all jobs belonging this native code generator
    // JobProcesed will be called for existing jobs, and in debug mode
    // that method will simply add them back to the NativeCodeGen's queue
    Processor()->RemoveManager(this);

    this->hasUpdatedQForDebugMode = true;

    if (Js::Configuration::Global.EnableJitInDebugMode())
    {
        Processor()->AddManager(this);
    }
}

void
NativeCodeGenerator::JobProcessed(JsUtil::Job *const job, const bool succeeded)
{
    // This function is called from inside the lock

    Assert(job);

    CodeGenWorkItem *workItem = static_cast<CodeGenWorkItem *>(job);

    class AutoCleanup
    {
    private:
        Js::ScriptContext *const scriptContext;
        Js::CodeGenRecyclableData *const recyclableData;

    public:
        AutoCleanup(Js::ScriptContext *const scriptContext, Js::CodeGenRecyclableData *const recyclableData)
            : scriptContext(scriptContext), recyclableData(recyclableData)
        {
            Assert(scriptContext);
        }

        ~AutoCleanup()
        {
            if(recyclableData)
            {
                scriptContext->GetThreadContext()->UnregisterCodeGenRecyclableData(recyclableData);
            }
        }
    } autoCleanup(scriptContext, workItem->RecyclableData());

    const ExecutionMode jitMode = workItem->GetJitMode();
    if(jitMode == ExecutionMode::FullJit && workItem->IsInJitQueue())
    {
        QueuedFullJitWorkItem *const queuedFullJitWorkItem = workItem->GetQueuedFullJitWorkItem();
        if(queuedFullJitWorkItem)
        {
            queuedFullJitWorkItems.Unlink(queuedFullJitWorkItem);
            --queuedFullJitWorkItemCount;
        }
    }

    Js::FunctionBody* functionBody = nullptr;
    CodeGenWorkItemType workitemType = workItem->Type();

    if (workitemType == JsFunctionType)
    {
        JsFunctionCodeGen * functionCodeGen = (JsFunctionCodeGen *)workItem;
        functionBody = functionCodeGen->GetFunctionBody();

        if (succeeded)
        {
            Js::FunctionEntryPointInfo* entryPointInfo = static_cast<Js::FunctionEntryPointInfo*>(functionCodeGen->GetEntryPoint());
            entryPointInfo->SetJitMode(jitMode);
            Assert(workItem->GetCodeAddress() != NULL);
            entryPointInfo->SetCodeGenDone();
        }
        else
        {
#if DBG
            functionBody->m_nativeEntryPointIsInterpreterThunk = true;
#endif
            // It's okay if the entry point has been reclaimed at this point
            // since the job failed anyway so the entry point should never get used
            // If it's still around, clean it up. If not, its finalizer would clean
            // it up anyway.
            Js::EntryPointInfo* entryPointInfo = functionCodeGen->GetEntryPoint();

            if (entryPointInfo)
            {
#if ENABLE_DEBUG_CONFIG_OPTIONS
                switch (job->failureReason)
                {
                case Job::FailureReason::OOM: entryPointInfo->SetCleanupReason(Js::EntryPointInfo::CodeGenFailedOOM); break;
                case Job::FailureReason::StackOverflow: entryPointInfo->SetCleanupReason(Js::EntryPointInfo::CodeGenFailedStackOverflow); break;
                case Job::FailureReason::Aborted: entryPointInfo->SetCleanupReason(Js::EntryPointInfo::CodeGenFailedAborted); break;
                default: Assert(job->failureReason == Job::FailureReason::NotFailed);
                }
#endif

                entryPointInfo->SetPendingCleanup();
            }

            functionCodeGen->OnWorkItemProcessFail(this);
        }

        InterlockedDecrement(&pendingCodeGenWorkItems);
        HeapDelete(functionCodeGen);
    }
    else if (workitemType == JsLoopBodyWorkItemType)
    {
        JsLoopBodyCodeGen * loopBodyCodeGen = (JsLoopBodyCodeGen*)workItem;
        functionBody = loopBodyCodeGen->GetFunctionBody();
        Js::EntryPointInfo * entryPoint = loopBodyCodeGen->GetEntryPoint();

        if (succeeded)
        {
            Assert(workItem->GetCodeAddress() != NULL);
            functionBody->SetLoopBodyEntryPoint(loopBodyCodeGen->loopHeader, loopBodyCodeGen->GetEntryPoint(), (Js::JavascriptMethod)workItem->GetCodeAddress());
            entryPoint->SetCodeGenDone();
        }
        else
        {
            // We re-use failed loop body entry points.
            // The loop body entry point could have been cleaned up if the parent function JITed,
            // in which case we don't want to reset it.
            if (entryPoint && !entryPoint->IsCleanedUp())
            {
                entryPoint->Reset(!entryPoint->IsJITCapReached()); // reset state to NotScheduled if JIT cap hasn't been reached
            }
            loopBodyCodeGen->OnWorkItemProcessFail(this);
        }
        HeapDelete(loopBodyCodeGen);
    }
    else
    {
        AssertMsg(false, "Unknown work item type");
        AssertMsg(workItem->GetCodeAddress() == NULL, "No other types should have native entry point for now.");
    }
}

JsUtil::Job *
NativeCodeGenerator::GetJobToProcessProactively()
{
    ASSERT_THREAD();

    // Look for work, starting with high priority items first, and above LowPri
    CodeGenWorkItem* workItem = workItems.Head();
    while(workItem != nullptr)
    {
        if(workItem->ShouldSpeculativelyJit(this->byteCodeSizeGenerated))
        {
            workItem->SetJitMode(ExecutionMode::FullJit);

            // Note: This gives a perf regression in fre build, but it is useful for debugging and won't be there for the final build
            //   anyway, so I left it in.
            if (PHASE_TRACE(Js::DelayPhase, workItem->GetFunctionBody())) {
                OUTPUT_TRACE(Js::DelayPhase, L"ScriptContext: 0x%p, Speculative JIT: %-25s, Byte code generated: %d \n",
                    this->scriptContext, workItem->GetFunctionBody()->GetExternalDisplayName(), this->byteCodeSizeGenerated);
            }
            Js::FunctionBody *fn = workItem->GetFunctionBody();
            Js::EntryPointInfo *entryPoint = workItem->GetEntryPoint();
            const auto recyclableData = GatherCodeGenData(fn, fn, entryPoint, workItem);

            workItems.Unlink(workItem);
            workItem->SetRecyclableData(recyclableData);

            {
                AutoOptionalCriticalSection lock(Processor()->GetCriticalSection());
                scriptContext->GetThreadContext()->RegisterCodeGenRecyclableData(recyclableData);
            }
#ifdef BGJIT_STATS
            scriptContext->speculativeJitCount++;
#endif

            QueuedFullJitWorkItem *const queuedFullJitWorkItem = workItem->EnsureQueuedFullJitWorkItem();
            if(queuedFullJitWorkItem) // ignore OOM, this work item just won't be removed from the job processor's queue
            {
                queuedFullJitWorkItems.LinkToBeginning(queuedFullJitWorkItem);
                ++queuedFullJitWorkItemCount;
            }
            workItem->OnAddToJitQueue();

            workItem->GetFunctionBody()->TraceExecutionMode("SpeculativeJit (before)");
            workItem->GetFunctionBody()->TransitionToFullJitExecutionMode();
            workItem->GetFunctionBody()->TraceExecutionMode("SpeculativeJit");
            break;
        }
        workItem = static_cast<CodeGenWorkItem*>(workItem->Next());
    }
    return workItem;
}

// Removes all of the proactive jobs from the generator.  Used when switching between attached/detached
// debug modes in order to drain the queue of jobs (since we switch from interpreted to native and back).
void
NativeCodeGenerator::RemoveProactiveJobs()
{
    CodeGenWorkItem* workItem = workItems.Head();
    while (workItem)
    {
        CodeGenWorkItem* temp = static_cast<CodeGenWorkItem*>(workItem->Next());
        workItem->Delete();
        workItem = temp;
    }
    workItems.Clear();

    //for(JsUtil::Job *job = workItems.Head(); job;)
    //{
    //    JsUtil::Job *const next = job->Next();
    //    JobProcessed(job, /*succeeded*/ false);
    //    job = next;
    //}
}

template<bool IsInlinee>
void
NativeCodeGenerator::GatherCodeGenData(
    Recycler *const recycler,
    Js::FunctionBody *const topFunctionBody,
    Js::FunctionBody *const functionBody,
    Js::EntryPointInfo *const entryPoint,
    InliningDecider &inliningDecider,
    ObjTypeSpecFldInfoList *objTypeSpecFldInfoList,
    Js::FunctionCodeGenJitTimeData *const jitTimeData,
    Js::FunctionCodeGenRuntimeData *const runtimeData,
    Js::JavascriptFunction* function,
    bool isJitTimeDataComputed,
    uint32 recursiveInlineDepth)
{
    ASSERT_THREAD();
    Assert(recycler);
    Assert(functionBody);
    Assert(jitTimeData);
    Assert(IsInlinee == !!runtimeData);
    Assert(topFunctionBody != nullptr && (!entryPoint->GetWorkItem() || entryPoint->GetWorkItem()->GetFunctionBody() == topFunctionBody));
    Assert(objTypeSpecFldInfoList != nullptr);

#ifdef FIELD_ACCESS_STATS
    jitTimeData->EnsureInlineCacheStats(recycler);
#define SetInlineCacheCount(counter, value)  jitTimeData->inlineCacheStats->counter = value;
#define IncInlineCacheCount(counter) if(!isJitTimeDataComputed) {jitTimeData->inlineCacheStats->counter++;}
#define AddInlineCacheStats(callerData, inlineeData) callerData->AddInlineeInlineCacheStats(inlineeData);
#define InlineCacheStatsArg(jitTimeData) !isJitTimeDataComputed ? jitTimeData->inlineCacheStats : nullptr
#else
#define SetInlineCacheCount(counter, value)
#define IncInlineCacheCount(counter)
#define AddInlineCacheStats(callerData, inlineeData)
#define InlineCacheStatsArg(jitTimeData) nullptr
#endif

#if DBG
    Assert(
        PHASE_ON(Js::Phase::SimulatePolyCacheWithOneTypeForFunctionPhase, functionBody) ==
        CONFIG_ISENABLED(Js::Flag::SimulatePolyCacheWithOneTypeForInlineCacheIndexFlag));
    if(PHASE_ON(Js::Phase::SimulatePolyCacheWithOneTypeForFunctionPhase, functionBody))
    {
        const Js::InlineCacheIndex inlineCacheIndex = CONFIG_FLAG(SimulatePolyCacheWithOneTypeForInlineCacheIndex);
        functionBody->CreateNewPolymorphicInlineCache(
            inlineCacheIndex,
            functionBody->GetPropertyIdFromCacheId(inlineCacheIndex),
            functionBody->GetInlineCache(inlineCacheIndex));
        if(functionBody->HasDynamicProfileInfo())
        {
            functionBody->GetAnyDynamicProfileInfo()->RecordPolymorphicFieldAccess(functionBody, inlineCacheIndex);
        }
    }
#endif

    if(IsInlinee)
    {
        // This function is recursive
        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);
    }
    else
    {
        //TryAggressiveInlining adjusts inlining heuristics and walks the call tree. If it can inlining everything it will set the InliningThreshold to be aggressive.
        if (!inliningDecider.GetIsLoopBody())
        {
            uint32 inlineeCount = 0;
            if (!PHASE_OFF(Js::TryAggressiveInliningPhase, topFunctionBody))
            {
                Assert(topFunctionBody == functionBody);
                inliningDecider.SetAggressiveHeuristics();
                if (!TryAggressiveInlining(topFunctionBody, functionBody, inliningDecider, inlineeCount, 0))
                {
                    uint countOfInlineesWithLoops = inliningDecider.getNumberOfInlineesWithLoop();
                    //TryAggressiveInlining failed, set back to default heuristics.
                    inliningDecider.ResetInlineHeuristics();
                    inliningDecider.SetLimitOnInlineesWithLoop(countOfInlineesWithLoops);
                }
                else
                {
                    jitTimeData->SetIsAggressiveInliningEnabled();
                }
                inliningDecider.ResetState();
            }
        }
        entryPoint->EnsurePolymorphicInlineCacheInfo(recycler, functionBody);
    }

    entryPoint->EnsureJitTransferData(recycler);
#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_VERBOSE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
    {
        Output::Print(L"ObjTypeSpec: top function %s (%s), function %s (%s): GatherCodeGenData(): \n",
            topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer));
        Output::Flush();
    }
#endif
    const auto profileData =
        functionBody->HasDynamicProfileInfo()
            ? functionBody->GetAnyDynamicProfileInfo()
            : functionBody->EnsureDynamicProfileInfo();

    bool inlineGetterSetter = false;
    bool inlineApplyTarget = false; //to indicate whether we can inline apply target or not.
    bool inlineCallTarget = false;
    if(profileData)
    {
        if (!IsInlinee)
        {
            PHASE_PRINT_TRACE(
                Js::ObjTypeSpecPhase, functionBody,
                L"Objtypespec (%s): Pending cache state on add %x to JIT queue: %d\n",
                functionBody->GetDebugNumberSet(debugStringBuffer), entryPoint, profileData->GetPolymorphicCacheState());

            entryPoint->SetPendingPolymorphicCacheState(profileData->GetPolymorphicCacheState());
            entryPoint->SetPendingInlinerVersion(profileData->GetInlinerVersion());
            entryPoint->SetPendingImplicitCallFlags(profileData->GetImplicitCallFlags());
        }

        if (functionBody->GetProfiledArrayCallSiteCount() != 0)
        {
            RecyclerWeakReference<Js::FunctionBody> *weakFuncRef = recycler->CreateWeakReferenceHandle(functionBody);
            if (!isJitTimeDataComputed)
            {
                jitTimeData->SetWeakFuncRef(weakFuncRef);
            }
            entryPoint->AddWeakFuncRef(weakFuncRef, recycler);
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (PHASE_VERBOSE_TESTTRACE(Js::ObjTypeSpecPhase, functionBody) ||
            PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
        {
            if (functionBody->GetInlineCacheCount() > 0)
            {
                if (!IsInlinee)
                {
                    Output::Print(L"-----------------------------------------------------------------------------\n");
                }
                else
                {
                    Output::Print(L"\tInlinee:\t");
                }
                functionBody->DumpFullFunctionName();
                Output::Print(L"\n");
            }
        }
#endif

        SetInlineCacheCount(totalInlineCacheCount, functionBody->GetInlineCacheCount());

        Assert(functionBody->GetProfiledFldCount() == functionBody->GetInlineCacheCount()); // otherwise, isInst inline caches need to be cloned
        for(uint i = 0; i < functionBody->GetInlineCacheCount(); ++i)
        {
            const auto cacheType = profileData->GetFldInfo(functionBody, i)->flags;

            PHASE_PRINT_VERBOSE_TESTTRACE(
                Js::ObjTypeSpecPhase, functionBody,
                L"Cache #%3d, Layout: %s, Profile info: %s\n",
                i,
                functionBody->GetInlineCache(i)->LayoutString(),
                cacheType == Js::FldInfo_NoInfo ? L"none" :
                (cacheType & Js::FldInfo_Polymorphic) ? L"polymorphic" : L"monomorphic");

            if (cacheType == Js::FldInfo_NoInfo)
            {
                IncInlineCacheCount(noInfoInlineCacheCount);
                continue;
            }

            Js::PolymorphicInlineCache * polymorphicCacheOnFunctionBody = functionBody->GetPolymorphicInlineCache(i);
            bool isPolymorphic = (cacheType & Js::FldInfo_Polymorphic) != 0;
            if (!isPolymorphic)
            {
                Js::InlineCache *inlineCache;
                if(function && Js::ScriptFunctionWithInlineCache::Is(function))
                {
                    inlineCache = Js::ScriptFunctionWithInlineCache::FromVar(function)->GetInlineCache(i);
                }
                else
                {
                    inlineCache = functionBody->GetInlineCache(i);
                }

                Js::ObjTypeSpecFldInfo* objTypeSpecFldInfo = nullptr;

#if ENABLE_DEBUG_CONFIG_OPTIONS
                if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_VERBOSE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
                {
                    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(i);
                    Js::PropertyRecord const * const propertyRecord = functionBody->GetScriptContext()->GetPropertyName(propertyId);
                    Output::Print(L"ObTypeSpec: top function %s (%s), function %s (%s): cloning mono cache for %s (#%d) cache %d \n",
                        topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer),
                        functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer2), propertyRecord->GetBuffer(), propertyId, i);
                    Output::Flush();
                }
#endif

                IncInlineCacheCount(monoInlineCacheCount);

                if (inlineCache->IsEmpty())
                {
                    IncInlineCacheCount(emptyMonoInlineCacheCount);
                }

                if(!PHASE_OFF(Js::ObjTypeSpecPhase, functionBody) || !PHASE_OFF(Js::FixedMethodsPhase, functionBody) || !PHASE_OFF(Js::UseFixedDataPropsPhase, functionBody))
                {
                    if(cacheType & (Js::FldInfo_FromLocal | Js::FldInfo_FromLocalWithoutProperty | Js::FldInfo_FromProto))
                    {
                        // WinBlue 170722: Disable ObjTypeSpec optimization for activation object in debug mode,
                        // as it can result in BailOutFailedTypeCheck before locals are set to undefined,
                        // which can result in using garbage object during bailout/restore values.
                        if (!(IsInDebugMode() && inlineCache->GetType() &&
                              inlineCache->GetType()->GetTypeId() == Js::TypeIds_ActivationObject))
                        {
                            objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), inlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                            if (objTypeSpecFldInfo)
                            {
                                IncInlineCacheCount(clonedMonoInlineCacheCount);

                                if (!PHASE_OFF(Js::InlineApplyTargetPhase, functionBody) && (cacheType & Js::FldInfo_InlineCandidate))
                                {
                                    if (IsInlinee || objTypeSpecFldInfo->isBuiltIn)
                                    {
                                        inlineApplyTarget = true;
                                    }
                                }

                                if (!PHASE_OFF(Js::InlineCallTargetPhase, functionBody) && (cacheType & Js::FldInfo_InlineCandidate))
                                {
                                    inlineCallTarget = true;
                                }
                                if (!isJitTimeDataComputed)
                                {
                                    jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                    objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                                }
                            }
                        }
                    }
                }
                if(!PHASE_OFF(Js::FixAccessorPropsPhase, functionBody))
                {
                    if (!objTypeSpecFldInfo && (cacheType & Js::FldInfo_FromAccessor) && (cacheType & Js::FldInfo_InlineCandidate))
                    {
                        objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), inlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                        if (objTypeSpecFldInfo)
                        {
                            inlineGetterSetter = true;
                            if (!isJitTimeDataComputed)
                            {
                                IncInlineCacheCount(clonedMonoInlineCacheCount);
                                jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                            }
                        }

                    }
                }
                if (!PHASE_OFF(Js::RootObjectFldFastPathPhase, functionBody))
                {
                    if (i >= functionBody->GetRootObjectLoadInlineCacheStart() && inlineCache->IsLocal())
                    {
                        void * rawType = inlineCache->u.local.type;
                        Js::Type * type = TypeWithoutAuxSlotTag(rawType);
                        Js::RootObjectBase * rootObject = functionBody->GetRootObject();
                        if (rootObject->GetType() == type)
                        {
                            Js::BigPropertyIndex propertyIndex = inlineCache->u.local.slotIndex;
                            if (rawType == type)
                            {
                                // type is not tagged, inline slot
                                propertyIndex = rootObject->GetPropertyIndexFromInlineSlotIndex(inlineCache->u.local.slotIndex);
                            }
                            else
                            {
                                propertyIndex = rootObject->GetPropertyIndexFromAuxSlotIndex(inlineCache->u.local.slotIndex);
                            }
                            Js::PropertyAttributes attributes;
                            if (rootObject->GetAttributesWithPropertyIndex(functionBody->GetPropertyIdFromCacheId(i), propertyIndex, &attributes)
                                && (attributes & PropertyConfigurable) == 0
                                && !isJitTimeDataComputed)
                            {
                                // non configurable
                                if (objTypeSpecFldInfo == nullptr)
                                {
                                    objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), inlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                                    if (objTypeSpecFldInfo)
                                    {
                                        IncInlineCacheCount(clonedMonoInlineCacheCount);
                                        jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                        objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                                    }
                                }
                                if (objTypeSpecFldInfo != nullptr)
                                {
                                    objTypeSpecFldInfo->SetRootObjectNonConfigurableField(i < functionBody->GetRootObjectStoreInlineCacheStart());
                                }
                            }
                        }
                    }
                }
            }
            // Even if the FldInfo says that the field access may be polymorphic, be optimistic that if the function object has inline caches, they'll be monomorphic
            else if(function && Js::ScriptFunctionWithInlineCache::Is(function) && (cacheType & Js::FldInfo_InlineCandidate || !polymorphicCacheOnFunctionBody))
            {
                Js::InlineCache *inlineCache = Js::ScriptFunctionWithInlineCache::FromVar(function)->GetInlineCache(i);
                Js::ObjTypeSpecFldInfo* objTypeSpecFldInfo = nullptr;

                if(!PHASE_OFF(Js::ObjTypeSpecPhase, functionBody) || !PHASE_OFF(Js::FixedMethodsPhase, functionBody))
                {
                    if(cacheType & (Js::FldInfo_FromLocal | Js::FldInfo_FromProto))  // Remove FldInfo_FromLocal?
                    {

                        // WinBlue 170722: Disable ObjTypeSpec optimization for activation object in debug mode,
                        // as it can result in BailOutFailedTypeCheck before locals are set to undefined,
                        // which can result in using garbage object during bailout/restore values.
                        if (!(IsInDebugMode() && inlineCache->GetType() &&
                              inlineCache->GetType()->GetTypeId() == Js::TypeIds_ActivationObject))
                        {
                            objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), inlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                            if (objTypeSpecFldInfo)
                            {
                                IncInlineCacheCount(clonedMonoInlineCacheCount);

                                if (!PHASE_OFF(Js::InlineApplyTargetPhase, functionBody) && IsInlinee && (cacheType & Js::FldInfo_InlineCandidate))
                                {
                                    inlineApplyTarget = true;
                                }
                                if (!isJitTimeDataComputed)
                                {
                                    jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                    objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                const auto polymorphicInlineCache = functionBody->GetPolymorphicInlineCache(i);

                if (polymorphicInlineCache != nullptr)
                {
                    IncInlineCacheCount(polyInlineCacheCount);
                    if (profileData->GetFldInfo(functionBody, i)->ShouldUsePolymorphicInlineCache())
                    {
                        IncInlineCacheCount(highUtilPolyInlineCacheCount);
                    }
                    else
                    {
                        IncInlineCacheCount(lowUtilPolyInlineCacheCount);
                    }

                    if (!PHASE_OFF(Js::EquivObjTypeSpecPhase, topFunctionBody) && !topFunctionBody->GetAnyDynamicProfileInfo()->IsEquivalentObjTypeSpecDisabled())
                    {
                        if (!polymorphicInlineCache->GetIgnoreForEquivalentObjTypeSpec() || (polymorphicInlineCache->GetCloneForJitTimeUse() && !PHASE_OFF(Js::PolymorphicInlinePhase, functionBody) && !PHASE_OFF(Js::PolymorphicInlineFixedMethodsPhase, functionBody)))
                        {
#if ENABLE_DEBUG_CONFIG_OPTIONS
                            if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_VERBOSE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
                            {
                                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                                wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                                Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(i);
                                Js::PropertyRecord const * const propertyRecord = functionBody->GetScriptContext()->GetPropertyName(propertyId);
                                Output::Print(L"ObTypeSpec: top function %s (%s), function %s (%s): cloning poly cache for %s (#%d) cache %d \n",
                                    topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer),
                                    functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer2), propertyRecord->GetBuffer(), propertyId, i);
                                Output::Flush();
                            }
#endif
                            Js::ObjTypeSpecFldInfo* objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), polymorphicInlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                            if (objTypeSpecFldInfo != nullptr)
                            {
                                if (!isJitTimeDataComputed)
                                {
                                    jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                    IncInlineCacheCount(clonedPolyInlineCacheCount);
                                    objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                                }
                                if (!PHASE_OFF(Js::InlineAccessorsPhase, functionBody) && (cacheType & Js::FldInfo_FromAccessor) && (cacheType & Js::FldInfo_InlineCandidate))
                                {
                                    inlineGetterSetter = true;
                                }
                            }
                        }
                        else
                        {
                            IncInlineCacheCount(ignoredPolyInlineCacheCount);
                        }
                    }
                    else
                    {
                        IncInlineCacheCount(disabledPolyInlineCacheCount);
                    }
                }
                else
                {
                    IncInlineCacheCount(nullPolyInlineCacheCount);
                }

                if (polymorphicInlineCache != nullptr)
                {
                    if (PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
                    {
                        if (IsInlinee) Output::Print(L"\t");
                        Output::Print(L"\t%d: PIC size = %d\n", i, polymorphicInlineCache->GetSize());
#if DBG_DUMP
                        polymorphicInlineCache->Dump();
#endif
                    }
                    else if (PHASE_TRACE1(Js::PolymorphicInlineCachePhase))
                    {
                        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                        Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(i);
                        Js::PropertyRecord const * const propertyRecord = functionBody->GetScriptContext()->GetPropertyName(propertyId);
                        Output::Print(L"Trace PIC JIT function %s (%s) field: %s (index: %d) \n", functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer),
                            propertyRecord->GetBuffer(), i);
                    }

                    byte polyCacheUtil = profileData->GetFldInfo(functionBody, i)->polymorphicInlineCacheUtilization;
                    entryPoint->GetPolymorphicInlineCacheInfo()->SetPolymorphicInlineCache(functionBody, i, polymorphicInlineCache, IsInlinee, polyCacheUtil);
                    if (IsInlinee)
                    {
                        Assert(entryPoint->GetPolymorphicInlineCacheInfo()->GetInlineeInfo(functionBody)->GetPolymorphicInlineCaches()->GetInlineCache(functionBody, i) == polymorphicInlineCache);
                    }
                    else
                    {
                        Assert(entryPoint->GetPolymorphicInlineCacheInfo()->GetSelfInfo()->GetPolymorphicInlineCaches()->GetInlineCache(functionBody, i) == polymorphicInlineCache);
                    }
                }
                else if(IsInlinee && CONFIG_FLAG(CloneInlinedPolymorphicCaches))
                {
                    // Clone polymorphic inline caches for runtime usage in this inlinee. The JIT should only use the pointers to
                    // the inline caches, as their cached data is not guaranteed to be stable while jitting.
                    Js::InlineCache *const inlineCache =
                        function && Js::ScriptFunctionWithInlineCache::Is(function)
                            ? Js::ScriptFunctionWithInlineCache::FromVar(function)->GetInlineCache(i)
                            : functionBody->GetInlineCache(i);
                    Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(i);
                    const auto clone = runtimeData->ClonedInlineCaches()->GetInlineCache(functionBody, i);
                    if (clone)
                    {
                        inlineCache->CopyTo(propertyId, functionBody->GetScriptContext(), clone);
                    }
                    else
                    {
                        runtimeData->ClonedInlineCaches()->SetInlineCache(
                            recycler,
                            functionBody,
                            i,
                            inlineCache->Clone(propertyId, functionBody->GetScriptContext()));
                    }
                }
            }
        }
    }

    // Gather code gen data for inlinees

    if(IsInlinee ? !inliningDecider.InlineIntoInliner(functionBody) : !inliningDecider.InlineIntoTopFunc())
    {
        return;
    }

    class AutoCleanup
    {
    private:
        Js::FunctionBody *const functionBody;

    public:
        AutoCleanup(Js::FunctionBody *const functionBody) : functionBody(functionBody)
        {
            functionBody->OnBeginInlineInto();
        }

        ~AutoCleanup()
        {
            functionBody->OnEndInlineInto();
        }
    } autoCleanup(functionBody);

    const auto profiledCallSiteCount = functionBody->GetProfiledCallSiteCount();
    Assert(profiledCallSiteCount != 0 || functionBody->GetAnyDynamicProfileInfo()->hasLdFldCallSiteInfo());
    if (profiledCallSiteCount && !isJitTimeDataComputed)
    {
        jitTimeData->inlineesBv = BVFixed::New<Recycler>(profiledCallSiteCount, recycler);
    }

    // Iterate through profiled call sites recursively and determine what should be inlined
    for(Js::ProfileId profiledCallSiteId = 0; profiledCallSiteId < profiledCallSiteCount; ++profiledCallSiteId)
    {
        Js::FunctionInfo *const inlinee = inliningDecider.InlineCallSite(functionBody, profiledCallSiteId, recursiveInlineDepth);
        if(!inlinee)
        {
            if (profileData->CallSiteHasProfileData(profiledCallSiteId))
            {
                jitTimeData->inlineesBv->Set(profiledCallSiteId);
            }

            //Try and see if this polymorphic call
            Js::FunctionBody* inlineeFunctionBodyArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = {0};
            bool canInlineArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = { 0 };
            uint polyInlineeCount = inliningDecider.InlinePolymorhicCallSite(functionBody, profiledCallSiteId, inlineeFunctionBodyArray,
                Js::DynamicProfileInfo::maxPolymorphicInliningSize, canInlineArray);

            //We should be able to inline at least two functions here.
            if (polyInlineeCount >= 2)
            {
                for (uint id = 0; id < polyInlineeCount; id++)
                {
                    bool isInlined = canInlineArray[id];

                    Js::FunctionCodeGenRuntimeData  *inlineeRunTimeData = IsInlinee ? runtimeData->EnsureInlinee(recycler, profiledCallSiteId, inlineeFunctionBodyArray[id]) : functionBody->EnsureInlineeCodeGenRuntimeData(recycler, profiledCallSiteId, inlineeFunctionBodyArray[id]);

                    if (!isJitTimeDataComputed)
                    {
                        Js::FunctionCodeGenJitTimeData  *inlineeJitTimeData = jitTimeData->AddInlinee(recycler, profiledCallSiteId, inlineeFunctionBodyArray[id], isInlined);
                        if (isInlined)
                        {
                            GatherCodeGenData<true>(
                                recycler,
                                topFunctionBody,
                                inlineeFunctionBodyArray[id],
                                entryPoint,
                                inliningDecider,
                                objTypeSpecFldInfoList,
                                inlineeJitTimeData,
                                inlineeRunTimeData
                                );

                            AddInlineCacheStats(jitTimeData, inlineeJitTimeData);
                        }
                    }
                }
            }
        }
        else
        {

            jitTimeData->inlineesBv->Set(profiledCallSiteId);
            Js::FunctionBody *const inlineeFunctionBody = inlinee->GetFunctionBody();
            if(!inlineeFunctionBody )
            {
                if (!isJitTimeDataComputed)
                {
                    jitTimeData->AddInlinee(recycler, profiledCallSiteId, inlinee);
                }
                continue;
            }

            // We are at a callsite that can be inlined. Let the callsite be foo().
            // If foo has inline caches on it, we need to be able to get those for cloning.
            // To do this,
            //  1. Retrieve the inline cache associated with the load of "foo",
            //  2. Try to get the fixed function object corresponding to "foo",
            //  3. Pass the fixed function object to GatherCodeGenData which can clone its inline caches.

            uint ldFldInlineCacheIndex = profileData->GetLdFldCacheIndexFromCallSiteInfo(functionBody, profiledCallSiteId);
            Js::InlineCache * inlineCache = nullptr;
            if ((ldFldInlineCacheIndex != Js::Constants::NoInlineCacheIndex) && (ldFldInlineCacheIndex < functionBody->GetInlineCacheCount()))
            {
                if(function && Js::ScriptFunctionWithInlineCache::Is(function))
                {
                    inlineCache = Js::ScriptFunctionWithInlineCache::FromVar(function)->GetInlineCache(ldFldInlineCacheIndex);
                }
                else
                {
                    inlineCache = functionBody->GetInlineCache(ldFldInlineCacheIndex);
                }
            }

            Js::JavascriptFunction* fixedFunctionObject = nullptr;
            if (inlineCache && (inlineCache->IsLocal() || inlineCache->IsProto()))
            {
                inlineCache->TryGetFixedMethodFromCache(functionBody, ldFldInlineCacheIndex, &fixedFunctionObject);
            }

            if (fixedFunctionObject && !fixedFunctionObject->GetFunctionInfo()->IsDeferred() && fixedFunctionObject->GetFunctionBody() != inlineeFunctionBody)
            {
                fixedFunctionObject = nullptr;
            }

            if (!PHASE_OFF(Js::InlineRecursivePhase, functionBody))
            {
                if (!isJitTimeDataComputed)
                {
                    Js::FunctionCodeGenRuntimeData *inlineeRuntimeData = IsInlinee ? runtimeData->EnsureInlinee(recycler, profiledCallSiteId, inlineeFunctionBody) : functionBody->EnsureInlineeCodeGenRuntimeData(recycler, profiledCallSiteId, inlineeFunctionBody);
                    Js::FunctionCodeGenJitTimeData  *inlineeJitTimeData = nullptr;
                    bool doShareJitTimeData = false;

                    // Share the jitTime data if i) it is a recursive call, ii) jitTimeData is not from a polymorphic chain, and iii) all the call sites are recursive
                    if (functionBody == inlineeFunctionBody   // recursive call
                        && jitTimeData->GetNext() == nullptr        // not from a polymorphic  call site
                        && profiledCallSiteCount == functionBody->GetNumberOfRecursiveCallSites()) // all the callsites are recursive
                    {
                        jitTimeData->SetupRecursiveInlineeChain(recycler, profiledCallSiteId);
                        inlineeJitTimeData = jitTimeData;
                        doShareJitTimeData = true;

                        // If a recursive  inliner has multiple recursive inlinees and if they hit the InlineCountMax
                        // threshold, then runtimeData for the inlinees may not be available (bug 2269097) for the inlinees
                        // as InlineCountMax threshold heuristics has higher priority than recursive inline heuristics. Since
                        // we share runtime data between recursive inliner and recursive inlinees, and all the call sites
                        // are recursive (we only do recursive inlining for functions where all the callsites are recursive),
                        // we can iterate over all the callsites of the inliner and setup the runtime data recursive inlinee chain

                        for (Js::ProfileId id = 0; id < profiledCallSiteCount; id++)
                        {
                            inlineeRuntimeData->SetupRecursiveInlineeChain(recycler, id, inlineeFunctionBody);
                        }
                    }
                    else
                    {
                        inlineeJitTimeData = jitTimeData->AddInlinee(recycler, profiledCallSiteId, inlinee);
                    }

                    GatherCodeGenData<true>(
                        recycler,
                        topFunctionBody,
                        inlineeFunctionBody,
                        entryPoint,
                        inliningDecider,
                        objTypeSpecFldInfoList,
                        inlineeJitTimeData,
                        inlineeRuntimeData,
                        fixedFunctionObject,
                        doShareJitTimeData,
                        functionBody == inlineeFunctionBody ? recursiveInlineDepth + 1 : 0);

                    if (jitTimeData != inlineeJitTimeData)
                    {
                        AddInlineCacheStats(jitTimeData, inlineeJitTimeData);
                    }
                }
            }
            else
            {
                Js::FunctionCodeGenJitTimeData *const inlineeJitTimeData = jitTimeData->AddInlinee(recycler, profiledCallSiteId, inlinee);
                GatherCodeGenData<true>(
                    recycler,
                    topFunctionBody,
                    inlineeFunctionBody,
                    entryPoint,
                    inliningDecider,
                    objTypeSpecFldInfoList,
                    inlineeJitTimeData,
                    IsInlinee
                    ? runtimeData->EnsureInlinee(recycler, profiledCallSiteId, inlineeFunctionBody)
                    : functionBody->EnsureInlineeCodeGenRuntimeData(recycler, profiledCallSiteId, inlineeFunctionBody),
                    fixedFunctionObject);

                    AddInlineCacheStats(jitTimeData, inlineeJitTimeData);
            }
        }
    }

    // Iterate through inlineCache getter setter and apply call sites recursively and determine what should be inlined
    if (inlineGetterSetter || inlineApplyTarget || inlineCallTarget)
    {
        for(uint inlineCacheIndex = 0; inlineCacheIndex < functionBody->GetInlineCacheCount(); ++inlineCacheIndex)
        {
            const auto cacheType = profileData->GetFldInfo(functionBody, inlineCacheIndex)->flags;
            if(cacheType == Js::FldInfo_NoInfo)
            {
                continue;
            }

            bool getSetInlineCandidate = inlineGetterSetter && ((cacheType & Js::FldInfo_FromAccessor) != 0);
            bool callApplyInlineCandidate = (inlineCallTarget || inlineApplyTarget) && ((cacheType & Js::FldInfo_FromAccessor) == 0);

            // 1. Do not inline if the x in a.x is both a getter/setter and is followed by a .apply
            // 2. If we were optimistic earlier in assuming that the inline caches on the function object would be monomorphic and asserted that we may possibly inline apply target,
            //    then even if the field info flags say that the field access may be polymorphic, carry that optimism forward and try to inline apply target.
            if (getSetInlineCandidate ^ callApplyInlineCandidate)
            {
                Js::ObjTypeSpecFldInfo* info = jitTimeData->GetObjTypeSpecFldInfoArray()->GetInfo(functionBody, inlineCacheIndex);
                if (info == nullptr)
                {
                    continue;
                }

                if (!(getSetInlineCandidate && info->UsesAccessor()) && !(callApplyInlineCandidate && !info->IsPoly()))
                {
                    continue;
                }

                Js::JavascriptFunction* inlineeFunction = info->GetFieldValueAsFunctionIfAvailable();

                if (inlineeFunction == nullptr)
                {
                    continue;
                }

                Js::FunctionInfo* inlineeFunctionInfo = inlineeFunction->GetFunctionInfo();

                Js::FunctionProxy* inlineeFunctionProxy = inlineeFunctionInfo->GetFunctionProxy();
                if (inlineeFunctionProxy != nullptr && !functionBody->CheckCalleeContextForInlining(inlineeFunctionProxy))
                {
                    continue;
                }

                const auto inlinee = inliningDecider.Inline(functionBody, inlineeFunctionInfo, false /*isConstructorCall*/, false /*isPolymorphicCall*/, 0, (uint16)inlineCacheIndex, 0, false);
                if(!inlinee)
                {
                    continue;
                }

                const auto inlineeFunctionBody = inlinee->GetFunctionBody();
                if(!inlineeFunctionBody)
                {
                    if ((
#ifdef ENABLE_DOM_FAST_PATH
                         inlinee->GetLocalFunctionId() == Js::JavascriptBuiltInFunction::DOMFastPathGetter ||
                         inlinee->GetLocalFunctionId() == Js::JavascriptBuiltInFunction::DOMFastPathSetter ||
#endif
                         (inlineeFunctionInfo->GetAttributes() & Js::FunctionInfo::Attributes::BuiltInInlinableAsLdFldInlinee) != 0) &&
                        !isJitTimeDataComputed)
                    {
                        jitTimeData->AddLdFldInlinee(recycler, inlineCacheIndex, inlinee);
                    }
                    continue;
                }

                Js::FunctionCodeGenRuntimeData *const inlineeRuntimeData = IsInlinee ? runtimeData->EnsureLdFldInlinee(recycler, inlineCacheIndex, inlineeFunctionBody) :
                    functionBody->EnsureLdFldInlineeCodeGenRuntimeData(recycler, inlineCacheIndex, inlineeFunctionBody);

                    if (inlineeRuntimeData->GetFunctionBody() != inlineeFunctionBody)
                    {
                        //There are obscure cases where profileData has not yet seen the polymorphic LdFld but the inlineCache has the newer object from which getter is invoked.
                        //In this case we don't want to inline that getter. Polymorphic bit will be set later correctly.
                        //See WinBlue 54540
                        continue;
                    }


                    if (!isJitTimeDataComputed)
                    {
                        Js::FunctionCodeGenJitTimeData *inlineeJitTimeData =  jitTimeData->AddLdFldInlinee(recycler, inlineCacheIndex, inlinee);
                        GatherCodeGenData<true>(
                            recycler,
                            topFunctionBody,
                            inlineeFunctionBody,
                            entryPoint,
                            inliningDecider,
                            objTypeSpecFldInfoList,
                            inlineeJitTimeData,
                            inlineeRuntimeData,
                            nullptr);

                        AddInlineCacheStats(jitTimeData, inlineeJitTimeData);
                    }
                }
            }
        }

#ifdef FIELD_ACCESS_STATS
    if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_VERBOSE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
    {
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        if (jitTimeData->inlineCacheStats)
        {
            Output::Print(L"ObTypeSpec: gathered code gen data for function %s (#%u) inlined %s (#%u): inline cache stats:\n",
                topFunctionBody->GetDisplayName(), topFunctionBody->GetFunctionNumber(), functionBody->GetDisplayName(), functionBody->GetFunctionNumber());
            Output::Print(L"    overall: total %u, no profile info %u\n",
                jitTimeData->inlineCacheStats->totalInlineCacheCount, jitTimeData->inlineCacheStats->noInfoInlineCacheCount);
            Output::Print(L"    mono: total %u, empty %u, cloned %u\n",
                jitTimeData->inlineCacheStats->monoInlineCacheCount, jitTimeData->inlineCacheStats->emptyMonoInlineCacheCount,
                jitTimeData->inlineCacheStats->clonedMonoInlineCacheCount);
            Output::Print(L"    poly: total %u (high %u, low %u), empty %u, equivalent %u, cloned %u\n",
                jitTimeData->inlineCacheStats->polyInlineCacheCount, jitTimeData->inlineCacheStats->highUtilPolyInlineCacheCount,
                jitTimeData->inlineCacheStats->lowUtilPolyInlineCacheCount, jitTimeData->inlineCacheStats->emptyPolyInlineCacheCount,
                jitTimeData->inlineCacheStats->equivPolyInlineCacheCount, jitTimeData->inlineCacheStats->clonedPolyInlineCacheCount);
        }
        else
        {
            Output::Print(L"ObTypeSpec: function %s (%s): inline cache stats unavailable\n", topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer));
        }
        Output::Flush();
    }
#endif

#undef SetInlineCacheCount
#undef IncInlineCacheCount
#undef AddInlineCacheStats
}

Js::CodeGenRecyclableData *
NativeCodeGenerator::GatherCodeGenData(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const functionBody, Js::EntryPointInfo *const entryPoint, CodeGenWorkItem* workItem, void* function)
{
    ASSERT_THREAD();
    Assert(functionBody);

#ifdef PROFILE_EXEC
    class AutoProfile
    {
    private:
        Js::ScriptContextProfiler *const codeGenProfiler;

    public:
        AutoProfile(Js::ScriptContextProfiler *const codeGenProfiler) : codeGenProfiler(codeGenProfiler)
        {
            ProfileBegin(codeGenProfiler, Js::DelayPhase);
            ProfileBegin(codeGenProfiler, Js::GatherCodeGenDataPhase);
        }

        ~AutoProfile()
        {
            ProfileEnd(codeGenProfiler, Js::GatherCodeGenDataPhase);
            ProfileEnd(codeGenProfiler, Js::DelayPhase);
        }
    } autoProfile(foregroundCodeGenProfiler);
#endif

    const auto recycler = scriptContext->GetRecycler();
    {
        const auto jitTimeData = RecyclerNew(recycler, Js::FunctionCodeGenJitTimeData, functionBody, entryPoint);
        InliningDecider inliningDecider(functionBody, workItem->Type() == JsLoopBodyWorkItemType, this->IsInDebugMode(), workItem->GetJitMode());

        BEGIN_TEMP_ALLOCATOR(gatherCodeGenDataAllocator, scriptContext, L"GatherCodeGenData");

        ObjTypeSpecFldInfoList* objTypeSpecFldInfoList = JitAnew(gatherCodeGenDataAllocator, ObjTypeSpecFldInfoList, gatherCodeGenDataAllocator);

#if ENABLE_DEBUG_CONFIG_OPTIONS
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        if (PHASE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
        {
            Output::Print(L"ObjTypeSpec: top function %s (%s), function %s (%s): GatherCodeGenData(): \n",
                topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer2));
        }
#endif
        GatherCodeGenData<false>(recycler, topFunctionBody, functionBody, entryPoint, inliningDecider, objTypeSpecFldInfoList, jitTimeData, nullptr, function ? Js::JavascriptFunction::FromVar(function) : nullptr, 0);

#ifdef FIELD_ACCESS_STATS
        Js::FieldAccessStats* fieldAccessStats = entryPoint->EnsureFieldAccessStats(recycler);
        fieldAccessStats->Add(jitTimeData->inlineCacheStats);
        entryPoint->GetScriptContext()->RecordFieldAccessStats(topFunctionBody, fieldAccessStats);
#endif

#ifdef FIELD_ACCESS_STATS
        if (PHASE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
        {
            auto stats = jitTimeData->inlineCacheStats;
            Output::Print(L"ObjTypeSpec: gathered code gen data for function %s (%s): inline cache stats:\n", topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer));
            Output::Print(L"    overall: total %u, no profile info %u\n", stats->totalInlineCacheCount, stats->noInfoInlineCacheCount);
            Output::Print(L"    mono: total %u, empty %u, cloned %u\n",
                stats->monoInlineCacheCount, stats->emptyMonoInlineCacheCount, stats->clonedMonoInlineCacheCount);
            Output::Print(L"    poly: total %u (high %u, low %u), null %u, empty %u, ignored %u, disabled %u, equivalent %u, non-equivalent %u, cloned %u\n",
                stats->polyInlineCacheCount, stats->highUtilPolyInlineCacheCount, stats->lowUtilPolyInlineCacheCount,
                stats->nullPolyInlineCacheCount, stats->emptyPolyInlineCacheCount, stats->ignoredPolyInlineCacheCount, stats->disabledPolyInlineCacheCount,
                stats->equivPolyInlineCacheCount, stats->nonEquivPolyInlineCacheCount, stats->clonedPolyInlineCacheCount);
        }
#endif

        uint objTypeSpecFldInfoCount = objTypeSpecFldInfoList->Count();
        jitTimeData->SetGlobalObjTypeSpecFldInfoArray(RecyclerNewArray(recycler, Js::ObjTypeSpecFldInfo*, objTypeSpecFldInfoCount), objTypeSpecFldInfoCount);
        uint propertyInfoId = objTypeSpecFldInfoCount - 1;
        FOREACH_SLISTCOUNTED_ENTRY(Js::ObjTypeSpecFldInfo*, info, objTypeSpecFldInfoList)
        {
            // Clear field values we don't need so we don't unnecessarily pin them while JIT-ing.
            if (!info->GetKeepFieldValue() && !(info->IsPoly() && info->DoesntHaveEquivalence()))
            {
                info->SetFieldValue(nullptr);
            }
            jitTimeData->SetGlobalObjTypeSpecFldInfo(propertyInfoId--, info);
        }
        NEXT_SLISTCOUNTED_ENTRY;

        END_TEMP_ALLOCATOR(gatherCodeGenDataAllocator, scriptContext);

        return RecyclerNew(recycler, Js::CodeGenRecyclableData, jitTimeData);
    }
}

bool
NativeCodeGenerator::IsBackgroundJIT() const
{
    return Processor()->ProcessesInBackground();
}

void
NativeCodeGenerator::EnterScriptStart()
{
    // We should be in execution
    Assert(scriptContext->GetThreadContext()->IsScriptActive());
    Assert(scriptContext->GetThreadContext()->IsInScript());

    if(CONFIG_FLAG(BgJitDelay) == 0 ||
        Js::Configuration::Global.flags.EnforceExecutionModeLimits ||
        scriptContext->GetThreadContext()->GetCallRootLevel() > 2)
    {
        return;
    }

    if (pendingCodeGenWorkItems == 0 || pendingCodeGenWorkItems > (uint)CONFIG_FLAG(BgJitPendingFuncCap))
    {
        // We have already finish code gen for this script context
        // Only wait if the script is small and we can easily pre-JIT all of it.
        return;
    }

    if (this->IsClosed())
    {
        return;
    }

    // Don't need to do anything if we're in debug mode
    if (this->IsInDebugMode() && !Js::Configuration::Global.EnableJitInDebugMode())
    {
        return;
    }

    // We've already done a few calls to this scriptContext, don't bother waiting.
    if (scriptContext->callCount >= 3)
    {
        return;
    }

    scriptContext->callCount++;

    if (scriptContext->GetDeferredBody())
    {
        OUTPUT_TRACE(Js::DelayPhase, L"No delay because the script has a deferred body\n");
        return;
    }

    if(CONFIG_FLAG(BgJitDelayFgBuffer) >= CONFIG_FLAG(BgJitDelay))
    {
        return;
    }

    class AutoCleanup
    {
    private:
        Js::ScriptContextProfiler *const codeGenProfiler;

    public:
        AutoCleanup(Js::ScriptContextProfiler *const codeGenProfiler) : codeGenProfiler(codeGenProfiler)
        {
            JS_ETW(EventWriteJSCRIPT_NATIVECODEGEN_DELAY_START(this, 0));
#ifdef PROFILE_EXEC
            ProfileBegin(codeGenProfiler, Js::DelayPhase);
            ProfileBegin(codeGenProfiler, Js::SpeculationPhase);
#endif
        }

        ~AutoCleanup()
        {
#ifdef PROFILE_EXEC
            ProfileEnd(codeGenProfiler, Js::SpeculationPhase);
            ProfileEnd(codeGenProfiler, Js::DelayPhase);
#endif
            JS_ETW(EventWriteJSCRIPT_NATIVECODEGEN_DELAY_STOP(this, 0));
        }
    } autoCleanup(
#ifdef PROFILE_EXEC
        this->foregroundCodeGenProfiler
#else
        nullptr
#endif
        );

    Processor()->PrioritizeManagerAndWait(this, CONFIG_FLAG(BgJitDelay) - CONFIG_FLAG(BgJitDelayFgBuffer));
}

// Is the given address within one of our JIT'd frame?
bool
IsNativeFunctionAddr(Js::ScriptContext *scriptContext, void * address)
{
    if (!scriptContext->GetNativeCodeGenerator())
    {
        return false;
    }

    return scriptContext->GetNativeCodeGenerator()->IsNativeFunctionAddr(address);
}

void
FreeNativeCodeGenAllocation(Js::ScriptContext *scriptContext, void * address)
{
    if (!scriptContext->GetNativeCodeGenerator())
    {
        return;
    }

    scriptContext->GetNativeCodeGenerator()->QueueFreeNativeCodeGenAllocation(address);
}

bool TryReleaseNonHiPriWorkItem(Js::ScriptContext* scriptContext, CodeGenWorkItem* workItem)
{
    if (!scriptContext->GetNativeCodeGenerator())
    {
        return false;
    }

    return scriptContext->GetNativeCodeGenerator()->TryReleaseNonHiPriWorkItem(workItem);
}

// Called from within the lock
// The work item cannot be used after this point if it returns true
bool NativeCodeGenerator::TryReleaseNonHiPriWorkItem(CodeGenWorkItem* workItem)
{
    // If its the highest priority, don't release it, let the job continue
    if (workItem->IsInJitQueue())
    {
        return false;
    }

    workItems.Unlink(workItem);
    Assert(!workItem->RecyclableData());

    workItem->Delete();
    return true;
}

// Called on the same thread that did the allocation
bool
NativeCodeGenerator::IsNativeFunctionAddr(void * address)
{
    return
        (this->backgroundAllocators && this->backgroundAllocators->emitBufferManager.IsInRange(address)) ||
        (this->foregroundAllocators && this->foregroundAllocators->emitBufferManager.IsInRange(address));
}

void
NativeCodeGenerator::FreeNativeCodeGenAllocation(void* address)
{
    if(this->backgroundAllocators)
    {
        this->backgroundAllocators->emitBufferManager.FreeAllocation(address);
    }
}

void
NativeCodeGenerator::QueueFreeNativeCodeGenAllocation(void* address)
{
    ASSERT_THREAD();

    if(IsClosed())
    {
        return;
    }

    //DeRegister Entry Point for CFG
    ThreadContext::GetContextForCurrentThread()->SetValidCallTargetForCFG(address, false);

    // The foreground allocators may have been used
    if(this->foregroundAllocators && this->foregroundAllocators->emitBufferManager.FreeAllocation(address))
    {
        return;
    }

    // The background allocators were used. Queue a job to free the allocation from the background thread.
    this->freeLoopBodyManager.QueueFreeLoopBodyJob(address);
}

void NativeCodeGenerator::FreeLoopBodyJobManager::QueueFreeLoopBodyJob(void* codeAddress)
{
    Assert(!this->isClosed);

    FreeLoopBodyJob* job = HeapNewNoThrow(FreeLoopBodyJob, this, codeAddress);

    if (job == nullptr)
    {
        FreeLoopBodyJob stackJob(this, codeAddress, false /* heapAllocated */);

        {
            AutoOptionalCriticalSection lock(Processor()->GetCriticalSection());
            Processor()->AddJob(&stackJob);
        }
        Processor()->PrioritizeJobAndWait(this, &stackJob);
    }
    else
    {
        AutoOptionalCriticalSection lock(Processor()->GetCriticalSection());
        if (Processor()->HasManager(this))
        {
            Processor()->AddJobAndProcessProactively<FreeLoopBodyJobManager, FreeLoopBodyJob*>(this, job);
        }
        else
        {
            HeapDelete(job);
        }
    }
}

#ifdef PROFILE_EXEC
void
NativeCodeGenerator::CreateProfiler(Js::ScriptContextProfiler * profiler)
{
    Assert(this->foregroundCodeGenProfiler == nullptr);

    this->foregroundCodeGenProfiler = profiler;
    profiler->AddRef();

}

Js::ScriptContextProfiler *
NativeCodeGenerator::EnsureForegroundCodeGenProfiler()
{
    if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
    {
        Assert(this->foregroundCodeGenProfiler != nullptr);
        Assert(this->foregroundCodeGenProfiler->IsInitialized());
    }

    return this->foregroundCodeGenProfiler;
}


void
NativeCodeGenerator::SetProfilerFromNativeCodeGen(NativeCodeGenerator * nativeCodeGen)
{
    Assert(Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag));

    Assert(this->foregroundCodeGenProfiler != nullptr);
    Assert(this->foregroundCodeGenProfiler->IsInitialized());
    Assert(nativeCodeGen->foregroundCodeGenProfiler != nullptr);
    Assert(nativeCodeGen->foregroundCodeGenProfiler->IsInitialized());

    this->foregroundCodeGenProfiler->Release();
    this->foregroundCodeGenProfiler = nativeCodeGen->foregroundCodeGenProfiler;
    this->foregroundCodeGenProfiler->AddRef();
}

void
NativeCodeGenerator::ProfilePrint()
{
    Js::ScriptContextProfiler *codegenProfiler = this->backgroundCodeGenProfiler;
    if (Js::Configuration::Global.flags.Verbose)
    {
        //Print individual CodegenProfiler information in verbose mode
        while (codegenProfiler)
        {
            codegenProfiler->ProfilePrint(Js::Configuration::Global.flags.Profile.GetFirstPhase());
            codegenProfiler = codegenProfiler->next;
        }
    }
    else
    {
        //Merge all the codegenProfiler for single snapshot.
        codegenProfiler = codegenProfiler->next;
        while (codegenProfiler)
        {
            this->backgroundCodeGenProfiler->ProfileMerge(codegenProfiler);
            codegenProfiler = codegenProfiler->next;
        }
        this->backgroundCodeGenProfiler->ProfilePrint(Js::Configuration::Global.flags.Profile.GetFirstPhase());
    }
}

void
NativeCodeGenerator::ProfileBegin(Js::ScriptContextProfiler *const profiler, Js::Phase phase)
{
    AssertMsg((profiler != nullptr) == Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag),
        "Profiler tag is supplied but the profiler pointer is NULL");
    if (profiler)
    {
        profiler->ProfileBegin(phase);
    }
}

void
NativeCodeGenerator::ProfileEnd(Js::ScriptContextProfiler *const profiler, Js::Phase phase)
{
    AssertMsg((profiler != nullptr) == Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag),
        "Profiler tag is supplied but the profiler pointer is NULL");
    if (profiler)
    {
        profiler->ProfileEnd(phase);
    }
}

#endif

void NativeCodeGenerator::AddToJitQueue(CodeGenWorkItem *const codeGenWorkItem, bool prioritize, bool lock, void* function)
{
    codeGenWorkItem->VerifyJitMode();

    Js::CodeGenRecyclableData* recyclableData = GatherCodeGenData(codeGenWorkItem->GetFunctionBody(), codeGenWorkItem->GetFunctionBody(), codeGenWorkItem->GetEntryPoint(), codeGenWorkItem, function);
    codeGenWorkItem->SetRecyclableData(recyclableData);

    AutoOptionalCriticalSection autoLock(lock ? Processor()->GetCriticalSection() : nullptr);
    scriptContext->GetThreadContext()->RegisterCodeGenRecyclableData(recyclableData);

    // If we have added a lot of jobs that are still waiting to be jitted, remove the oldest job
    // to ensure we do not spend time jitting stale work items.
    const ExecutionMode jitMode = codeGenWorkItem->GetJitMode();
    if(jitMode == ExecutionMode::FullJit &&
        queuedFullJitWorkItemCount >= (unsigned int)CONFIG_FLAG(JitQueueThreshold))
    {
        CodeGenWorkItem *const workItemRemoved = queuedFullJitWorkItems.Tail()->WorkItem();
        Assert(workItemRemoved->GetJitMode() == ExecutionMode::FullJit);
        if(Processor()->RemoveJob(workItemRemoved))
        {
            queuedFullJitWorkItems.UnlinkFromEnd();
            --queuedFullJitWorkItemCount;
            workItemRemoved->OnRemoveFromJitQueue(this);
        }
    }

    Processor()->AddJob(codeGenWorkItem, prioritize);   // This one can throw (really unlikely though), OOM specifically.
    if(jitMode == ExecutionMode::FullJit)
    {
        QueuedFullJitWorkItem *const queuedFullJitWorkItem = codeGenWorkItem->EnsureQueuedFullJitWorkItem();
        if(queuedFullJitWorkItem) // ignore OOM, this work item just won't be removed from the job processor's queue
        {
            if(prioritize)
            {
                queuedFullJitWorkItems.LinkToBeginning(queuedFullJitWorkItem);
            }
            else
            {
                queuedFullJitWorkItems.LinkToEnd(queuedFullJitWorkItem);
            }
            ++queuedFullJitWorkItemCount;
        }
    }
    codeGenWorkItem->OnAddToJitQueue();
}

void NativeCodeGenerator::AddWorkItem(CodeGenWorkItem* workitem)
{
    workitem->ResetJitMode();
    workItems.LinkToEnd(workitem);
}

Js::ScriptContextProfiler * NativeCodeGenerator::GetBackgroundCodeGenProfiler(PageAllocator *allocator)
{
#ifdef  PROFILE_EXEC
    if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
    {
        Js::ScriptContextProfiler *codegenProfiler = this->backgroundCodeGenProfiler;
        while (codegenProfiler)
        {
            if (codegenProfiler->pageAllocator == allocator)
            {
                if (!codegenProfiler->IsInitialized())
                {
                    codegenProfiler->Initialize(allocator, nullptr);
                }
                return codegenProfiler;
            }
            codegenProfiler = codegenProfiler->next;
        }
        Assert(false);
    }
    return nullptr;
#else
    return nullptr;
#endif
}

void NativeCodeGenerator::AllocateBackgroundCodeGenProfiler(PageAllocator *pageAllocator)
{
#ifdef PROFILE_EXEC
if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
{
    Js::ScriptContextProfiler *codegenProfiler = NoCheckHeapNew(Js::ScriptContextProfiler);
    codegenProfiler->pageAllocator = pageAllocator;
    codegenProfiler->next = this->backgroundCodeGenProfiler;
    this->backgroundCodeGenProfiler = codegenProfiler;
}
#endif
}

bool NativeCodeGenerator::TryAggressiveInlining(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const inlineeFunctionBody, InliningDecider &inliningDecider, uint& inlineeCount, uint recursiveInlineDepth)
{
    PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

    if (!inlineeFunctionBody->GetProfiledCallSiteCount())
    {
        // Nothing to inline. See this as fully inlineable function.
        return true;
    }

    class AutoCleanup
    {
    private:
        Js::FunctionBody *const functionBody;

    public:
        AutoCleanup(Js::FunctionBody *const functionBody) : functionBody(functionBody)
        {
            functionBody->OnBeginInlineInto();
        }

        ~AutoCleanup()
        {
            functionBody->OnEndInlineInto();
        }
    } autoCleanup(inlineeFunctionBody);

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    class AutoTrace
    {

        Js::FunctionBody *const topFunc;
        Js::FunctionBody *const inlineeFunc;
        uint32& inlineeCount;
        bool done;

        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

    public:
        AutoTrace(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const inlineeFunctionBody, uint32& inlineeCount) : topFunc(topFunctionBody),
            inlineeFunc(inlineeFunctionBody), done(false), inlineeCount(inlineeCount)
        {
            if (topFunc == inlineeFunc)
            {
                INLINE_TESTTRACE(L"INLINING: Recursive tryAggressiveInlining started topFunc: %s (%s)\n", topFunc->GetDisplayName(),
                    topFunc->GetDebugNumberSet(debugStringBuffer))
            }
        }
        void Done(bool success)
        {
            if (success)
            {
                done = true;
                if (topFunc == inlineeFunc)
                {
                    INLINE_TESTTRACE(L"INLINING: Recursive tryAggressiveInlining succeeded topFunc: %s (%s), inlinee count: %d\n", topFunc->GetDisplayName(),
                        topFunc->GetDebugNumberSet(debugStringBuffer), inlineeCount);
                }
                else
                {
                    INLINE_TESTTRACE(L"INLINING: TryAggressiveInlining succeeded topFunc: %s (%s), inlinee: %s (%s) \n", topFunc->GetDisplayName(),
                        topFunc->GetDebugNumberSet(debugStringBuffer),
                        inlineeFunc->GetDisplayName(),
                        inlineeFunc->GetDebugNumberSet(debugStringBuffer2));
                }
            }
            else
            {
                Assert(done == false);
            }
        }
        void TraceFailure(const wchar_t *message)
        {

            INLINE_TESTTRACE(L"INLINING: TryAggressiveInlining failed topFunc (%s): %s (%s), inlinee: %s (%s) \n", message, topFunc->GetDisplayName(),
                topFunc->GetDebugNumberSet(debugStringBuffer),
                inlineeFunc->GetDisplayName(),
                inlineeFunc->GetDebugNumberSet(debugStringBuffer2));
        }
        ~AutoTrace()
        {
            if (!done)
            {
                if (topFunc == inlineeFunc)
                {
                    INLINE_TESTTRACE(L"INLINING: Recursive tryAggressiveInlining failed topFunc: %s (%s)\n", topFunc->GetDisplayName(),
                        topFunc->GetDebugNumberSet(debugStringBuffer));
                }
                else
                {
                    INLINE_TESTTRACE(L"INLINING: TryAggressiveInlining failed topFunc: %s (%s), inlinee: %s (%s) \n", topFunc->GetDisplayName(),
                        topFunc->GetDebugNumberSet(debugStringBuffer),
                        inlineeFunc->GetDisplayName(),
                        inlineeFunc->GetDebugNumberSet(debugStringBuffer2));
                }

            }
        }
    };

    AutoTrace trace(topFunctionBody, inlineeFunctionBody, inlineeCount);
#endif

    if (inlineeFunctionBody->GetProfiledSwitchCount())
    {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        trace.TraceFailure(L"Switch statement in inlinee");
#endif
        return false;
    }

    bool isInlinee = topFunctionBody != inlineeFunctionBody;

    if (isInlinee ? !inliningDecider.InlineIntoInliner(inlineeFunctionBody) : !inliningDecider.InlineIntoTopFunc())
    {
        return false;
    }

    const auto profiledCallSiteCount = inlineeFunctionBody->GetProfiledCallSiteCount();
    for (Js::ProfileId profiledCallSiteId = 0; profiledCallSiteId < profiledCallSiteCount; ++profiledCallSiteId)
    {
        bool isConstructorCall = false;
        bool isPolymorphicCall = false;

        if (!inliningDecider.HasCallSiteInfo(inlineeFunctionBody, profiledCallSiteId))
        {
            //There is no callsite information. We should hit bailonnoprofile for these callsites. Ignore.
            continue;
        }

        Js::FunctionInfo *inlinee = inliningDecider.GetCallSiteFuncInfo(inlineeFunctionBody, profiledCallSiteId, &isConstructorCall, &isPolymorphicCall);
        if (!inlinee)
        {
            if (isPolymorphicCall)
            {
                //Try and see if this polymorphic call
                Js::FunctionBody* inlineeFunctionBodyArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = { 0 };
                bool canInlineArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = { 0 };
                uint polyInlineeCount = inliningDecider.InlinePolymorhicCallSite(inlineeFunctionBody, profiledCallSiteId, inlineeFunctionBodyArray,
                    Js::DynamicProfileInfo::maxPolymorphicInliningSize, canInlineArray);

                //We should be able to inline everything here.
                if (polyInlineeCount >= 2)
                {
                    for (uint i = 0; i < polyInlineeCount; i++)
                    {
                        bool isInlined = canInlineArray[i];
                        if (isInlined)
                        {
                            ++inlineeCount;
                            if (!TryAggressiveInlining(topFunctionBody, inlineeFunctionBodyArray[i], inliningDecider, inlineeCount, inlineeFunctionBody == inlineeFunctionBodyArray[i] ? recursiveInlineDepth + 1 : 0))
                            {
                                return false;
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        else
        {
            inlinee = inliningDecider.Inline(inlineeFunctionBody, inlinee, isConstructorCall, false, inliningDecider.GetConstantArgInfo(inlineeFunctionBody, profiledCallSiteId), profiledCallSiteId, inlineeFunctionBody == inlinee ? recursiveInlineDepth + 1 : 0, true);
            if (!inlinee)
            {
                return false;
            }
            Js::FunctionBody *const functionBody = inlinee->GetFunctionBody();
            if (!functionBody)
            {
                //Built-in
                continue;
            }
            //Recursive call
            ++inlineeCount;
            if (!TryAggressiveInlining(topFunctionBody, functionBody, inliningDecider, inlineeCount, inlineeFunctionBody == functionBody ? recursiveInlineDepth + 1 : 0 ))
            {
                return false;
            }
        }
    }

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    trace.Done(true);
#endif
    return true;
}
