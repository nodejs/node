//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Lifetime
{
public:

    Lifetime(JitArenaAllocator * alloc, StackSym *sym, RegNum reg, uint32 start, uint32 end, Func *func)
        : sym(sym), reg(reg), start(start), end(end), previousDefBlockNumber(0), defList(alloc),
        useList(alloc), lastUseLabel(NULL), region(NULL), isSpilled(false), useCount(0), useCountAdjust(0), allDefsCost(0), isLiveAcrossCalls(false),
        isLiveAcrossUserCalls(false), isDeadStore(true), isOpHelperSpilled(false), cantOpHelperSpill(false), isOpHelperSpillAsArg(false),
        isFloat(false), isSimd128F4(false), isSimd128I4(false), isSimd128D2(false), cantSpill(false), dontAllocate(false), isSecondChanceAllocated(false), isCheapSpill(false), spillStackSlot(NULL),
          totalOpHelperLengthByEnd(0), needsStoreCompensation(false), alloc(alloc), regionUseCount(NULL), regionUseCountAdjust(NULL),
          cantStackPack(false)
    {
        intUsageBv.ClearAll();
        regPreference.ClearAll();
    }

public:
    StackSym *          sym;
    uint32 *            regionUseCount;
    uint32 *            regionUseCountAdjust;
    SList<IR::Instr *>  defList;
    SList<IR::Instr *>  useList;
    IR::LabelInstr *    lastUseLabel;
    Region *            region;
    StackSlot *         spillStackSlot;
    JitArenaAllocator * alloc;
    BitVector           intUsageBv;
    BitVector           regPreference;
    uint32              start;
    uint32              end;
    uint32              previousDefBlockNumber;
    uint32              useCount;
    uint32              useCountAdjust;
    uint32              allDefsCost;
    uint32              lastAllocationStart;
    RegNum              reg;
    uint                totalOpHelperLengthByEnd;
    uint8               isSpilled:1;
    uint8               isLiveAcrossCalls:1;
    uint8               isLiveAcrossUserCalls:1;
    uint8               isDeadStore:1;
    uint8               isOpHelperSpilled:1;
    uint8               isOpHelperSpillAsArg : 1;
    uint8               cantOpHelperSpill:1;
    uint8               isFloat:1;
    uint8               isSimd128F4 : 1;
    uint8               isSimd128I4 : 1;
    uint8               isSimd128D2 : 1;
    uint8               cantSpill:1;
    uint8               dontAllocate:1;
    uint8               isSecondChanceAllocated:1;
    uint8               isCheapSpill:1;
    uint8               needsStoreCompensation:1;
    uint8               cantStackPack : 1;

    bool isSimd128()
    {
        return (isSimd128F4 | isSimd128I4 | isSimd128D2) != 0;
    }

    void AddToUseCount(uint32 newUseValue, Loop *loop, Func *func)
    {
        Assert((this->useCount + newUseValue) >= this->useCount);
        this->useCount += newUseValue;

        if (loop)
        {
            if (!this->regionUseCount)
            {
                this->regionUseCount = AnewArrayZ(this->alloc, uint32, func->loopCount+1);
                this->regionUseCountAdjust = AnewArrayZ(this->alloc, uint32, func->loopCount+1);
            }
            while (loop)
            {
                this->regionUseCount[loop->loopNumber] += newUseValue;
                loop = loop->parent;
            }
        }
    }
    void SubFromUseCount(uint32 newUseValue, Loop *loop)
    {
        Assert((this->useCount - newUseValue) <= this->useCount);
        this->useCount -= newUseValue;

        Assert(!loop || this->regionUseCount);

        while (loop)
        {
            Assert((this->regionUseCount[loop->loopNumber] - newUseValue) <= this->regionUseCount[loop->loopNumber]);
            this->regionUseCount[loop->loopNumber] -= newUseValue;
            loop = loop->parent;
        }
    }
    uint32 GetRegionUseCount(Loop *loop)
    {
        if (loop && !PHASE_OFF1(Js::RegionUseCountPhase))
        {
            if (this->regionUseCount)
            {
                return this->regionUseCount[loop->loopNumber];
            }
            else
            {
                return 0;
            }
        }
        else
        {
            return this->useCount;
        }
    }
    void AddToUseCountAdjust(uint32 newUseValue, Loop *loop, Func *func)
    {
        Assert((this->useCountAdjust + newUseValue) >= this->useCountAdjust);
        this->useCountAdjust += newUseValue;

        if (loop)
        {
            if (!this->regionUseCount)
            {
                this->regionUseCount = AnewArrayZ(this->alloc, uint32, func->loopCount+1);
                this->regionUseCountAdjust = AnewArrayZ(this->alloc, uint32, func->loopCount+1);
            }
            do
            {
                this->regionUseCountAdjust[loop->loopNumber] += newUseValue;
                loop = loop->parent;
            } while (loop);
        }
    }
    void ApplyUseCountAdjust(Loop *loop)
    {
        Assert((this->useCount + this->useCountAdjust) >= this->useCount);
        this->useCount -= this->useCountAdjust;
        this->useCountAdjust = 0;

        if (loop && this->regionUseCount)
        {
            do
            {
                Assert((this->regionUseCount[loop->loopNumber] - this->regionUseCountAdjust[loop->loopNumber]) <= this->regionUseCount[loop->loopNumber]);
                this->regionUseCount[loop->loopNumber] -= this->regionUseCountAdjust[loop->loopNumber];
                this->regionUseCountAdjust[loop->loopNumber] = 0;
                loop = loop->parent;
            } while (loop);
        }
    }
};

