//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

const int InductionVariable::ChangeMagnitudeLimitForLoopCountBasedHoisting = 64 << 10;

InductionVariable::InductionVariable()
#if DBG
    : sym(nullptr)
#endif
{
    Assert(!IsValid());
}

InductionVariable::InductionVariable(StackSym *const sym, const ValueNumber symValueNumber, const int change)
    : sym(sym), symValueNumber(symValueNumber), changeBounds(change, change), isChangeDeterminate(true)
{
    Assert(sym);
    Assert(!sym->IsTypeSpec());
    Assert(IsValid());
}

#if DBG

bool InductionVariable::IsValid() const
{
    return !!sym;
}

#endif

StackSym *InductionVariable::Sym() const
{
    Assert(IsValid());
    return sym;
}

ValueNumber InductionVariable::SymValueNumber() const
{
    Assert(IsChangeDeterminate());
    return symValueNumber;
}

void InductionVariable::SetSymValueNumber(const ValueNumber symValueNumber)
{
    Assert(IsChangeDeterminate());
    this->symValueNumber = symValueNumber;
}

bool InductionVariable::IsChangeDeterminate() const
{
    Assert(IsValid());
    return isChangeDeterminate;
}

void InductionVariable::SetChangeIsIndeterminate()
{
    Assert(IsValid());
    isChangeDeterminate = false;
}

const IntConstantBounds &InductionVariable::ChangeBounds() const
{
    Assert(IsChangeDeterminate());
    return changeBounds;
}

bool InductionVariable::IsChangeUnidirectional() const
{
    return
        ChangeBounds().LowerBound() >= 0 && ChangeBounds().UpperBound() != 0 ||
        ChangeBounds().UpperBound() <= 0 && ChangeBounds().LowerBound() != 0;
}

bool InductionVariable::Add(const int n)
{
    Assert(IsChangeDeterminate());

    if(n == 0)
        return true;

    int newLowerBound;
    if(changeBounds.LowerBound() == IntConstMin)
    {
        if(n >= 0)
        {
            isChangeDeterminate = false;
            return false;
        }
        newLowerBound = IntConstMin;
    }
    else if(changeBounds.LowerBound() == IntConstMax)
    {
        if(n < 0)
        {
            isChangeDeterminate = false;
            return false;
        }
        newLowerBound = IntConstMax;
    }
    else if(Int32Math::Add(changeBounds.LowerBound(), n, &newLowerBound))
        newLowerBound = n < 0 ? IntConstMin : IntConstMax;

    int newUpperBound;
    if(changeBounds.UpperBound() == IntConstMin)
    {
        if(n >= 0)
        {
            isChangeDeterminate = false;
            return false;
        }
        newUpperBound = IntConstMin;
    }
    else if(changeBounds.UpperBound() == IntConstMax)
    {
        if(n < 0)
        {
            isChangeDeterminate = false;
            return false;
        }
        newUpperBound = IntConstMax;
    }
    else if(Int32Math::Add(changeBounds.UpperBound(), n, &newUpperBound))
        newUpperBound = n < 0 ? IntConstMin : IntConstMax;

    changeBounds = IntConstantBounds(newLowerBound, newUpperBound);
    return true;
}

void InductionVariable::ExpandInnerLoopChange()
{
    Assert(IsValid());

    if(!isChangeDeterminate)
        return;

    changeBounds =
        IntConstantBounds(
            changeBounds.LowerBound() < 0 ? IntConstMin : changeBounds.LowerBound(),
            changeBounds.UpperBound() > 0 ? IntConstMax : changeBounds.UpperBound());
}

void InductionVariable::Merge(const InductionVariable &other)
{
    Assert(Sym() == other.Sym());
    // The value number may be different, the caller will give the merged info the appropriate value number

    isChangeDeterminate &= other.isChangeDeterminate;
    if(!isChangeDeterminate)
        return;

    changeBounds =
        IntConstantBounds(
            min(changeBounds.LowerBound(), other.changeBounds.LowerBound()),
            max(changeBounds.UpperBound(), other.changeBounds.UpperBound()));
}
