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
void
PeepsMD::ProcessImplicitRegs(IR::Instr *instr)
{
    if (LowererMD::IsCall(instr))
    {
        this->peeps->ClearReg(RegR0);
        this->peeps->ClearReg(RegR1);
        this->peeps->ClearReg(RegR2);
        this->peeps->ClearReg(RegR3);
        this->peeps->ClearReg(RegR12);
        this->peeps->ClearReg(RegLR);
        this->peeps->ClearReg(RegD0);
        this->peeps->ClearReg(RegD1);
        this->peeps->ClearReg(RegD2);
        this->peeps->ClearReg(RegD3);
        this->peeps->ClearReg(RegD4);
        this->peeps->ClearReg(RegD5);
        this->peeps->ClearReg(RegD6);
        this->peeps->ClearReg(RegD7);
    }
    else if (instr->m_opcode == Js::OpCode::SMULL ||
             instr->m_opcode == Js::OpCode::SMLAL)
    {
        // As we don't currently have support for 4 operand instrs, we use R12 as 4th operand,
        // Notify the peeps that we use r12: SMULL, dst, r12, src1, src2.
        this->peeps->ClearReg(RegR12);
    }
}

void
PeepsMD::PeepAssign(IR::Instr *instr)
{
    return;
}
