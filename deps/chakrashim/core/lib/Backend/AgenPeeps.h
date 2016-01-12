//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


// Shared for x86 and x64
class AgenPeeps
{

private:
    Func *      func;

public:
    AgenPeeps(Func *func) : func(func){}
    void                 PeepFunc();
    bool DependentInstrs(IR::Instr *instr1, IR::Instr *instr2);

private:
    int MoveInstrUp(IR::Instr *instr, IR::Instr *startBlock, int distance);
    bool AlwaysDependent(IR::Instr *instr);
    bool DependentOpnds(IR::Opnd *opnd1, IR::Opnd *opnd2);
    bool AgenDependentInstrs(IR::Instr *instr1, IR::Instr *instr2);
    bool IsMemoryInstr(IR::Instr *instr);
    bool IsLoad(IR::Instr *instr);
    bool IsStore(IR::Instr *instr);
    bool IsMemoryOpnd(IR::Opnd *opnd);
};

