//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

#ifdef ENABLE_NATIVE_CODEGEN
#ifdef _M_X64
const BYTE InterpreterThunkEmitter::FunctionBodyOffset = 23;
const BYTE InterpreterThunkEmitter::DynamicThunkAddressOffset = 27;
const BYTE InterpreterThunkEmitter::CallBlockStartAddrOffset = 37;
const BYTE InterpreterThunkEmitter::ThunkSizeOffset = 51;
const BYTE InterpreterThunkEmitter::ErrorOffset = 60;
const BYTE InterpreterThunkEmitter::ThunkAddressOffset = 77;

const BYTE InterpreterThunkEmitter::PrologSize = 76;
const BYTE InterpreterThunkEmitter::StackAllocSize = 0x28;

//
// Home the arguments onto the stack and pass a pointer to the base of the stack location to the inner thunk
//
// Calling convention requires that caller should allocate at least 0x20 bytes and the stack be 16 byte aligned.
// Hence, we allocate 0x28 bytes of stack space for the callee to use. The callee uses 8 bytes to push the first
// argument and the rest 0x20 ensures alignment is correct.
//
const BYTE InterpreterThunkEmitter::InterpreterThunk[] = {
    0x48, 0x89, 0x54, 0x24, 0x10,                                  // mov         qword ptr [rsp+10h],rdx
    0x48, 0x89, 0x4C, 0x24, 0x08,                                  // mov         qword ptr [rsp+8],rcx
    0x4C, 0x89, 0x44, 0x24, 0x18,                                  // mov         qword ptr [rsp+18h],r8
    0x4C, 0x89, 0x4C, 0x24, 0x20,                                  // mov         qword ptr [rsp+20h],r9
    0x48, 0x8B, 0x41, 0x00,                                        // mov         rax, qword ptr [rcx+FunctionBodyOffset]
    0x48, 0x8B, 0x50, 0x00,                                        // mov         rdx, qword ptr [rax+DynamicThunkAddressOffset]
                                                                   // Range Check for Valid call target
    0x48, 0x83, 0xE2, 0xF8,                                        // and         rdx, 0xFFFFFFFFFFFFFFF8h  ;Force 8 byte alignment
    0x48, 0x8b, 0xca,                                              // mov         rcx, rdx
    0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    // mov         rax, CallBlockStartAddress
    0x48, 0x2b, 0xc8,                                              // sub         rcx, rax
    0x48, 0x81, 0xf9, 0x00, 0x00, 0x00, 0x00,                      // cmp         rcx, ThunkSize
    0x76, 0x09,                                                    // jbe         $safe
    0x48, 0xc7, 0xc1, 0x00, 0x00, 0x00, 0x00,                      // mov         rcx, errorcode
    0xcd, 0x29,                                                    // int         29h

    // $safe:
    0x48, 0x8D, 0x4C, 0x24, 0x08,                                  // lea         rcx, [rsp+8]                ;Load the address to stack
    0x48, 0x83, 0xEC, StackAllocSize,                              // sub         rsp,28h
    0x48, 0xB8, 0x00, 0x00, 0x00 ,0x00, 0x00, 0x00, 0x00, 0x00,    // mov         rax, <thunk>
    0xFF, 0xE2,                                                    // jmp         rdx
    0xCC                                                           // int         3                           ;for alignment to size of 8 we are adding this
};

const BYTE InterpreterThunkEmitter::Epilog[] = {
    0x48, 0x83, 0xC4, StackAllocSize,                              // add         rsp,28h
    0xC3                                                           // ret
};
#elif defined(_M_ARM)
const BYTE InterpreterThunkEmitter::ThunkAddressOffset = 8;
const BYTE InterpreterThunkEmitter::FunctionBodyOffset = 18;
const BYTE InterpreterThunkEmitter::DynamicThunkAddressOffset = 22;
const BYTE InterpreterThunkEmitter::CallBlockStartAddressInstrOffset = 38;
const BYTE InterpreterThunkEmitter::CallThunkSizeInstrOffset = 50;
const BYTE InterpreterThunkEmitter::ErrorOffset = 60;

