//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "PrologEncoderMD.h"

unsigned __int8 PrologEncoderMD::GetRequiredNodeCountForAlloca(size_t size)
{
    Assert(size);
    Assert(size % 8 == 0);

    if (size <= 0x80)
        return 1;
    else if (size <= 0x7FF8)
        return 2;
    else
        return 3;
}

unsigned __int8 PrologEncoderMD::GetOp(IR::Instr *instr)
{
    switch (instr->m_opcode)
    {
    case Js::OpCode::PUSH:
        Assert(instr->GetSrc1()->IsRegOpnd());
        return UWOP_PUSH_NONVOL;

    case Js::OpCode::MOVAPD:
    case Js::OpCode::MOVAPS:
        Assert(instr->GetSrc1()->IsRegOpnd() && REGNUM_ISXMMXREG(instr->GetSrc1()->AsRegOpnd()->GetReg()));
        return UWOP_SAVE_XMM128;

    case Js::OpCode::MOV:
        // MOV rbp, rsp (or)
        // MOV rax, imm
        Assert(instr->GetDst() && instr->GetSrc1());
        return UWOP_IGNORE;

    case Js::OpCode::SUB:
        Assert(instr->GetSrc1() && instr->GetSrc2());
        Assert(instr->GetSrc1()->AsRegOpnd()->GetReg() == RegRSP &&
               instr->GetSrc2()->IsIntConstOpnd());
        if (instr->GetSrc2()->AsIntConstOpnd()->GetValue() <= 128)
            return UWOP_ALLOC_SMALL;
        else
            return UWOP_ALLOC_LARGE;

    case Js::OpCode::CALL:
        // TODO: call target could also be a stack overflow check.
        // Assert(instr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper == IR::HelperCRT_chkstk);
        Assert(instr->GetSrc1()->IsRegOpnd());
        return UWOP_IGNORE;

    default:
        AssertMsg(false, "Unsupported opcode in prolog.");
    }

    return UWOP_IGNORE;
}

unsigned __int8 PrologEncoderMD::GetNonVolRegToSave(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::PUSH);
    return (instr->GetSrc1()->AsRegOpnd()->GetReg() - 1) & 0xFF;
}

unsigned __int8 PrologEncoderMD::GetXmmRegToSave(IR::Instr *instr, unsigned __int16 *scaledOffset)
{
    Assert(scaledOffset);
    Assert(instr->m_opcode == Js::OpCode::MOVAPD || instr->m_opcode == Js::OpCode::MOVAPS);
    Assert(instr->GetDst() && instr->GetDst()->IsIndirOpnd());

    unsigned __int8 reg = ((instr->GetSrc1()->AsRegOpnd()->GetReg() - FIRST_XMM_REG) & 0xFF);
    unsigned __int32 offsetFromInstr = instr->GetDst()->AsIndirOpnd()->GetOffset();

    // The offset in the instruction is relative to the stack pointer before the saved reg size and stack args size were
    // subtracted, but the offset in the unwind info needs to be relative to the final stack pointer value
    // (see LowererMDArch::LowerEntryInstr), so adjust for that
    Assert(instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetReg() == LowererMDArch::GetRegStackPointer());
    Func *const topFunc = instr->m_func->GetTopFunc();
    offsetFromInstr += topFunc->GetArgsSize() + topFunc->GetSavedRegSize();

    // Can only encode nonnegative 16-byte-aligned offsets in the unwind info
    Assert(static_cast<int32>(offsetFromInstr) >= 0);
    Assert(::Math::Align(offsetFromInstr, static_cast<unsigned __int32>(MachDouble * 2)) == offsetFromInstr);

    // Stored offset is scaled by 16
    offsetFromInstr /= MachDouble * 2;

    // We currently don't allow a total stack size greater than 1 MB. If we start allowing that, we will need to handle offsets
    // greater than (1 MB - 16) in the unwind info as well, for which a FAR version of the op-code is necessary.
    Assert(IS_UINT16(offsetFromInstr));

    *scaledOffset = TO_UINT16(offsetFromInstr);

    return reg;
}

size_t PrologEncoderMD::GetAllocaSize(IR::Instr *instr)
{
    Assert(instr->m_opcode == Js::OpCode::SUB);
    Assert(instr->GetSrc1() && instr->GetSrc2());
    Assert(instr->GetSrc1()->AsRegOpnd()->GetReg() == RegRSP &&
           instr->GetSrc2()->IsIntConstOpnd());
    Assert(instr->GetSrc2()->AsIntConstOpnd()->GetValue() % 8 == 0);

    return instr->GetSrc2()->AsIntConstOpnd()->GetValue();
}

unsigned __int8 PrologEncoderMD::GetFPReg()
{
    return RegRBP - 1;
}
