//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct CodeGenWorkItem;
struct JsFunctionCodeGen;
struct JsLoopBodyCodeGen;
class InliningDecider;
namespace Js
{
    class ObjTypeSpecFldInfo;
    class FunctionCodeGenJitTimeData;
};

class NativeCodeGenerator sealed : public JsUtil::WaitableJobManager
{
#if ENABLE_DEBUG_CONFIG_OPTIONS
    static volatile UINT_PTR CodegenFailureSeed;
#endif

    friend JsUtil::ForegroundJobProcessor;
    friend JsUtil::BackgroundJobProcessor;
    friend Js::RemoteScriptContext;

public:
    NativeCodeGenerator(Js::ScriptContext * scriptContext);
    ~NativeCodeGenerator();
    void Close();

    JsFunctionCodeGen * NewFunctionCodeGen(Js::FunctionBody *functionBody, Js::EntryPointInfo* info);
    JsLoopBodyCodeGen * NewLoopBodyCodeGen(Js::FunctionBody *functionBody, Js::EntryPointInfo* info);

    bool GenerateFunction(Js::FunctionBody * fn, Js::ScriptFunction * function = nullptr);
    void GenerateLoopBody(Js::FunctionBody * functionBody, Js::LoopHeader * loopHeader, Js::EntryPointInfo* info = nullptr, uint localCount = 0, Js::Var localSlots[] = nullptr);
    static bool IsValidVar(const Js::Var var, Recycler *const recycler);

#ifdef ENABLE_PREJIT
    void GenerateAllFunctions(Js::FunctionBody * fn);
    bool DoBackEnd(Js::FunctionBody * fn);
#endif

#ifdef IR_VIEWER
    Js::Var RejitIRViewerFunction(Js::FunctionBody *fn, Js::ScriptContext *scriptContext);
#endif
void SetProfileMode(BOOL fSet);
public:
    static Js::Var CheckCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...);

#ifdef ASMJS_PLAT
    static Js::Var CheckAsmJsCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...);
#endif
    static bool IsThunk(Js::JavascriptMethod codeAddress);
    static bool IsAsmJsCodeGenThunk(Js::JavascriptMethod codeAddress);
    static CheckCodeGenFunction GetCheckCodeGenFunction(Js::JavascriptMethod codeAddress);
    static Js::JavascriptMethod CheckCodeGen(Js::ScriptFunction * function);
    static Js::Var CheckAsmJsCodeGen(Js::ScriptFunction * function);

public:
    static void Jit_TransitionFromSimpleJit(void *const framePointer);
private:
    static void TransitionFromSimpleJit(Js::ScriptFunction *const function);

private:
    static Js::JavascriptMethod CheckCodeGenDone(Js::FunctionBody *const functionBody, Js::FunctionEntryPointInfo *const entryPointInfo, Js::ScriptFunction * function);
    CodeGenWorkItem *GetJob(Js::EntryPointInfo *const entryPoint) const;
    bool WasAddedToJobProcessor(JsUtil::Job *const job) const;
    bool ShouldProcessInForeground(const bool willWaitForJob, const unsigned int numJobsInQueue) const;
    void Prioritize(JsUtil::Job *const job, const bool forceAddJobToProcessor = false, void* function = nullptr);
    void PrioritizedButNotYetProcessed(JsUtil::Job *const job);
    void BeforeWaitForJob(Js::EntryPointInfo *const entryPoint) const;
    void AfterWaitForJob(Js::EntryPointInfo *const entryPoint) const;
    static bool WorkItemExceedsJITLimits(CodeGenWorkItem *const codeGenWork);
    virtual bool Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData) override;
    virtual void JobProcessed(JsUtil::Job *const job, const bool succeeded) override;
    JsUtil::Job *GetJobToProcessProactively();
    void AddToJitQueue(CodeGenWorkItem *const codeGenWorkItem, bool prioritize, bool lock, void* function = nullptr);
    void RemoveProactiveJobs();
    typedef SListCounted<Js::ObjTypeSpecFldInfo*, ArenaAllocator> ObjTypeSpecFldInfoList;

    template<bool IsInlinee> void GatherCodeGenData(
        Recycler *const recycler,
        Js::FunctionBody *const topFunctionBody,
        Js::FunctionBody *const functionBody,
        Js::EntryPointInfo *const entryPoint,
        InliningDecider &inliningDecider,
        ObjTypeSpecFldInfoList *objTypeSpecFldInfoList,
        Js::FunctionCodeGenJitTimeData *const jitTimeData,
        Js::FunctionCodeGenRuntimeData *const runtimeData,
        Js::JavascriptFunction* function = nullptr,
        bool isJitTimeDataComputed = false,
        uint32 recursiveInlineDepth = 0);
    Js::CodeGenRecyclableData *GatherCodeGenData(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const functionBody, Js::EntryPointInfo *const entryPoint, CodeGenWorkItem* workItem, void* function = nullptr);