const BYTE InterpreterThunkEmitter::InterpreterThunk[] = {
    0x0F, 0xB4,                                                      // push        {r0-r3}
    0x2D, 0xE9, 0x00, 0x48,                                          // push        {r11,lr}
    0xEB, 0x46,                                                      // mov         r11,sp
    0x00, 0x00, 0x00, 0x00,                                          // movw        r1,ThunkAddress
    0x00, 0x00, 0x00, 0x00,                                          // movt        r1,ThunkAddress
    0xD0, 0xF8, 0x00, 0x20,                                          // ldr.w       r2,[r0,#0x00]
    0xD2, 0xF8, 0x00, 0x30,                                          // ldr.w       r3,[r2,#0x00]
    0x4F, 0xF6, 0xF9, 0x70,                                          // mov         r0,#0xFFF9
    0xCF, 0xF6, 0xFF, 0x70,                                          // movt        r0,#0xFFFF
    0x03, 0xEA, 0x00, 0x03,                                          // and         r3,r3,r0
    0x18, 0x46,                                                      // mov         r0, r3
    0x00, 0x00, 0x00, 0x00,                                          // movw        r12, CallBlockStartAddress
    0x00, 0x00, 0x00, 0x00,                                          // movt        r12, CallBlockStartAddress
    0xA0, 0xEB, 0x0C, 0x00,                                          // sub         r0, r12
    0x00, 0x00, 0x00, 0x00,                                          // mov         r12, ThunkSize
    0x60, 0x45,                                                      // cmp         r0, r12
    0x02, 0xD9,                                                      // bls         $safe
    0x4F, 0xF0, 0x00, 0x00,                                          // mov         r0, errorcode
    0xFB, 0xDE,                                                      // Equivalent to int 0x29

    //$safe:
    0x02, 0xA8,                                                      // add         r0,sp,#8
    0x18, 0x47,                                                      // bx          r3
    0xFE, 0xDE,                                                      // int         3       ;Required for alignment
    0xFE, 0xDE                                                       // int         3       ;Required for alignment
};

const BYTE InterpreterThunkEmitter::JmpOffset = 2;

const BYTE InterpreterThunkEmitter::Call[] = {
    0x88, 0x47,                                                      // blx         r1
    0x00, 0x00, 0x00, 0x00,                                          // b.w         epilog
    0xFE, 0xDE,                                                      // int         3       ;Required for alignment
};

const BYTE InterpreterThunkEmitter::Epilog[] = {
    0x5D, 0xF8, 0x04, 0xBB,                                         // pop         {r11}
    0x5D, 0xF8, 0x14, 0xFB                                          // ldr         pc,[sp],#0x14
};
#elif defined(_M_ARM64)
const BYTE InterpreterThunkEmitter::FunctionBodyOffset = 24;
const BYTE InterpreterThunkEmitter::DynamicThunkAddressOffset = 28;
const BYTE InterpreterThunkEmitter::ThunkAddressOffset = 32;

//TODO: saravind :Implement Range Check for ARM64
const BYTE InterpreterThunkEmitter::InterpreterThunk[] = {
    0xFD, 0x7B, 0xBB, 0xA9,                                         //stp         fp, lr, [sp, #-80]!   ;Prologue
    0xFD, 0x03, 0x00, 0x91,                                         //mov         fp, sp                ;update frame pointer to the stack pointer
    0xE0, 0x07, 0x01, 0xA9,                                         //stp         x0, x1, [sp, #16]     ;Prologue again; save all registers
    0xE2, 0x0F, 0x02, 0xA9,                                         //stp         x2, x3, [sp, #32]
    0xE4, 0x17, 0x03, 0xA9,                                         //stp         x4, x5, [sp, #48]
    0xE6, 0x1F, 0x04, 0xA9,                                         //stp         x6, x7, [sp, #64]
    0x02, 0x00, 0x40, 0xF9,                                         //ldr         x2, [x0, #0x00]       ;offset will be replaced with Offset of FunctionInfo
    0x43, 0x00, 0x40, 0xF9,                                         //ldr         x3, [x2, #0x00]       ;offset will be replaced with offset of DynamicInterpreterThunk
                                                                    //Following 4 MOV Instrs are to move the 64-bit address of the InterpreterThunk address into register x1.
    0x00, 0x00, 0x00, 0x00,                                         //movz        x1, #0x00             ;This is overwritten with the actual thunk address(16 - 0 bits) move
    0x00, 0x00, 0x00, 0x00,                                         //movk        x1, #0x00, lsl #16    ;This is overwritten with the actual thunk address(32 - 16 bits) move
    0x00, 0x00, 0x00, 0x00,                                         //movk        x1, #0x00, lsl #32    ;This is overwritten with the actual thunk address(48 - 32 bits) move
    0x00, 0x00, 0x00, 0x00,                                         //movk        x1, #0x00, lsl #48    ;This is overwritten with the actual thunk address(64 - 48 bits) move
    0xE0, 0x43, 0x00, 0x91,                                         //add         x0, sp, #16
    0x60, 0x00, 0x1F, 0xD6                                          //br          x3
};

