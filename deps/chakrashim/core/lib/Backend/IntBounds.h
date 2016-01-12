//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Value;
class IntConstantBounds;

typedef JsUtil::BaseHashSet<ValueRelativeOffset, JitArenaAllocator, PowerOf2SizePolicy, ValueNumber> RelativeIntBoundSet;

#pragma region IntBounds

class IntBounds sealed
{
private:
    int constantLowerBound, constantUpperBound;
    bool wasConstantUpperBoundEstablishedExplicitly;
    RelativeIntBoundSet relativeLowerBounds, relativeUpperBounds;

protected:
    IntBounds(const IntConstantBounds &constantBounds, const bool wasConstantUpperBoundEstablishedExplicitly, JitArenaAllocator *const allocator);

public:
    static IntBounds *New(const IntConstantBounds &constantBounds, const bool wasConstantUpperBoundEstablishedExplicitly, JitArenaAllocator *const allocator);
    IntBounds *Clone() const;
    void Delete() const;

public:
    void Verify() const;

private:
    bool HasBounds() const;
public:
    bool RequiresIntBoundedValueInfo(const ValueType valueType) const;
    bool RequiresIntBoundedValueInfo() const;

public:
    int ConstantLowerBound() const;
    int ConstantUpperBound() const;
    IntConstantBounds ConstantBounds() const;
    bool WasConstantUpperBoundEstablishedExplicitly() const;

public:
    const RelativeIntBoundSet &RelativeLowerBounds() const;
    const RelativeIntBoundSet &RelativeUpperBounds() const;

public:
    void SetLowerBound(const int constantBound);
    void SetLowerBound(const int constantBoundBase, const int offset);
    void SetUpperBound(const int constantBound, const bool wasEstablishedExplicitly);
    void SetUpperBound(const int constantBoundBase, const int offset, const bool wasEstablishedExplicitly);
private:
    template<bool Lower> void SetBound(const int constantBoundBase, const int offset, const bool wasEstablishedExplicitly);

public:
    void SetLowerBound(const ValueNumber myValueNumber, const Value *const baseValue, const bool wasEstablishedExplicitly);
    void SetLowerBound(const ValueNumber myValueNumber, const Value *const baseValue, const int offset, const bool wasEstablishedExplicitly);
    void SetUpperBound(const ValueNumber myValueNumber, const Value *const baseValue, const bool wasEstablishedExplicitly);
    void SetUpperBound(const ValueNumber myValueNumber, const Value *const baseValue, const int offset, const bool wasEstablishedExplicitly);
private:
    template<bool Lower> void SetBound(const ValueNumber myValueNumber, const Value *const baseValue, const int offset, const bool wasEstablishedExplicitly);

public:
    bool SetIsNot(const int constantValue, const bool isExplicit);
    bool SetIsNot(const Value *const value, const bool isExplicit);

public:
    static bool IsGreaterThanOrEqualTo(const int constantValue, const int constantBoundBase, const int offset);
    static bool IsLessThanOrEqualTo(const int constantValue, const int constantBoundBase, const int offset);

public:
    bool IsGreaterThanOrEqualTo(const int constantBoundBase, const int offset = 0) const;
    bool IsLessThanOrEqualTo(const int constantBoundBase, const int offset = 0) const;

public:
    bool IsGreaterThanOrEqualTo(const Value *const value, const int offset = 0) const;
    bool IsLessThanOrEqualTo(const Value *const value, const int offset = 0) const;

public:
    static const IntBounds *Add(const Value *const baseValue, const int n, const bool baseValueInfoIsPrecise, const IntConstantBounds &newConstantBounds, JitArenaAllocator *const allocator);

public:
    bool AddCannotOverflowBasedOnRelativeBounds(const int n) const;
    bool SubCannotOverflowBasedOnRelativeBounds(const int n) const;

public:
    static const IntBounds *Merge(const Value *const bounds0Value, const IntBounds *const bounds0, const Value *const bounds1Value, const IntConstantBounds &constantBounds1);
    static const IntBounds *Merge(const Value *const bounds0Value, const IntBounds *const bounds0, const Value *const bounds1Value, const IntBounds *const bounds1);
private:
    template<bool Lower> static void MergeBoundSets(const Value *const bounds0Value, const IntBounds *const bounds0, const Value *const bounds1Value, const IntBounds *const bounds1, IntBounds *const mergedBounds);

    PREVENT_ASSIGN(IntBounds);
};

#pragma endregion

#pragma region IntBoundCheckCompatibilityId and IntBoundCheck

// A bounds check takes the form (a <= b + offset). For instance, (i >= 0) would be represented as (0 <= i + 0), and (i < n)
// would be represented as (i <= n - 1). When a bounds check is required, from the two value numbers, it can be determined
// whether an existing, compatible bounds check can be updated by decreasing the offset aggressively instead of adding a new
// bounds check. The offset may only be decreased such that it does not invalidate the invariant previously established by the
// check.
//
// For instance, given that we have the bound checks (0 <= i + 0) and (i <= n - 1), suppose the bounds checks (i + 1 >= 0) and
// (i + 1 < n) are required. (i + 1 >= 0) would be represented as (0 <= i + 1). Given that (0 <= i + 0), it is already
// guaranteed that (0 <= i + 1), so no change is necessary for that part. (i + 1 < n) would be represented as (i <= n + 0). The
// compatible bounds check (i <= n - 1) would be found and updated aggressively to (i <= n + 0).

class IntBoundCheckCompatibilityId
{
private:
    const ValueNumber leftValueNumber, rightValueNumber;

public:
    IntBoundCheckCompatibilityId(const ValueNumber leftValueNumber, const ValueNumber rightValueNumber);

public:
    bool operator ==(const IntBoundCheckCompatibilityId &other) const;
    operator hash_t() const;
};

class IntBoundCheck
{
private:
    ValueNumber leftValueNumber, rightValueNumber;
    IR::Instr *instr;
    BasicBlock *block;

public:
    IntBoundCheck();
    IntBoundCheck(const ValueNumber leftValueNumber, const ValueNumber rightValueNumber, IR::Instr *const instr, BasicBlock *const block);

#if DBG
public:
    bool IsValid() const;
#endif

public:
    ValueNumber LeftValueNumber() const;
    ValueNumber RightValueNumber() const;
    IR::Instr *Instr() const;
    BasicBlock *Block() const;
    IntBoundCheckCompatibilityId CompatibilityId() const;

public:
    bool SetBoundOffset(const int offset, const bool isLoopCountBasedBound = false) const;
};

template<>
IntBoundCheckCompatibilityId JsUtil::ValueToKey<IntBoundCheckCompatibilityId, IntBoundCheck>::ToKey(
    const IntBoundCheck &intBoundCheck)
{
    return intBoundCheck.CompatibilityId();
}

typedef JsUtil::BaseHashSet<IntBoundCheck, JitArenaAllocator, PowerOf2SizePolicy, IntBoundCheckCompatibilityId> IntBoundCheckSet;

#pragma endregion
