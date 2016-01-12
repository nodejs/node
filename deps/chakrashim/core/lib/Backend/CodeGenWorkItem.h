//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Func;

struct CodeGenWorkItem : public JsUtil::Job
{
protected:
    CodeGenWorkItem(
        JsUtil::JobManager *const manager,
        Js::FunctionBody *const functionBody,
        Js::EntryPointInfo* entryPointInfo,
        bool isJitInDebugMode,
        CodeGenWorkItemType type);
    ~CodeGenWorkItem();

    Js::FunctionBody *const functionBody;
    size_t codeAddress;
    ptrdiff_t codeSize;
    ushort pdataCount;
    ushort xdataSize;

    CodeGenWorkItemType type;
    ExecutionMode jitMode;

public:
    virtual uint GetByteCodeCount() const abstract;
    virtual uint GetFunctionNumber() const abstract;
    virtual size_t GetDisplayName(_Out_writes_opt_z_(sizeInChars) WCHAR* displayName, _In_ size_t sizeInChars) abstract;
    virtual void GetEntryPointAddress(void** entrypoint, ptrdiff_t *size) abstract;
    virtual uint GetInterpretedCount() const abstract;
    virtual void Delete() abstract;
#if DBG_DUMP | defined(VTUNE_PROFILING)
    virtual void RecordNativeMap(uint32 nativeOffset, uint32 statementIndex) abstract;
#endif
#if DBG_DUMP
    virtual void DumpNativeOffsetMaps() abstract;
    virtual void DumpNativeThrowSpanSequence() abstract;
#endif
    virtual void InitializeReader(Js::ByteCodeReader &reader, Js::StatementReader &statementReader) abstract;

    ExecutionMode GetJitMode()
    {
        return jitMode;
    }

    CodeGenWorkItemType Type() const { return type; }

    Js::ScriptContext* GetScriptContext()
    {
        return functionBody->GetScriptContext();
    }

    Js::FunctionBody* GetFunctionBody() const
    {
        return functionBody;
    }

    void SetCodeAddress(size_t codeAddress) { this->codeAddress = codeAddress; }
    size_t GetCodeAddress() { return codeAddress; }

    void SetCodeSize(ptrdiff_t codeSize) { this->codeSize = codeSize; }
    ptrdiff_t GetCodeSize() { return codeSize; }

    void SetPdataCount(ushort pdataCount) { this->pdataCount = pdataCount; }
    ushort GetPdataCount() { return pdataCount; }

    void SetXdataSize(ushort xdataSize) { this->xdataSize = xdataSize; }
    ushort GetXdataSize() { return xdataSize; }

protected:
    virtual uint GetLoopNumber() const
    {
        return Js::LoopHeader::NoLoop;
    }

protected:
    // This reference does not keep the entry point alive, and it's not expected to
    // The entry point is kept alive only if it's in the JIT queue, in which case recyclableData will be allocated and will keep the entry point alive
    // If the entry point is getting collected, it'll actually remove itself from the work item list so this work item will get deleted when the EntryPointInfo goes away
    Js::EntryPointInfo* entryPointInfo;
    Js::CodeGenRecyclableData *recyclableData;

private:
    bool isInJitQueue;                  // indicates if the work item has been added to the global jit queue
    bool isJitInDebugMode;              // Whether JIT is in debug mode for this work item.
    bool isAllocationCommitted;         // Whether the EmitBuffer allocation has been committed

    QueuedFullJitWorkItem *queuedFullJitWorkItem;
    EmitBufferAllocation *allocation;

#ifdef IR_VIEWER
public:
    bool isRejitIRViewerFunction;               // re-JIT function for IRViewer object generation
    Js::DynamicObject *irViewerOutput;          // hold results of IRViewer APIs
    Js::ScriptContext *irViewerRequestContext;  // keep track of the request context

    Js::DynamicObject * GetIRViewerOutput(Js::ScriptContext *scriptContext)
    {
        if (!irViewerOutput)
        {
            irViewerOutput = scriptContext->GetLibrary()->CreateObject();
        }

        return irViewerOutput;
    }

    void SetIRViewerOutput(Js::DynamicObject *output)
    {
        irViewerOutput = output;
    }
#endif
private:

    void SetAllocation(EmitBufferAllocation *allocation)
    {
        this->allocation = allocation;
    }
    EmitBufferAllocation *GetAllocation() { return allocation; }

public:
    Js::EntryPointInfo* GetEntryPoint() const
    {
        return this->entryPointInfo;
    }

    void RecordNativeCodeSize(Func *func, size_t bytes, ushort pdataCount, ushort xdataSize);
    void RecordNativeCode(Func *func, const BYTE* sourceBuffer);