const BYTE InterpreterThunkEmitter::JmpOffset = 4;

const BYTE InterpreterThunkEmitter::Call[] = {
    0x20, 0x00, 0x3f, 0xd6,                                         // blr         x1
    0x00, 0x00, 0x00, 0x00                                          // b           epilog
};

const BYTE InterpreterThunkEmitter::Epilog[] = {
    0xfd, 0x7b, 0xc5, 0xa8,                                         // ldp         fp, lr, [sp], #80
    0xc0, 0x03, 0x5f, 0xd6                                          // ret
};
#else
const BYTE InterpreterThunkEmitter::FunctionBodyOffset = 8;
const BYTE InterpreterThunkEmitter::DynamicThunkAddressOffset = 11;
const BYTE InterpreterThunkEmitter::CallBlockStartAddrOffset = 18;
const BYTE InterpreterThunkEmitter::ThunkSizeOffset = 23;
const BYTE InterpreterThunkEmitter::ErrorOffset = 30;
const BYTE InterpreterThunkEmitter::ThunkAddressOffset = 41;

const BYTE InterpreterThunkEmitter::InterpreterThunk[] = {
    0x55,                                                           //   push        ebp                ;Prolog - setup the stack frame
    0x8B, 0xEC,                                                     //   mov         ebp,esp
    0x8B, 0x45, 0x08,                                               //   mov         eax, dword ptr [ebp+8]
    0x8B, 0x40, 0x00,                                               //   mov         eax, dword ptr [eax+FunctionBodyOffset]
    0x8B, 0x48, 0x00,                                               //   mov         ecx, dword ptr [eax+DynamicThunkAddressOffset]
                                                                    //   Range Check for Valid call target
    0x83, 0xE1, 0xF8,                                               //   and         ecx, 0FFFFFFF8h
    0x8b, 0xc1,                                                     //   mov         eax, ecx
    0x2d, 0x00, 0x00, 0x00, 0x00,                                   //   sub         eax, CallBlockStartAddress
    0x3d, 0x00, 0x00, 0x00, 0x00,                                   //   cmp         eax, ThunkSize
    0x76, 0x07,                                                     //   jbe         SHORT $safe
    0xb9, 0x00, 0x00, 0x00, 0x00,                                   //   mov         ecx, errorcode
    0xCD, 0x29,                                                     //   int         29h

    //$safe
    0x8D, 0x45, 0x08,                                               //   lea         eax, ebp+8
    0x50,                                                           //   push        eax
    0xB8, 0x00, 0x00, 0x00, 0x00,                                   //   mov         eax, <thunk>
    0xFF, 0xE1,                                                     //   jmp         ecx
    0xCC                                                            //   int 3 for 8byte alignment
};

const BYTE InterpreterThunkEmitter::Epilog[] = {
    0x5D,                                                         // pop ebp
    0xC3                                                          // ret
};
#endif

#if defined(_M_X64) || defined(_M_IX86)
const BYTE InterpreterThunkEmitter::JmpOffset = 3;

const BYTE InterpreterThunkEmitter::Call[] = {
    0xFF, 0xD0,                                                // call       rax
    0xE9, 0x00, 0x00, 0x00, 0x00,                              // jmp        [offset]
    0xCC,                                                      // int 3      ;for alignment to size of 8 we are adding this
};

#endif

const BYTE InterpreterThunkEmitter::PageCount = 1;
const uint InterpreterThunkEmitter::BlockSize = AutoSystemInfo::PageSize * InterpreterThunkEmitter::PageCount;
const BYTE InterpreterThunkEmitter::HeaderSize = sizeof(InterpreterThunk);
const BYTE InterpreterThunkEmitter::ThunkSize = sizeof(Call);
const uint InterpreterThunkEmitter::ThunksPerBlock = (BlockSize - HeaderSize) / ThunkSize;

InterpreterThunkEmitter::InterpreterThunkEmitter(AllocationPolicyManager * policyManager, ArenaAllocator* allocator, void * interpreterThunk) :
    emitBufferManager(policyManager, allocator, /*scriptContext*/ nullptr, L"Interpreter thunk buffer", /*allocXdata*/ false),
    allocation(nullptr),
    allocator(allocator),
    thunkCount(0),
    thunkBuffer(nullptr),
    interpreterThunk(interpreterThunk)
{
}

