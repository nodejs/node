//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef ENABLE_NATIVE_CODEGEN
class ThunkBlock
{
private:
#if PDATA_ENABLED
    void* registeredPdataTable;
#endif
    BYTE*    start;
    BVFixed* freeList;

public:
    ThunkBlock(BYTE* start) :
        start(start),
        freeList(NULL)
#if PDATA_ENABLED
        , registeredPdataTable(NULL)
#endif
    {}

    bool Contains(BYTE* address) const;
    void Release(BYTE* address);
    bool EnsureFreeList(ArenaAllocator* allocator);
    BYTE* AllocateFromFreeList();
    bool IsFreeListEmpty() const;
    BYTE* GetStart() const { return start; }

#if PDATA_ENABLED
    void* GetPdata() const { return registeredPdataTable; }
    void SetPdata(void* pdata) { Assert(!this->registeredPdataTable); this->registeredPdataTable = pdata; }
#endif

private:
    BVIndex FromThunkAddress(BYTE* address);
    BYTE* ToThunkAddress(BVIndex index);
};

//
// Emits interpreter thunks such that each javascript function in the interpreter gets a unique return address on call.
// This unique address can be used to identify the function when a stackwalk is
// performed using ETW.
//
// On every call, we generate 1 thunk (size ~ 1 page) that is implemented as a jump table.
// The shape of the thunk is as follows:
//         1. Function prolog
//         2. Determine the thunk number from the JavascriptFunction object passed as first argument.
//         3. Calculate the address of the correct calling point and jump to it.
//         4. Make the call and jump to the epilog.
//         5. Function epilog.
//
class InterpreterThunkEmitter
{
private:
    /* ------- instance methods --------*/
    EmitBufferManager<> emitBufferManager;
    EmitBufferAllocation *allocation;
    SListBase<ThunkBlock> thunkBlocks;
    SListBase<ThunkBlock> freeListedThunkBlocks;
    void * interpreterThunk; // the static interpreter thunk invoked by the dynamic emitted thunk
    BYTE*                thunkBuffer;
    ArenaAllocator*      allocator;
    DWORD thunkCount;                      // Count of thunks available in the current thunk block

    /* -------static constants ----------*/
    // Interpreter thunk buffer includes function prolog, setting up of arguments, jumping to the appropriate calling point.
    static const BYTE ThunkAddressOffset;
    static const BYTE FunctionBodyOffset;
    static const BYTE DynamicThunkAddressOffset;
    static const BYTE InterpreterThunkEmitter::CallBlockStartAddrOffset;
    static const BYTE InterpreterThunkEmitter::ThunkSizeOffset;
    static const BYTE InterpreterThunkEmitter::ErrorOffset;
#if defined(_M_ARM)
    static const BYTE InterpreterThunkEmitter::CallBlockStartAddressInstrOffset;
    static const BYTE InterpreterThunkEmitter::CallThunkSizeInstrOffset;
#endif
    static const BYTE InterpreterThunk[];

    // Call buffer includes a call to the inner interpreter thunk and eventual jump to the epilog
    static const BYTE JmpOffset;
    static const BYTE Call[];

    static const BYTE Epilog[];

    static const BYTE PageCount;
#if defined(_M_X64)
    static const BYTE PrologSize;
    static const BYTE StackAllocSize;
#endif

    /* ------private helpers -----------*/
    void NewThunkBlock();
    void EncodeInterpreterThunk(__in_bcount(thunkSize) BYTE* thunkBuffer, __in_bcount(thunkSize) BYTE* thunkBufferStartAddress, __in const DWORD thunkSize, __in_bcount(epilogSize) BYTE* epilogStart, __in const DWORD epilogSize);
#if defined(_M_ARM32_OR_ARM64)
    DWORD EncodeMove(DWORD opCode, int reg, DWORD imm16);
    void GeneratePdata(_In_ const BYTE* entryPoint, _In_ const DWORD functionSize, _Out_ RUNTIME_FUNCTION* function);
#endif

    /*-------static helpers ---------*/
    inline static DWORD FillDebugBreak(__out_bcount_full(count) BYTE* dest, __in DWORD count);
    inline static DWORD CopyWithAlignment(__in_bcount(sizeInBytes) BYTE* dest, __in const DWORD sizeInBytes, __in_bcount(srcSize) const BYTE* src, __in_range(0, sizeInBytes) const DWORD srcSize, __in const DWORD alignment);
    template<class T>
    inline static void Emit(__in_bcount(sizeof(T) + offset) BYTE* dest, __in const DWORD offset, __in const T value)
    {
        AssertMsg(*(T*) (dest + offset) == 0, "Overwriting an already existing opcode?");
        *(T*)(dest + offset) = value;
    };
public:
    static const BYTE HeaderSize;
    static const BYTE ThunkSize;
    static const uint ThunksPerBlock;
    static const uint BlockSize;
    static void* ConvertToEntryPoint(PVOID dynamicInterpreterThunk);

    InterpreterThunkEmitter(AllocationPolicyManager * policyManager, ArenaAllocator* allocator, void * interpreterThunk);

    BYTE* GetNextThunk(PVOID* ppDynamicInterpreterThunk);

    BYTE* AllocateFromFreeList(PVOID* ppDynamicInterpreterThunk);

    void Close();
    void Release(BYTE* thunkAddress, bool addtoFreeList);
    // Returns true if the argument falls within the range managed by this buffer.
    inline bool IsInRange(void* address)
    {
        return emitBufferManager.IsInRange(address);
    }
    const EmitBufferManager<>* GetEmitBufferManager() const
    {
        return &emitBufferManager;
    }

};
#endif