    Js::CodeGenRecyclableData *RecyclableData() const
    {
        return recyclableData;
    }

    void SetRecyclableData(Js::CodeGenRecyclableData *const recyclableData)
    {
        Assert(recyclableData);
        Assert(!this->recyclableData);

        this->recyclableData = recyclableData;
    }

    void SetEntryPointInfo(Js::EntryPointInfo* entryPointInfo)
    {
        this->entryPointInfo = entryPointInfo;
    }

public:
    void ResetJitMode()
    {
        jitMode = ExecutionMode::Interpreter;
    }

    void SetJitMode(const ExecutionMode jitMode)
    {
        this->jitMode = jitMode;
        VerifyJitMode();
    }

    void VerifyJitMode() const
    {
        Assert(jitMode == ExecutionMode::SimpleJit || jitMode == ExecutionMode::FullJit);
        Assert(jitMode != ExecutionMode::SimpleJit || GetFunctionBody()->DoSimpleJit());
        Assert(jitMode != ExecutionMode::FullJit || GetFunctionBody()->DoFullJit());
    }

    void OnAddToJitQueue();
    void OnRemoveFromJitQueue(NativeCodeGenerator* generator);

public:
    bool ShouldSpeculativelyJit(uint byteCodeSizeGenerated) const;
private:
    bool ShouldSpeculativelyJitBasedOnProfile() const;

public:
    bool IsInJitQueue() const
    {
        return isInJitQueue;
    }

    bool IsJitInDebugMode() const
    {
        return isJitInDebugMode;
    }

#if _M_X64 || _M_ARM
    size_t RecordUnwindInfo(size_t offset, BYTE *unwindInfo, size_t size)
    {
#if _M_X64
        Assert(offset == 0);
        Assert(XDATA_SIZE >= size);
        js_memcpy_s(GetAllocation()->allocation->xdata.address, XDATA_SIZE, unwindInfo, size);
        return 0;
#else
        BYTE *xdataFinal = GetAllocation()->allocation->xdata.address + offset;

        Assert(xdataFinal);
        Assert(((DWORD)xdataFinal & 0x3) == 0); // 4 byte aligned
        js_memcpy_s(xdataFinal, size, unwindInfo, size);
        return (size_t)xdataFinal;
#endif
    }
#endif

#if _M_ARM
    void RecordPdataEntry(int index, DWORD beginAddress, DWORD unwindData)
    {
        RUNTIME_FUNCTION *function = GetAllocation()->allocation->xdata.GetPdataArray() + index;
        function->BeginAddress = beginAddress;
        function->UnwindData = unwindData;
    }

    void FinalizePdata()
    {
        GetAllocation()->allocation->RegisterPdata((ULONG_PTR)GetCodeAddress(), GetCodeSize());
    }
#endif

    void OnWorkItemProcessFail(NativeCodeGenerator *codeGen);

    void FinalizeNativeCode(Func *func);

    void RecordNativeThrowMap(Js::SmallSpanSequenceIter& iter, uint32 nativeOffset, uint32 statementIndex)
    {
        this->functionBody->RecordNativeThrowMap(iter, nativeOffset, statementIndex, this->GetEntryPoint(), GetLoopNumber());
    }

    QueuedFullJitWorkItem *GetQueuedFullJitWorkItem() const;
    QueuedFullJitWorkItem *EnsureQueuedFullJitWorkItem();

private:
    bool ShouldSpeculativelyJit() const;
};

struct JsFunctionCodeGen sealed : public CodeGenWorkItem
{
    JsFunctionCodeGen(
        JsUtil::JobManager *const manager,
        Js::FunctionBody *const functionBody,
        Js::EntryPointInfo* entryPointInfo,
        bool isJitInDebugMode)
        : CodeGenWorkItem(manager, functionBody, entryPointInfo, isJitInDebugMode, JsFunctionType)
    {
    }

public:
    uint GetByteCodeCount() const override
    {
        return functionBody->GetByteCodeCount() +  functionBody->GetConstantCount();
    }

    uint GetFunctionNumber() const override
    {
        return functionBody->GetFunctionNumber();
    }

    size_t GetDisplayName(_Out_writes_opt_z_(sizeInChars) WCHAR* displayName, _In_ size_t sizeInChars) override
    {
        const WCHAR* name = functionBody->GetExternalDisplayName();
        size_t nameSizeInChars = wcslen(name) + 1;
        size_t sizeInBytes = nameSizeInChars * sizeof(WCHAR);
        if(displayName == NULL || sizeInChars < nameSizeInChars)
        {
           return nameSizeInChars;
        }
        js_memcpy_s(displayName, sizeInChars * sizeof(WCHAR), name, sizeInBytes);
        return nameSizeInChars;
    }