//
// Returns the next thunk. Batch allocated PageCount pages of thunks and issue them one at a time
//
BYTE* InterpreterThunkEmitter::GetNextThunk(PVOID* ppDynamicInterpreterThunk)
{
    Assert(ppDynamicInterpreterThunk);
    Assert(*ppDynamicInterpreterThunk == nullptr);

    if(thunkCount == 0)
    {
        if(!this->freeListedThunkBlocks.Empty())
        {
            return AllocateFromFreeList(ppDynamicInterpreterThunk);
        }
        NewThunkBlock();
    }

    Assert(this->thunkBuffer != nullptr);
    BYTE* thunk = this->thunkBuffer;
#if _M_ARM
    thunk = (BYTE*)((DWORD)thunk | 0x01);
#endif
    *ppDynamicInterpreterThunk = thunk + HeaderSize + ((--thunkCount) * ThunkSize);
#if _M_ARM
    AssertMsg(((uintptr_t)(*ppDynamicInterpreterThunk) & 0x6) == 0, "Not 8 byte aligned?");
#else
    AssertMsg(((uintptr_t)(*ppDynamicInterpreterThunk) & 0x7) == 0, "Not 8 byte aligned?");
#endif
    return thunk;
}

//
// Interpreter thunks have an entrypoint at the beginning of the page boundary. Each function has a unique thunk return address
// and this function can convert to the unique thunk return address to the beginning of the page which corresponds with the entrypoint
//
void* InterpreterThunkEmitter::ConvertToEntryPoint(PVOID dynamicInterpreterThunk)
{
    Assert(dynamicInterpreterThunk != nullptr);
    void* entryPoint = (void*)((size_t)dynamicInterpreterThunk & (~((size_t)(BlockSize) - 1)));

#if _M_ARM
    entryPoint = (BYTE*)((DWORD)entryPoint | 0x01);
#endif
    return entryPoint;
}

void InterpreterThunkEmitter::NewThunkBlock()
{
    Assert(this->thunkCount == 0);
    BYTE* buffer;
    BYTE* currentBuffer;
    DWORD bufferSize = BlockSize;
    DWORD thunkCount = 0;

    allocation = emitBufferManager.AllocateBuffer(bufferSize, &buffer, /*readWrite*/ true);
    currentBuffer = buffer;

#ifdef _M_X64
    PrologEncoder prologEncoder(allocator);
    prologEncoder.EncodeSmallProlog(PrologSize, StackAllocSize);
    DWORD pdataSize = prologEncoder.SizeOfPData();
#elif defined(_M_ARM32_OR_ARM64)
    DWORD pdataSize = sizeof(RUNTIME_FUNCTION);
#else
    DWORD pdataSize = 0;
#endif
    DWORD bytesRemaining = bufferSize;
    DWORD bytesWritten = 0;
    DWORD epilogSize = sizeof(Epilog);

    // Ensure there is space for PDATA at the end
    BYTE* pdataStart = currentBuffer + (bufferSize - Math::Align(pdataSize, EMIT_BUFFER_ALIGNMENT));
    BYTE* epilogStart = pdataStart - Math::Align(epilogSize, EMIT_BUFFER_ALIGNMENT);

    // Copy the thunk buffer and modify it.
    js_memcpy_s(currentBuffer, bytesRemaining, InterpreterThunk, HeaderSize);
    EncodeInterpreterThunk(currentBuffer, buffer, HeaderSize, epilogStart, epilogSize);
    currentBuffer += HeaderSize;
    bytesRemaining -= HeaderSize;

    // Copy call buffer
    DWORD callSize = sizeof(Call);
    while(currentBuffer < epilogStart - callSize)
    {
        js_memcpy_s(currentBuffer, bytesRemaining, Call, callSize);
#if _M_ARM
        int offset = (epilogStart - (currentBuffer + JmpOffset));
        Assert(offset >= 0);
        DWORD encodedOffset = EncoderMD::BranchOffset_T2_24(offset);
        DWORD encodedBranch = /*opcode=*/ 0x9000F000 | encodedOffset;
        Emit(currentBuffer, JmpOffset, encodedBranch);
#elif _M_ARM64
        int64 offset = (epilogStart - (currentBuffer + JmpOffset));
        Assert(offset >= 0);
        DWORD encodedOffset = EncoderMD::BranchOffset_26(offset);
        DWORD encodedBranch = /*opcode=*/ 0x14000000 | encodedOffset;
        Emit(currentBuffer, JmpOffset, encodedBranch);
#else
        // jump requires an offset from the end of the jump instruction.
        int offset = (int)(epilogStart - (currentBuffer + JmpOffset + sizeof(int)));
        Assert(offset >= 0);
        Emit(currentBuffer, JmpOffset, offset);
#endif
        currentBuffer += callSize;
        bytesRemaining -= callSize;
        thunkCount++;
    }

    // Fill any gap till start of epilog
    bytesWritten = FillDebugBreak(currentBuffer, (DWORD)(epilogStart - currentBuffer));
    bytesRemaining -= bytesWritten;
    currentBuffer += bytesWritten;

    // Copy epilog
    bytesWritten = CopyWithAlignment(currentBuffer, bytesRemaining, Epilog, epilogSize, EMIT_BUFFER_ALIGNMENT);
    currentBuffer += bytesWritten;
    bytesRemaining -= bytesWritten;

    // Generate and register PDATA
#if PDATA_ENABLED
    BYTE* epilogEnd = epilogStart + epilogSize;
    DWORD functionSize = (DWORD)(epilogEnd - buffer);
    Assert(pdataStart == currentBuffer);
#ifdef _M_X64
    Assert(bytesRemaining >= pdataSize);
    BYTE* pdata = prologEncoder.Finalize(buffer, functionSize, pdataStart);
    bytesWritten = CopyWithAlignment(pdataStart, bytesRemaining, pdata, pdataSize, EMIT_BUFFER_ALIGNMENT);
#elif defined(_M_ARM32_OR_ARM64)
    RUNTIME_FUNCTION pdata;
    GeneratePdata(buffer, functionSize, &pdata);
    bytesWritten = CopyWithAlignment(pdataStart, bytesRemaining, (const BYTE*)&pdata, pdataSize, EMIT_BUFFER_ALIGNMENT);
#endif
    void* pdataTable;
    PDataManager::RegisterPdata((PRUNTIME_FUNCTION) pdataStart, (ULONG_PTR) buffer, (ULONG_PTR) epilogEnd, &pdataTable);
#endif
    if (!emitBufferManager.CommitReadWriteBufferForInterpreter(allocation, buffer, bufferSize))
    {
        Js::Throw::OutOfMemory();
    }

    // Call to set VALID flag for CFG check
    ThreadContext::GetContextForCurrentThread()->SetValidCallTargetForCFG(buffer);

    // Update object state only at the end when everything has succeeded - and no exceptions can be thrown.
    ThunkBlock* block = this->thunkBlocks.PrependNode(allocator, buffer);
    UNREFERENCED_PARAMETER(block);
#if PDATA_ENABLED
    block->SetPdata(pdataTable);
#endif
    this->thunkCount = thunkCount;
    this->thunkBuffer = buffer;
}


