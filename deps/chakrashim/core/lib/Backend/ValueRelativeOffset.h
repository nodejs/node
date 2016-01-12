//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

typedef unsigned int ValueNumber;
class Value;

extern const ValueNumber InvalidValueNumber;
extern const ValueNumber ZeroValueNumber;
extern const ValueNumber FirstNewValueNumber;

class ValueRelativeOffset sealed
{
private:
    const Value *baseValue;
    int offset;
    bool wasEstablishedExplicitly;

public:
    ValueRelativeOffset();
    ValueRelativeOffset(const Value *const baseValue, const bool wasEstablishedExplicitly);
    ValueRelativeOffset(const Value *const baseValue, const int offset, const bool wasEstablishedExplicitly);

#if DBG
private:
    bool IsValid() const;
#endif

public:
    ValueNumber BaseValueNumber() const;
    StackSym *BaseSym() const;
    int Offset() const;
    void SetOffset(const int offset);
    bool WasEstablishedExplicitly() const;
    void SetWasEstablishedExplicitly();

public:
    bool Add(const int n);

public:
    template<bool Lower, bool Aggressive> void Merge(const ValueRelativeOffset &other);
    template<bool Lower, bool Aggressive> void MergeConstantValue(const int constantValue);
};

template<>
ValueNumber JsUtil::ValueToKey<ValueNumber, ValueRelativeOffset>::ToKey(const ValueRelativeOffset &valueRelativeOffset)
{
    return valueRelativeOffset.BaseValueNumber();
}
