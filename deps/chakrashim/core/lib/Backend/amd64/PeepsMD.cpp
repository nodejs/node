//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

// PeepsMD::Init
void
PeepsMD::Init(Peeps *peeps)
{
    this->peeps = peeps;
}

// PeepsMD::ProcessImplicitRegs
// Note: only do calls for now
void
PeepsMD::ProcessImplicitRegs(IR::Instr *instr)
{
    if (LowererMD::IsCall(instr))
    {
        this->peeps->ClearReg(RegRAX);
        this->peeps->ClearReg(RegRCX);
        this->peeps->ClearReg(RegRDX);
        this->peeps->ClearReg(RegR8);
        this->peeps->ClearReg(RegR9);
        this->peeps->ClearReg(RegR10);
        this->peeps->ClearReg(RegR11);
        this->peeps->ClearReg(RegXMM0);
        this->peeps->ClearReg(RegXMM1);
        this->peeps->ClearReg(RegXMM2);
        this->peeps->ClearReg(RegXMM3);
        this->peeps->ClearReg(RegXMM4);
        this->peeps->ClearReg(RegXMM5);
    }
    else if (instr->m_opcode == Js::OpCode::IMUL)
    {
        this->peeps->ClearReg(RegRDX);
    }
    else if (instr->m_opcode == Js::OpCode::IDIV)
    {
        if (instr->GetDst()->AsRegOpnd()->GetReg() == RegRDX)
        {
            this->peeps->ClearReg(RegRAX);
        }
        else
        {
            Assert(instr->GetDst()->AsRegOpnd()->GetReg() == RegRAX);
            this->peeps->ClearReg(RegRDX);
        }
    }
}

void
PeepsMD::PeepAssign(IR::Instr *instr)
{
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src = instr->GetSrc1();
    if(dst->IsRegOpnd() && instr->m_opcode == Js::OpCode::MOV)
    {
        if (src->IsImmediateOpnd() && src->GetImmediateValue() == 0)
        {
            Assert(instr->GetSrc2() == NULL);

            // 32-bit XOR has a smaller encoding
            if (TySize[dst->GetType()] == MachPtr)
            {
                dst->SetType(TyInt32);
            }

            instr->m_opcode = Js::OpCode::XOR;
            instr->ReplaceSrc1(dst);
            instr->SetSrc2(dst);
        }
        else if (!instr->isInlineeEntryInstr)
        {
            if(src->IsIntConstOpnd() && src->GetSize() <= TySize[TyUint32])
            {
                dst->SetType(TyUint32);
            }
            else if(src->IsAddrOpnd() && (((size_t)src->AsAddrOpnd()->m_address >> 32) == 0 ))
            {
                instr->ReplaceSrc1(IR::IntConstOpnd::New(::Math::PointerCastToIntegral<UIntConstType>(src->AsAddrOpnd()->m_address), TyUint32, instr->m_func));
                dst->SetType(TyUint32);
            }
        }
    }
    else if (((instr->m_opcode == Js::OpCode::MOVSD || instr->m_opcode == Js::OpCode::MOVSS)
                && src->IsRegOpnd()
                && dst->IsRegOpnd()
                && (TySize[src->GetType()] == TySize[dst->GetType()]))
        || ((instr->m_opcode == Js::OpCode::MOVUPS)
                && src->IsRegOpnd()
                && dst->IsRegOpnd())
        || (instr->m_opcode == Js::OpCode::MOVAPD))
    {
        // MOVAPS has 1 byte shorter encoding
        instr->m_opcode = Js::OpCode::MOVAPS;
    }
    else if (instr->m_opcode == Js::OpCode::MOVSD_ZERO)
    {
        instr->m_opcode = Js::OpCode::XORPS;
        instr->SetSrc1(dst);
        instr->SetSrc2(dst);
    }
}


