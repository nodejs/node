//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "PrologEncoderMD.h"

void PrologEncoder::RecordNonVolRegSave()
{
    requiredUnwindCodeNodeCount++;
}

void PrologEncoder::RecordXmmRegSave()
{
    requiredUnwindCodeNodeCount += 2;
}

void PrologEncoder::RecordAlloca(size_t size)
{
    Assert(size);

    requiredUnwindCodeNodeCount += PrologEncoderMD::GetRequiredNodeCountForAlloca(size);
}

DWORD PrologEncoder::SizeOfPData()
{
    return sizeof(PData) + (sizeof(UNWIND_CODE) * requiredUnwindCodeNodeCount);
}

void PrologEncoder::SetPDataPtr(void *pdata)
{
    this->pdata = static_cast<PData *>(pdata);
}

void PrologEncoder::EncodeSmallProlog(uint8 prologSize, size_t allocaSize)
{
    Assert(!pdata);
    Assert(allocaSize <= 128);
    Assert(requiredUnwindCodeNodeCount == 0);

    // Increment requiredUnwindCodeNodeCount to ensure we do the Alloc for the correct size
    currentUnwindCodeNodeIndex = ++requiredUnwindCodeNodeCount;

    if (!pdata)
    {
        pdata = (PData *)alloc->Alloc(SizeOfPData());
    }

    currentInstrOffset = prologSize;

    UnwindCode* unwindCode = GetUnwindCode(1);
    size_t slots = (allocaSize - MachPtr) / MachPtr;
    uint8 unwindCodeOpInfo = TO_UNIBBLE(slots);

    unwindCode->SetOffset(prologSize);
    unwindCode->SetOp(TO_UNIBBLE(UWOP_ALLOC_SMALL));
    unwindCode->SetOpInfo(unwindCodeOpInfo);
}

void PrologEncoder::EncodeInstr(IR::Instr *instr, unsigned __int8 size)
{
    Assert(instr);
    Assert(size);

    // We've started encoding instructions. This is a good time to initialize
    // pdata as we now have the final size.
    if (!pdata)
    {
        pdata = (PData *)alloc->Alloc(SizeOfPData());
    }

    Assert(pdata);

    UnwindCode       *unwindCode       = nullptr;
    unsigned __int8   unwindCodeOp     = PrologEncoderMD::GetOp(instr);
    unsigned __int8   unwindCodeOpInfo = 0;
    unsigned __int16  uint16Val        = 0;
    unsigned __int32  uint32Val        = 0;

    if (!currentInstrOffset)
        currentUnwindCodeNodeIndex = requiredUnwindCodeNodeCount;

    Assert((currentInstrOffset + size) > currentInstrOffset);
    currentInstrOffset += size;

    switch (unwindCodeOp)
    {
    case UWOP_PUSH_NONVOL:
    {
        unwindCode = GetUnwindCode(1);
        unwindCodeOpInfo = PrologEncoderMD::GetNonVolRegToSave(instr);
        break;
    }

    case UWOP_SAVE_XMM128:
    {
        unwindCode       = GetUnwindCode(2);
        unwindCodeOpInfo = PrologEncoderMD::GetXmmRegToSave(instr, &uint16Val);

        *((unsigned __int16 *)&((UNWIND_CODE *)unwindCode)[1]) = uint16Val;
        break;
    }

    case UWOP_ALLOC_SMALL:
    {
        unwindCode = GetUnwindCode(1);
        size_t allocaSize = PrologEncoderMD::GetAllocaSize(instr);
        Assert((allocaSize - MachPtr) % MachPtr == 0);
        size_t slots = (allocaSize - MachPtr) / MachPtr;
        Assert(IS_UNIBBLE(slots));
        unwindCodeOpInfo = TO_UNIBBLE(slots);
        break;
    }

    case UWOP_ALLOC_LARGE:
    {
        size_t allocaSize = PrologEncoderMD::GetAllocaSize(instr);
        Assert(allocaSize > 0x80);
        Assert(allocaSize % 8 == 0);
        Assert(IS_UNIBBLE(unwindCodeOp));

        size_t slots = allocaSize / MachPtr;
        if (allocaSize > 0x7FF8)
        {
            unwindCode       = GetUnwindCode(3);
            uint32Val        = TO_UINT32(allocaSize);
            unwindCodeOpInfo = 1;

            unwindCode->SetOp(TO_UNIBBLE(unwindCodeOp));
            *((unsigned __int32 *)&((UNWIND_CODE *)unwindCode)[1]) = uint32Val;
        }
        else
        {
            unwindCode       = GetUnwindCode(2);
            uint16Val        = TO_UINT16(slots);
            unwindCodeOpInfo = 0;

            unwindCode->SetOp(TO_UNIBBLE(unwindCodeOp));
            *((unsigned __int16 *)&((UNWIND_CODE *)unwindCode)[1]) = uint16Val;
        }
        break;
    }

    case UWOP_IGNORE:
    {
        return;
    }

    default:
    {
        AssertMsg(false, "PrologEncoderMD returned unsupported UnwindCodeOp.");
    }
    }

    Assert(unwindCode);

    unwindCode->SetOffset(currentInstrOffset);

    Assert(IS_UNIBBLE(unwindCodeOp));
    unwindCode->SetOp(TO_UNIBBLE(unwindCodeOp));

    Assert(IS_UNIBBLE(unwindCodeOpInfo));
    unwindCode->SetOpInfo(TO_UNIBBLE(unwindCodeOpInfo));
}

