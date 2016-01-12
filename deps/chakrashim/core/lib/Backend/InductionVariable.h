//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class InductionVariable;

typedef JsUtil::BaseHashSet<InductionVariable, JitArenaAllocator, PowerOf2SizePolicy, SymID> InductionVariableSet;

class InductionVariable
{
public:
    static const int ChangeMagnitudeLimitForLoopCountBasedHoisting;

private:
    StackSym *sym;
    ValueNumber symValueNumber;
    IntConstantBounds changeBounds;
    bool isChangeDeterminate;

public:
    InductionVariable();
    InductionVariable(StackSym *const sym, const ValueNumber symValueNumber, const int change);

#if DBG
private:
    bool IsValid() const;
#endif

public:
    StackSym *Sym() const;
    ValueNumber SymValueNumber() const;
    void SetSymValueNumber(const ValueNumber symValueNumber);
    bool IsChangeDeterminate() const;
    void SetChangeIsIndeterminate();
    const IntConstantBounds &ChangeBounds() const;
    bool IsChangeUnidirectional() const;

public:
    bool Add(const int n);
    void ExpandInnerLoopChange();

public:
    void Merge(const InductionVariable &other);
};

template<>
SymID JsUtil::ValueToKey<SymID, InductionVariable>::ToKey(const InductionVariable &inductionVariable)
{
    return inductionVariable.Sym()->m_id;
}
