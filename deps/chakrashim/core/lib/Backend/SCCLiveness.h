//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "lifetime.h"

struct OpHelperSpilledLifetime
{
    Lifetime * lifetime;
    RegNum     reg;
    bool       spillAsArg;
    bool       reload; // determines if we want to reload the lifetime at the end of the helper
};

class OpHelperBlock
{
public:
    OpHelperBlock(JitArenaAllocator * alloc) : spilledLifetime(alloc) {}

    IR::LabelInstr * opHelperLabel;
    IR::Instr * opHelperEndInstr;
    SList<OpHelperSpilledLifetime> spilledLifetime;

public:
    uint Length() const;

private:
    // Disable copy constructor
    OpHelperBlock(OpHelperBlock const&);
};

class SCCLiveness
{
private:
    Func *          func;
    JitArenaAllocator *tempAlloc;
    Loop *          curLoop;
    uint32          lastCall;
    uint32          lastNonOpHelperCall;
    uint16          loopNest;
    IR::LabelInstr * lastOpHelperLabel;
    Region *        curRegion;
    SListBase<Loop*> * extendedLifetimesLoopList;
public:
    SCCLiveness(Func *func, JitArenaAllocator *tempAlloc) : func(func),
        tempAlloc(tempAlloc), loopNest(0), lastCall(0), lastNonOpHelperCall(0),
        curLoop(NULL), lastOpHelperLabel(NULL), opHelperBlockList(tempAlloc),
        curRegion(NULL), lifetimeList(tempAlloc),
        totalOpHelperFullVisitedLength(0)
    {
        extendedLifetimesLoopList = JitAnew(tempAlloc, SListBase<Loop *>);
    }

    void            Build();
#if DBG_DUMP
    void            Dump();
#endif

    SList<Lifetime *> lifetimeList;
    SList<OpHelperBlock>  opHelperBlockList;
    uint            totalOpHelperFullVisitedLength;

private:
    void            ProcessSrc(IR::Opnd *src, IR::Instr *instr);
    void            ProcessDst(IR::Opnd *dst, IR::Instr *instr);
    void            ProcessRegUse(IR::RegOpnd *regUse, IR::Instr *instr);
    void            ProcessRegDef(IR::RegOpnd *regDef, IR::Instr *instr);
    void            ExtendLifetime(Lifetime *lifetime, IR::Instr *instr);
    void            ProcessStackSymUse(StackSym * stackSym, IR::Instr * instr, int usageSize = MachPtr);
    void            ProcessBailOutUses(IR::Instr * bailOutInstr);
    Lifetime *      InsertLifetime(StackSym *stackSym, RegNum reg, IR::Instr *const currentInstr);
    void            EndOpHelper(IR::Instr * instr);
    bool            FoldIndir(IR::Instr *instr, IR::Opnd *opnd);
    uint            CurrentOpHelperVisitedLength(IR::Instr *const currentInstr) const;
};