    void GetEntryPointAddress(void** entrypoint, ptrdiff_t *size) override
    {
         Assert(entrypoint);
         *entrypoint = this->GetEntryPoint()->address;
         *size = this->GetEntryPoint()->GetCodeSize();
    }

    uint GetInterpretedCount() const override
    {
        return this->functionBody->interpretedCount;
    }

    void Delete() override
    {
        HeapDelete(this);
    }

#if DBG_DUMP | defined(VTUNE_PROFILING)
    void RecordNativeMap(uint32 nativeOffset, uint32 statementIndex) override
    {
        Js::FunctionEntryPointInfo* info = (Js::FunctionEntryPointInfo*) this->GetEntryPoint();

        info->RecordNativeMap(nativeOffset, statementIndex);
    }
#endif

#if DBG_DUMP
    virtual void DumpNativeOffsetMaps() override
    {
        this->GetEntryPoint()->DumpNativeOffsetMaps();
    }

    virtual void DumpNativeThrowSpanSequence() override
    {
        this->GetEntryPoint()->DumpNativeThrowSpanSequence();
    }
#endif

    virtual void InitializeReader(Js::ByteCodeReader &reader, Js::StatementReader &statementReader) override
    {
        reader.Create(this->functionBody, 0, IsJitInDebugMode());
        statementReader.Create(this->functionBody, 0, IsJitInDebugMode());
    }
};

struct JsLoopBodyCodeGen sealed : public CodeGenWorkItem
{
    JsLoopBodyCodeGen(
        JsUtil::JobManager *const manager,
        Js::FunctionBody *const functionBody,
        Js::EntryPointInfo* entryPointInfo,
        bool isJitInDebugMode)
        : CodeGenWorkItem(manager, functionBody, entryPointInfo, isJitInDebugMode, JsLoopBodyWorkItemType)
        , symIdToValueTypeMap(nullptr)
    {
    }

    Js::LoopHeader * loopHeader;
    typedef JsUtil::BaseDictionary<uint, ValueType, HeapAllocator> SymIdToValueTypeMap;
    SymIdToValueTypeMap *symIdToValueTypeMap;

    uint GetLoopNumber() const override
    {
        return functionBody->GetLoopNumber(loopHeader);
    }

    uint GetByteCodeCount() const override
    {
        return (loopHeader->endOffset - loopHeader->startOffset) + functionBody->GetConstantCount();
    }

    uint GetFunctionNumber() const override
    {
        return functionBody->GetFunctionNumber();
    }

    size_t GetDisplayName(_Out_writes_opt_z_(sizeInChars) WCHAR* displayName, _In_ size_t sizeInChars) override
    {
        return this->functionBody->GetLoopBodyName(this->GetLoopNumber(), displayName, sizeInChars);
    }

    void GetEntryPointAddress(void** entrypoint, ptrdiff_t *size) override
    {
        Assert(entrypoint);
        Js::EntryPointInfo * entryPoint = this->GetEntryPoint();
        *entrypoint = entryPoint->address;
        *size = entryPoint->GetCodeSize();
    }

    uint GetInterpretedCount() const override
    {
        return loopHeader->interpretCount;
    }

#if DBG_DUMP | defined(VTUNE_PROFILING)
    void RecordNativeMap(uint32 nativeOffset, uint32 statementIndex) override
    {
        this->GetEntryPoint()->RecordNativeMap(nativeOffset, statementIndex);
    }
#endif

#if DBG_DUMP
    virtual void DumpNativeOffsetMaps() override
    {
        this->GetEntryPoint()->DumpNativeOffsetMaps();
    }

    virtual void DumpNativeThrowSpanSequence() override
    {
        this->GetEntryPoint()->DumpNativeThrowSpanSequence();
    }
#endif

    void Delete() override
    {
        HeapDelete(this);
    }

    virtual void InitializeReader(Js::ByteCodeReader &reader, Js::StatementReader &statementReader) override
    {
        reader.Create(this->functionBody, this->loopHeader->startOffset, IsJitInDebugMode());
        statementReader.Create(this->functionBody, this->loopHeader->startOffset, IsJitInDebugMode());
    }

    ~JsLoopBodyCodeGen()
    {
        if (this->symIdToValueTypeMap)
        {
            HeapDelete(this->symIdToValueTypeMap);
            this->symIdToValueTypeMap = NULL;
        }
    }
};