#if _M_ARM
void InterpreterThunkEmitter::EncodeInterpreterThunk(__in_bcount(thunkSize) BYTE* thunkBuffer, __in_bcount(thunkSize) BYTE* thunkBufferStartAddress, __in const DWORD thunkSize, __in_bcount(epilogSize) BYTE* epilogStart, __in const DWORD epilogSize)
{
    _Analysis_assume_(thunkSize == HeaderSize);
    // Encode MOVW
    DWORD lowerThunkBits = (uint32)this->interpreterThunk & 0x0000FFFF;
    DWORD movW = EncodeMove(/*Opcode*/ 0x0000F240, /*register*/1, lowerThunkBits);
    Emit(thunkBuffer,ThunkAddressOffset, movW);

    // Encode MOVT
    DWORD higherThunkBits = ((uint32)this->interpreterThunk & 0xFFFF0000) >> 16;
    DWORD movT = EncodeMove(/*Opcode*/ 0x0000F2C0, /*register*/1, higherThunkBits);
    Emit(thunkBuffer, ThunkAddressOffset + sizeof(movW), movT);

    // Encode LDR - Load of function Body
    thunkBuffer[FunctionBodyOffset] = Js::JavascriptFunction::GetOffsetOfFunctionInfo();

    // Encode LDR - Load of interpreter thunk number
    thunkBuffer[DynamicThunkAddressOffset] = Js::FunctionBody::GetOffsetOfDynamicInterpreterThunk();

    // Encode MOVW R12, CallBlockStartAddress
    uintptr_t callBlockStartAddress = (uintptr_t)thunkBufferStartAddress + HeaderSize;
    uint totalThunkSize = (uint)(epilogStart - callBlockStartAddress);

    DWORD lowerCallBlockStartAddress = callBlockStartAddress & 0x0000FFFF;
    DWORD movWblockStart = EncodeMove(/*Opcode*/ 0x0000F240, /*register*/12, lowerCallBlockStartAddress);
    Emit(thunkBuffer,CallBlockStartAddressInstrOffset, movWblockStart);

    // Encode MOVT R12, CallBlockStartAddress
    DWORD higherCallBlockStartAddress = (callBlockStartAddress & 0xFFFF0000) >> 16;
    DWORD movTblockStart = EncodeMove(/*Opcode*/ 0x0000F2C0, /*register*/12, higherCallBlockStartAddress);
    Emit(thunkBuffer, CallBlockStartAddressInstrOffset + sizeof(movWblockStart), movTblockStart);

    //Encode MOV R12, CallBlockSize
    DWORD movBlockSize = EncodeMove(/*Opcode*/ 0x0000F240, /*register*/12, (DWORD)totalThunkSize);
    Emit(thunkBuffer, CallThunkSizeInstrOffset, movBlockSize);

    Emit(thunkBuffer, ErrorOffset, (BYTE) FAST_FAIL_INVALID_ARG);
}