public:
    void UpdateQueueForDebugMode();
    bool IsBackgroundJIT() const;
    void EnterScriptStart();
    bool IsNativeFunctionAddr(void * address);
    void FreeNativeCodeGenAllocation(void* address);
    bool TryReleaseNonHiPriWorkItem(CodeGenWorkItem* workItem);

    void QueueFreeNativeCodeGenAllocation(void* address);

    bool IsClosed() { return isClosed; }
    void AddWorkItem(CodeGenWorkItem* workItem);
    CodeGenAllocators* GetCodeGenAllocator(PageAllocator* pageallocator){ return EnsureForegroundAllocators(pageallocator); }

#if DBG_DUMP
    FILE * asmFile;
#endif

#ifdef PROFILE_EXEC
    void CreateProfiler(Js::ScriptContextProfiler * profiler);
    void SetProfilerFromNativeCodeGen(NativeCodeGenerator * nativeCodeGen);
    Js::ScriptContextProfiler *EnsureForegroundCodeGenProfiler();
    static void ProfileBegin(Js::ScriptContextProfiler *const profiler, Js::Phase);
    static void ProfileEnd(Js::ScriptContextProfiler *const profiler, Js::Phase);
    void ProfilePrint();
#endif

private:

    void CodeGen(PageAllocator * pageAllocator, CodeGenWorkItem* workItem, const bool foreground);

    CodeGenAllocators *CreateAllocators(PageAllocator *const pageAllocator)
    {
        return HeapNew(CodeGenAllocators, pageAllocator->GetAllocationPolicyManager(), scriptContext);
    }

    CodeGenAllocators *EnsureForegroundAllocators(PageAllocator * pageAllocator)
    {
        if (this->foregroundAllocators == nullptr)
        {
            this->foregroundAllocators = CreateAllocators(pageAllocator);

#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
            if (this->scriptContext->webWorkerId != Js::Constants::NonWebWorkerContextId)
            {
                this->foregroundAllocators->canCreatePreReservedSegment = true;
            }
#endif
        }

        return this->foregroundAllocators;
    }


    CodeGenAllocators * GetBackgroundAllocator(PageAllocator *pageAllocator)
    {
        return this->backgroundAllocators;
    }

    Js::ScriptContextProfiler * GetBackgroundCodeGenProfiler(PageAllocator *allocator);

    void  AllocateBackgroundCodeGenProfiler(PageAllocator * pageAllocator);

    void AllocateBackgroundAllocators(PageAllocator * pageAllocator)
    {
        if (!this->backgroundAllocators)
        {
            this->backgroundAllocators = CreateAllocators(pageAllocator);
#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
            this->backgroundAllocators->canCreatePreReservedSegment = true;
#endif
        }

        AllocateBackgroundCodeGenProfiler(pageAllocator);
    }

    virtual void ProcessorThreadSpecificCallBack(PageAllocator * pageAllocator) override
    {
        AllocateBackgroundAllocators(pageAllocator);
    }

    bool IsInDebugMode() const;
    static ExecutionMode PrejitJitMode(Js::FunctionBody *const functionBody);

    bool TryAggressiveInlining(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const functionBody, InliningDecider &inliningDecider, uint32& inlineeCount, uint recursiveInlineDepth);