BYTE *PrologEncoder::Finalize(BYTE *functionStart,
                              DWORD codeSize,
                              BYTE *pdataBuffer)
{
    Assert(pdata);
    Assert(pdataBuffer > functionStart);
    Assert((size_t)pdataBuffer % sizeof(DWORD) == 0);

    pdata->runtimeFunction.BeginAddress = 0;
    pdata->runtimeFunction.EndAddress   = codeSize;
    pdata->runtimeFunction.UnwindData   = (DWORD)((pdataBuffer + sizeof(RUNTIME_FUNCTION)) - functionStart);

    FinalizeUnwindInfo();

    return (BYTE *)&pdata->runtimeFunction;
}

void PrologEncoder::FinalizeUnwindInfo()
{
    pdata->unwindInfo.Version           = 1;
    pdata->unwindInfo.Flags             = 0;
    pdata->unwindInfo.SizeOfProlog      = currentInstrOffset;
    pdata->unwindInfo.CountOfCodes      = requiredUnwindCodeNodeCount;

    // We don't use the frame pointer in the standard way, and since we don't do dynamic stack allocation, we don't change the
    // stack pointer except during calls. From the perspective of the unwind info, it needs to restore information relative to
    // the stack pointer, so don't register the frame pointer.
    pdata->unwindInfo.FrameRegister     = 0;
    pdata->unwindInfo.FrameOffset       = 0;

    AssertMsg(requiredUnwindCodeNodeCount <= 34, "We allocate 72 bytes for xdata - 34 (UnwindCodes) * 2 + 4 (UnwindInfo)");
}

PrologEncoder::UnwindCode *PrologEncoder::GetUnwindCode(unsigned __int8 nodeCount)
{
    Assert(nodeCount && ((currentUnwindCodeNodeIndex - nodeCount) >= 0));
    currentUnwindCodeNodeIndex -= nodeCount;

    return static_cast<UnwindCode *>(&pdata->unwindInfo.unwindCodes[currentUnwindCodeNodeIndex]);
}

DWORD PrologEncoder::SizeOfUnwindInfo()
{
    return (sizeof(UNWIND_INFO) + (sizeof(UNWIND_CODE) * requiredUnwindCodeNodeCount));
}

BYTE *PrologEncoder::GetUnwindInfo()
{
    return (BYTE *)&pdata->unwindInfo;
}