DWORD InterpreterThunkEmitter::EncodeMove(DWORD opCode, int reg, DWORD imm16)
{
    DWORD encodedMove = reg << 24;
    DWORD encodedImm = 0;
    EncoderMD::EncodeImmediate16(imm16, &encodedImm);
    encodedMove |= encodedImm;
    AssertMsg((encodedMove & opCode) == 0, "Any bits getting overwritten?");
    encodedMove |= opCode;
    return encodedMove;
}

void InterpreterThunkEmitter::GeneratePdata(_In_ const BYTE* entryPoint, _In_ const DWORD functionSize, _Out_ RUNTIME_FUNCTION* function)
{
    function->BeginAddress   = 0x1;                        // Since our base address is the start of the function - this is offset from the base address
    function->Flag           = 1;                          // Packed unwind data is used
    function->FunctionLength = functionSize / 2;
    function->Ret            = 0;                          // Return via Pop
    function->H              = 1;                          // Homes parameters
    function->Reg            = 7;                          // No saved registers - R11 is the frame pointer - not considered here
    function->R              = 1;                          // No registers are being saved.
    function->L              = 1;                          // Save/restore LR register
    function->C              = 1;                          // Frame pointer chain in R11 established
    function->StackAdjust    = 0;                          // Stack allocation for the function
}

#elif _M_ARM64
void InterpreterThunkEmitter::EncodeInterpreterThunk(__in_bcount(thunkSize) BYTE* thunkBuffer, __in_bcount(thunkSize) BYTE* thunkBufferStartAddress, __in const DWORD thunkSize, __in_bcount(epilogSize) BYTE* epilogStart, __in const DWORD epilogSize)
{
    int addrOffset = ThunkAddressOffset;

    _Analysis_assume_(thunkSize == HeaderSize);
    AssertMsg(thunkSize == HeaderSize, "Mismatch in the size of the InterpreterHeaderThunk and the thunkSize used in this API (EncodeInterpreterThunk)");

    // Following 4 MOV Instrs are to move the 64-bit address of the InterpreterThunk address into register x1.

    // Encode MOVZ (movz        x1, #<interpreterThunk 16-0 bits>)
    DWORD lowerThunkBits = (uint64)this->interpreterThunk & 0x0000FFFF;
    DWORD movZ = EncodeMove(/*Opcode*/ 0xD2800000, /*register x1*/1, lowerThunkBits); // no shift; hw = 00
    Emit(thunkBuffer,addrOffset, movZ);
    AssertMsg(sizeof(movZ) == 4, "movZ has to be 32-bit encoded");
    addrOffset+= sizeof(movZ);

    // Encode MOVK (movk        x1, #<interpreterThunk 32-16 bits>, lsl #16)
    DWORD higherThunkBits = ((uint64)this->interpreterThunk & 0xFFFF0000) >> 16;
    DWORD movK = EncodeMove(/*Opcode*/ 0xF2A00000, /*register x1*/1, higherThunkBits); // left shift 16 bits; hw = 01
    Emit(thunkBuffer, addrOffset, movK);
    AssertMsg(sizeof(movK) == 4, "movK has to be 32-bit encoded");
    addrOffset+= sizeof(movK);

    // Encode MOVK (movk        x1, #<interpreterThunk 48-32 bits>, lsl #16)
    higherThunkBits = ((uint64)this->interpreterThunk & 0xFFFF00000000) >> 32;
    movK = EncodeMove(/*Opcode*/ 0xF2C00000, /*register x1*/1, higherThunkBits); // left shift 32 bits; hw = 02
    Emit(thunkBuffer, addrOffset, movK);
    AssertMsg(sizeof(movK) == 4, "movK has to be 32-bit encoded");
    addrOffset += sizeof(movK);

    // Encode MOVK (movk        x1, #<interpreterThunk 64-48 bits>, lsl #16)
    higherThunkBits = ((uint64)this->interpreterThunk & 0xFFFF000000000000) >> 48;
    movK = EncodeMove(/*Opcode*/ 0xF2E00000, /*register x1*/1, higherThunkBits); // left shift 48 bits; hw = 03
    AssertMsg(sizeof(movK) == 4, "movK has to be 32-bit encoded");
    Emit(thunkBuffer, addrOffset, movK);

    // Encode LDR - Load of function Body
    ULONG offsetOfFunctionInfo = Js::JavascriptFunction::GetOffsetOfFunctionInfo();
    AssertMsg(offsetOfFunctionInfo % 8 == 0, "Immediate offset for LDR must be 8 byte aligned");
    AssertMsg(offsetOfFunctionInfo < 0x8000, "Immediate offset for LDR must be less than 0x8000");
    *(PULONG)&thunkBuffer[FunctionBodyOffset] |= (offsetOfFunctionInfo / 8) << 10;

    // Encode LDR - Load of interpreter thunk number
    ULONG offsetOfDynamicInterpreterThunk = Js::FunctionBody::GetOffsetOfDynamicInterpreterThunk();
    AssertMsg(offsetOfDynamicInterpreterThunk % 8 == 0, "Immediate offset for LDR must be 8 byte aligned");
    AssertMsg(offsetOfDynamicInterpreterThunk < 0x8000, "Immediate offset for LDR must be less than 0x8000");
    *(PULONG)&thunkBuffer[DynamicThunkAddressOffset] |= (offsetOfDynamicInterpreterThunk / 8) << 10;
}