private:
    Js::ScriptContext * scriptContext;
    Js::FunctionBody::SetNativeEntryPointFuncType SetNativeEntryPoint;
    uint pendingCodeGenWorkItems;
    JsUtil::DoublyLinkedList<CodeGenWorkItem> workItems;
    JsUtil::DoublyLinkedList<QueuedFullJitWorkItem> queuedFullJitWorkItems;
    uint queuedFullJitWorkItemCount;
    uint byteCodeSizeGenerated;

    bool isOptimizedForManyInstances;
    bool isClosed;
    bool hasUpdatedQForDebugMode;

    class FreeLoopBodyJob: public JsUtil::Job
    {
    public:
        FreeLoopBodyJob(JsUtil::JobManager *const manager, void* address, bool isHeapAllocated = true):
          JsUtil::Job(manager),
          codeAddress(address),
          heapAllocated(isHeapAllocated)
        {
        }

        bool heapAllocated;
        void* codeAddress;
    };

    class FreeLoopBodyJobManager sealed: public WaitableJobManager
    {
    public:
        FreeLoopBodyJobManager(JsUtil::JobProcessor* processor)
            : JsUtil::WaitableJobManager(processor)
            , autoClose(true)
            , isClosed(false)
        {
            Processor()->AddManager(this);
        }

        virtual ~FreeLoopBodyJobManager()
        {
            if (autoClose && !isClosed)
            {
                Close();
            }
            Assert(this->isClosed);
        }

        void Close()
        {
            Assert(!this->isClosed);
            Processor()->RemoveManager(this);
            this->isClosed = true;
        }

        void SetAutoClose(bool autoClose)
        {
            this->autoClose = autoClose;
        }

        FreeLoopBodyJob* GetJob(FreeLoopBodyJob* job)
        {
            return job;
        }

        bool WasAddedToJobProcessor(JsUtil::Job *const job) const
        {
            return true;
        }

        void SetNativeCodeGen(NativeCodeGenerator* nativeCodeGen)
        {
            this->nativeCodeGen = nativeCodeGen;
        }

        void BeforeWaitForJob(FreeLoopBodyJob*) const {}
        void AfterWaitForJob(FreeLoopBodyJob*) const {}

        virtual bool Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData) override
        {
            FreeLoopBodyJob* freeLoopBodyJob = static_cast<FreeLoopBodyJob*>(job);

            // Free Loop Body
            nativeCodeGen->FreeNativeCodeGenAllocation(freeLoopBodyJob->codeAddress);

            return true;
        }

        virtual void JobProcessed(JsUtil::Job *const job, const bool succeeded) override
        {
            FreeLoopBodyJob* freeLoopBodyJob = static_cast<FreeLoopBodyJob*>(job);

            if (freeLoopBodyJob->heapAllocated)
            {
                HeapDelete(freeLoopBodyJob);
            }
        }

        void QueueFreeLoopBodyJob(void* codeAddress);

    private:
        NativeCodeGenerator* nativeCodeGen;
        bool autoClose;
        bool isClosed;
    };

    FreeLoopBodyJobManager freeLoopBodyManager;

    CodeGenAllocators * foregroundAllocators;
    CodeGenAllocators * backgroundAllocators;
#ifdef PROFILE_EXEC
    Js::ScriptContextProfiler * foregroundCodeGenProfiler;
    Js::ScriptContextProfiler * backgroundCodeGenProfiler;
#endif

#if DBG
    ThreadContextId mainThreadId;
    friend void CheckIsExecutable(Js::RecyclableObject * function, Js::JavascriptMethod entrypoint);
#endif
};
