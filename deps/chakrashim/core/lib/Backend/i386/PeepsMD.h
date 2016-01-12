//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Peeps;

class PeepsMD
{
private:
    Func *      func;
    Peeps *     peeps;
public:
    PeepsMD(Func *func) : func(func) {}

    void        Init(Peeps *peeps);
    void        ProcessImplicitRegs(IR::Instr *instr);
    void        PeepAssign(IR::Instr *instr);
};