DWORD InterpreterThunkEmitter::EncodeMove(DWORD opCode, int reg, DWORD imm16)
{
    DWORD encodedMove = reg << 0;
    DWORD encodedImm = 0;
    EncoderMD::EncodeImmediate16(imm16, &encodedImm);
    encodedMove |= encodedImm;
    AssertMsg((encodedMove & opCode) == 0, "Any bits getting overwritten?");
    encodedMove |= opCode;
    return encodedMove;
}

void InterpreterThunkEmitter::GeneratePdata(_In_ const BYTE* entryPoint, _In_ const DWORD functionSize, _Out_ RUNTIME_FUNCTION* function)
{
    function->BeginAddress = 0x0;               // Since our base address is the start of the function - this is offset from the base address
    function->Flag = 1;                         // Packed unwind data is used
    function->FunctionLength = functionSize / 4;
    function->RegF = 0;                         // number of non-volatile FP registers (d8-d15) saved in the canonical stack location
    function->RegI = 0;                         // number of non-volatile INT registers (r19-r28) saved in the canonical stack location
    function->H = 1;                            // Homes parameters
    // (indicating whether the function "homes" the integer parameter registers (r0-r7) by storing them at the very start of the function)

    function->CR = 3;                           // chained function, a store/load pair instruction is used in prolog/epilog <r29,lr>
    function->FrameSize = 5;                    // the number of bytes of stack that is allocated for this function divided by 16
}
#else
void InterpreterThunkEmitter::EncodeInterpreterThunk(__in_bcount(thunkSize) BYTE* thunkBuffer, __in_bcount(thunkSize) BYTE* thunkBufferStartAddress, __in const DWORD thunkSize, __in_bcount(epilogSize) BYTE* epilogStart, __in const DWORD epilogSize)
{
    _Analysis_assume_(thunkSize == HeaderSize);
    Emit(thunkBuffer, ThunkAddressOffset, (uintptr_t)interpreterThunk);
    thunkBuffer[DynamicThunkAddressOffset] = Js::FunctionBody::GetOffsetOfDynamicInterpreterThunk();
    thunkBuffer[FunctionBodyOffset] = Js::JavascriptFunction::GetOffsetOfFunctionInfo();
    Emit(thunkBuffer, CallBlockStartAddrOffset, (uintptr_t) thunkBufferStartAddress + HeaderSize);
    uint totalThunkSize = (uint)(epilogStart - (thunkBufferStartAddress + HeaderSize));
    Emit(thunkBuffer, ThunkSizeOffset, totalThunkSize);
    Emit(thunkBuffer, ErrorOffset, (BYTE) FAST_FAIL_INVALID_ARG);
}
#endif



inline /*static*/
DWORD InterpreterThunkEmitter::FillDebugBreak(__out_bcount_full(count) BYTE* dest, __in DWORD count)
{
#if defined(_M_ARM)
    Assert(count % 2 == 0);
#elif defined(_M_ARM64)
    Assert(count % 4 == 0);
#endif
    CustomHeap::FillDebugBreak(dest, count);
    return count;
}



inline /*static*/
DWORD InterpreterThunkEmitter::CopyWithAlignment(
    __in_bcount(sizeInBytes) BYTE* dest,
    __in const DWORD sizeInBytes,
    __in_bcount(srcSize) const BYTE* src,
    __in_range(0, sizeInBytes) const DWORD srcSize,
    __in const DWORD alignment)
{
    js_memcpy_s(dest, sizeInBytes, src, srcSize);
    dest += srcSize;

    DWORD alignPad = Math::Align(srcSize, alignment) - srcSize;
    Assert(alignPad <= (sizeInBytes - srcSize));
    if(alignPad > 0 && alignPad <= (sizeInBytes - srcSize))
    {
        FillDebugBreak(dest, alignPad);
        return srcSize + alignPad;
    }
    return srcSize;
}

// We only decommit at close because there might still be some
// code running here.
// The destructor of emitBufferManager will cause the eventual release.
void InterpreterThunkEmitter::Close()
{
#if PDATA_ENABLED
    auto unregiserPdata = ([&] (const ThunkBlock& block)
    {
        PDataManager::UnregisterPdata((PRUNTIME_FUNCTION) block.GetPdata());
    });
    thunkBlocks.Iterate(unregiserPdata);
    freeListedThunkBlocks.Iterate(unregiserPdata);
#endif
    this->thunkBlocks.Clear(allocator);
    this->freeListedThunkBlocks.Clear(allocator);
    emitBufferManager.Decommit();
    this->thunkBuffer = nullptr;
    this->thunkCount = 0;
}

void InterpreterThunkEmitter::Release(BYTE* thunkAddress, bool addtoFreeList)
{
    if(!addtoFreeList)
    {
        return;
    }

    auto predicate = ([=] (const ThunkBlock& block)
    {
        return block.Contains(thunkAddress);
    });

    ThunkBlock* block = freeListedThunkBlocks.Find(predicate);
    if(!block)
    {
        block = thunkBlocks.MoveTo(&freeListedThunkBlocks, predicate);
    }

    // if EnsureFreeList fails in an OOM scenario - we just leak the thunks
    if(block && block->EnsureFreeList(allocator))
    {
        block->Release(thunkAddress);
    }
}

BYTE* InterpreterThunkEmitter::AllocateFromFreeList(PVOID* ppDynamicInterpreterThunk )
{
    ThunkBlock& block = this->freeListedThunkBlocks.Head();
    BYTE* thunk = block.AllocateFromFreeList();
#if _M_ARM
    thunk = (BYTE*)((DWORD)thunk | 0x01);
#endif
    if(block.IsFreeListEmpty())
    {
        this->freeListedThunkBlocks.MoveHeadTo(&this->thunkBlocks);
    }
    *ppDynamicInterpreterThunk = thunk;
    BYTE* entryPoint = block.GetStart();
#if _M_ARM
    entryPoint = (BYTE*)((DWORD)entryPoint | 0x01);
#endif
    return entryPoint;
}


bool ThunkBlock::Contains(BYTE* address) const
{
    bool contains = address >= start && address < (start + InterpreterThunkEmitter::BlockSize);
    return contains;
}

void ThunkBlock::Release(BYTE* address)
{
    Assert(Contains(address));
    Assert(this->freeList);

    BVIndex index = FromThunkAddress(address);
    this->freeList->Set(index);
}

BYTE* ThunkBlock::AllocateFromFreeList()
{
    Assert(this->freeList);
    BVIndex index = this->freeList->GetNextBit(0);
    BYTE* address = ToThunkAddress(index);
    this->freeList->Clear(index);
    return address;
}

BVIndex ThunkBlock::FromThunkAddress(BYTE* address)
{
    int index = ((uint)(address - start) - InterpreterThunkEmitter::HeaderSize) / InterpreterThunkEmitter::ThunkSize;
    Assert(index < InterpreterThunkEmitter::ThunksPerBlock);
    return index;
}

BYTE* ThunkBlock::ToThunkAddress(BVIndex index)
{
    Assert(index < InterpreterThunkEmitter::ThunksPerBlock);
    BYTE* address = start + InterpreterThunkEmitter::HeaderSize + InterpreterThunkEmitter::ThunkSize * index;
    return address;
}

bool ThunkBlock::EnsureFreeList(ArenaAllocator* allocator)
{
    if(!this->freeList)
    {
        this->freeList = BVFixed::NewNoThrow(InterpreterThunkEmitter::ThunksPerBlock, allocator);
    }
    return this->freeList != nullptr;
}

bool ThunkBlock::IsFreeListEmpty() const
{
    Assert(this->freeList);
    return this->freeList->IsAllClear();
}

#endif

