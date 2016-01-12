//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class BackwardPass;

enum class ValueStructureKind
{
    Generic,
    IntConstant,
    IntRange,
    IntBounded,
    FloatConstant,
    VarConstant,
    JsType,
    Array
};

class IntConstantValueInfo;
class IntRangeValueInfo;
class IntBoundedValueInfo;
class FloatConstantValueInfo;
class VarConstantValueInfo;
class JsTypeValueInfo;
class EquivalentTypeSetValueInfo;
class ArrayValueInfo;
class LoopCount;
class GlobOpt;

class ValueInfo : protected ValueType
{
private:
    const ValueStructureKind structureKind;
    Sym *                   symStore;

protected:
    ValueInfo(const ValueType type, const ValueStructureKind structureKind)
        : ValueType(type), structureKind(structureKind)
    {
        // We can only prove that the representation is a tagged int on a ToVar. Currently, we cannot have more than one value
        // info per value number in a block, so a value info specifying tagged int representation cannot be created for a
        // specific sym. Instead, a value info can be shared by multiple syms, and hence cannot specify tagged int
        // representation. Currently, the tagged int representation info can only be carried on the dst opnd of ToVar, and can't
        // even be propagated forward.
        Assert(!type.IsTaggedInt());

        SetSymStore(nullptr);
    }

private:
    ValueInfo(const ValueInfo &other, const bool)
        : ValueType(other), structureKind(ValueStructureKind::Generic) // uses generic structure kind, as opposed to copying the structure kind
    {
        SetSymStore(other.GetSymStore());
    }

public:
    static ValueInfo *          New(JitArenaAllocator *const alloc, const ValueType type)
    {
        return JitAnew(alloc, ValueInfo, type, ValueStructureKind::Generic);
    }

    const ValueType &       Type() const { return *this; }
    ValueType &             Type() { return *this; }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // ValueType imports. Only importing functions that are appropriate to be called on Value.

public:
    using ValueType::IsUninitialized;
    using ValueType::IsDefinite;

    using ValueType::IsTaggedInt;
    using ValueType::IsIntAndLikelyTagged;
    using ValueType::IsLikelyTaggedInt;

    using ValueType::HasBeenUntaggedInt;
    using ValueType::IsIntAndLikelyUntagged;
    using ValueType::IsLikelyUntaggedInt;

    using ValueType::HasBeenInt;
    using ValueType::IsInt;
    using ValueType::IsLikelyInt;

    using ValueType::IsNotInt;
    using ValueType::IsNotNumber;

    using ValueType::HasBeenFloat;
    using ValueType::IsFloat;
    using ValueType::IsLikelyFloat;

    using ValueType::HasBeenNumber;
    using ValueType::IsNumber;
    using ValueType::IsLikelyNumber;

    using ValueType::HasBeenUnknownNumber;
    using ValueType::IsUnknownNumber;
    using ValueType::IsLikelyUnknownNumber;

    using ValueType::HasBeenUndefined;
    using ValueType::IsUndefined;
    using ValueType::IsLikelyUndefined;

    using ValueType::HasBeenNull;
    using ValueType::IsNull;
    using ValueType::IsLikelyNull;

    using ValueType::HasBeenBoolean;
    using ValueType::IsBoolean;
    using ValueType::IsLikelyBoolean;

    using ValueType::HasBeenString;
    using ValueType::IsString;
    using ValueType::IsLikelyString;
    using ValueType::IsNotString;

    using ValueType::HasBeenPrimitive;
    using ValueType::IsPrimitive;
    using ValueType::IsLikelyPrimitive;

    using ValueType::HasBeenObject;
    using ValueType::IsObject;
    using ValueType::IsLikelyObject;
    using ValueType::IsNotObject;
    using ValueType::CanMergeToObject;
    using ValueType::CanMergeToSpecificObjectType;

    using ValueType::IsRegExp;
    using ValueType::IsLikelyRegExp;

    using ValueType::IsArray;
    using ValueType::IsLikelyArray;
    using ValueType::IsNotArray;

    using ValueType::IsArrayOrObjectWithArray;
    using ValueType::IsLikelyArrayOrObjectWithArray;
    using ValueType::IsNotArrayOrObjectWithArray;

    using ValueType::IsNativeArray;
    using ValueType::IsLikelyNativeArray;
    using ValueType::IsNotNativeArray;

    using ValueType::IsNativeIntArray;
    using ValueType::IsLikelyNativeIntArray;

    using ValueType::IsNativeFloatArray;
    using ValueType::IsLikelyNativeFloatArray;

    using ValueType::IsTypedArray;
    using ValueType::IsLikelyTypedArray;

    using ValueType::IsOptimizedTypedArray;
    using ValueType::IsLikelyOptimizedTypedArray;
    using ValueType::IsLikelyOptimizedVirtualTypedArray;

    using ValueType::IsAnyArrayWithNativeFloatValues;
    using ValueType::IsLikelyAnyArrayWithNativeFloatValues;

    using ValueType::IsAnyArray;
    using ValueType::IsLikelyAnyArray;

    using ValueType::IsAnyOptimizedArray;
    using ValueType::IsLikelyAnyOptimizedArray;

    // The following apply to object types only
    using ValueType::GetObjectType;

    // The following apply to javascript array types only
    using ValueType::HasNoMissingValues;
    using ValueType::HasIntElements;
    using ValueType::HasFloatElements;
    using ValueType::HasVarElements;

    using ValueType::IsSimd128;
    using ValueType::IsSimd128Float32x4;
    using ValueType::IsSimd128Int32x4;
    using ValueType::IsSimd128Float64x2;
    using ValueType::IsLikelySimd128;
    using ValueType::IsLikelySimd128Float32x4;
    using ValueType::IsLikelySimd128Int32x4;
    using ValueType::IsLikelySimd128Float64x2;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

public:
    bool                            IsGeneric() const;

private:
    bool                            IsIntConstant() const;
    const IntConstantValueInfo *    AsIntConstant() const;
    bool                            IsIntRange() const;
    const IntRangeValueInfo *       AsIntRange() const;

public:
    bool                            IsIntBounded() const;
    const IntBoundedValueInfo *     AsIntBounded() const;
    bool                            IsFloatConstant() const;
    FloatConstantValueInfo *        AsFloatConstant();
    const FloatConstantValueInfo *  AsFloatConstant() const;
    bool                            IsVarConstant() const;
    VarConstantValueInfo *          AsVarConstant();
    bool                            IsJsType() const;
    JsTypeValueInfo *               AsJsType();
    const JsTypeValueInfo *         AsJsType() const;
#if FALSE
    bool                            IsObjectType() const;
    JsTypeValueInfo *               AsObjectType();
    bool                            IsEquivalentTypeSet() const;
    EquivalentTypeSetValueInfo *    AsEquivalentTypeSet();
#endif
    bool                            IsArrayValueInfo() const;
    const ArrayValueInfo *          AsArrayValueInfo() const;
    ArrayValueInfo *                AsArrayValueInfo();

public:
    bool HasIntConstantValue(const bool includeLikelyInt = false) const;
    bool TryGetIntConstantValue(int32 *const intValueRef, const bool includeLikelyInt = false) const;
    bool TryGetIntConstantLowerBound(int32 *const intConstantBoundRef, const bool includeLikelyInt = false) const;
    bool TryGetIntConstantUpperBound(int32 *const intConstantBoundRef, const bool includeLikelyInt = false) const;
    bool TryGetIntConstantBounds(IntConstantBounds *const intConstantBoundsRef, const bool includeLikelyInt = false) const;
    bool WasNegativeZeroPreventedByBailout() const;

public:
    static bool IsEqualTo(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2);
    static bool IsNotEqualTo(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2);
private:
    static bool IsEqualTo_NoConverse(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2);
    static bool IsNotEqualTo_NoConverse(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2);

public:
    static bool IsGreaterThanOrEqualTo(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2);
    static bool IsGreaterThan(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2);
    static bool IsLessThanOrEqualTo(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2);
    static bool IsLessThan(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2);

public:
    static bool IsGreaterThanOrEqualTo(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2, const int src2Offset);
    static bool IsLessThanOrEqualTo(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2, const int src2Offset);

private:
    static bool IsGreaterThanOrEqualTo_NoConverse(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2, const int src2Offset);
    static bool IsLessThanOrEqualTo_NoConverse(const Value *const src1Value, const int32 min1, const int32 max1, const Value *const src2Value, const int32 min2, const int32 max2, const int src2Offset);

public:
    ValueInfo *SpecializeToInt32(JitArenaAllocator *const allocator, const bool isForLoopBackEdgeCompensation = false);
    ValueInfo *SpecializeToFloat64(JitArenaAllocator *const allocator);

    // SIMD_JS
    ValueInfo *SpecializeToSimd128(IRType type, JitArenaAllocator *const allocator);
    ValueInfo *SpecializeToSimd128F4(JitArenaAllocator *const allocator);
    ValueInfo *SpecializeToSimd128I4(JitArenaAllocator *const allocator);



public:
    Sym *                   GetSymStore() const { return this->symStore; }
    void                    SetSymStore(Sym * sym)
    {
        // Sym store should always be a var sym
        Assert(sym == nullptr || sym->IsPropertySym() || !sym->AsStackSym()->IsTypeSpec()); // property syms always have a var stack sym
        this->symStore = sym;
    }

    bool GetIsShared() const;
    void SetIsShared();

    ValueInfo *             Copy(JitArenaAllocator * allocator);

    ValueInfo *             CopyWithGenericStructureKind(JitArenaAllocator * allocator) const
    {
        return JitAnew(allocator, ValueInfo, *this, false);
    }

    bool                    GetIntValMinMax(int *pMin, int *pMax, bool doAggressiveIntTypeSpec);

#if DBG_DUMP
    void                    Dump();
#endif
#if DBG
    // Add a vtable in debug builds so that the actual can been inspected easily in the debugger without having to manually cast
    virtual void            AddVtable() { Assert(false); }
#endif
};

class Value
{
private:
    const ValueNumber valueNumber;
    ValueInfo *valueInfo;

protected:
    Value(const ValueNumber valueNumber, ValueInfo *valueInfo)
        : valueNumber(valueNumber), valueInfo(valueInfo)
    {
    };

public:
    static Value *New(JitArenaAllocator *const allocator, const ValueNumber valueNumber, ValueInfo *valueInfo)
    {
        return JitAnew(allocator, Value, valueNumber, valueInfo);
    }

    ValueNumber GetValueNumber() const { return this->valueNumber; }
    ValueInfo * GetValueInfo() const { return this->valueInfo; }
    ValueInfo * ShareValueInfo() const { this->valueInfo->SetIsShared(); return this->valueInfo; }

    void        SetValueInfo(ValueInfo * newValueInfo) { Assert(newValueInfo); this->valueInfo = newValueInfo; }

    Value *     Copy(JitArenaAllocator * allocator, ValueNumber newValueNumber) { return Value::New(allocator, newValueNumber, this->ShareValueInfo()); }

#if DBG_DUMP
    __declspec(noinline) void Dump() const { Output::Print(L"0x%X  ValueNumber: %3d,  -> ", this, this->valueNumber);  this->valueInfo->Dump(); }
#endif
};

template<>
ValueNumber JsUtil::ValueToKey<ValueNumber, Value *>::ToKey(Value *const &value)
{
    Assert(value);
    return value->GetValueNumber();
}

class IntConstantValueInfo : public ValueInfo
{
private:
    const int32 intValue;

protected:
    IntConstantValueInfo(const int32 intValue)
        : ValueInfo(GetInt(IsTaggable(intValue)), ValueStructureKind::IntConstant),
        intValue(intValue)
    {
    }

public:
    static IntConstantValueInfo *New(JitArenaAllocator *const allocator, const int32 intValue)
    {
        return JitAnew(allocator, IntConstantValueInfo, intValue);
    }

    IntConstantValueInfo *Copy(JitArenaAllocator *const allocator) const
    {
        return JitAnew(allocator, IntConstantValueInfo, *this);
    }

public:
    int32 IntValue() const
    {
        return intValue;
    }

private:
    static bool IsTaggable(const int32 i)
    {
#if INT32VAR
        // All 32-bit ints are taggable on 64-bit architectures
        return true;
#else
        return i >= Js::Constants::Int31MinValue && i <= Js::Constants::Int31MaxValue;
#endif
    }
};

class IntRangeValueInfo : public ValueInfo, public IntConstantBounds
{
private:
    // Definitely-int values are inherently not negative zero. This member variable, if true, indicates that this value was
    // produced by an int-specialized instruction that prevented a negative zero result using a negative zero bailout
    // (BailOutOnNegativeZero). Negative zero tracking in the dead-store phase tracks this information to see if some of these
    // negative zero bailout checks can be removed.
    bool wasNegativeZeroPreventedByBailout;

protected:
    IntRangeValueInfo(
        const IntConstantBounds &constantBounds,
        const bool wasNegativeZeroPreventedByBailout,
        const ValueStructureKind structureKind = ValueStructureKind::IntRange)
        : ValueInfo(constantBounds.GetValueType(), structureKind),
        IntConstantBounds(constantBounds),
        wasNegativeZeroPreventedByBailout(wasNegativeZeroPreventedByBailout)
    {
        Assert(!wasNegativeZeroPreventedByBailout || constantBounds.LowerBound() <= 0 && constantBounds.UpperBound() >= 0);
    }

public:
    static IntRangeValueInfo *New(
        JitArenaAllocator *const allocator,
        const int32 lowerBound,
        const int32 upperBound,
        const bool wasNegativeZeroPreventedByBailout)
    {
        return
            JitAnew(
                allocator,
                IntRangeValueInfo,
                IntConstantBounds(lowerBound, upperBound),
                wasNegativeZeroPreventedByBailout);
    }

    IntRangeValueInfo *Copy(JitArenaAllocator *const allocator) const
    {
        return JitAnew(allocator, IntRangeValueInfo, *this);
    }

public:
    bool WasNegativeZeroPreventedByBailout() const
    {
        return wasNegativeZeroPreventedByBailout;
    }
};

class FloatConstantValueInfo : public ValueInfo
{
private:
    const FloatConstType floatValue;

public:
    FloatConstantValueInfo(const FloatConstType floatValue)
        : ValueInfo(Float, ValueStructureKind::FloatConstant), floatValue(floatValue)
    {
    }

    static FloatConstantValueInfo *New(
        JitArenaAllocator *const allocator,
        const FloatConstType floatValue)
    {
        return JitAnew(allocator, FloatConstantValueInfo, floatValue);
    }

    FloatConstantValueInfo *Copy(JitArenaAllocator *const allocator) const
    {
        return JitAnew(allocator, FloatConstantValueInfo, *this);
    }

public:
    FloatConstType FloatValue() const
    {
        return floatValue;
    }
};

class VarConstantValueInfo : public ValueInfo
{
private:
    Js::Var const varValue;
    bool isFunction;

public:
    VarConstantValueInfo(Js::Var varValue, ValueType valueType, bool isFunction = false)
        : ValueInfo(valueType, ValueStructureKind::VarConstant),
        varValue(varValue), isFunction(isFunction)
    {
    }

    static VarConstantValueInfo *New(JitArenaAllocator *const allocator, Js::Var varValue, ValueType valueType, bool isFunction = false)
    {
        return JitAnew(allocator, VarConstantValueInfo, varValue, valueType, isFunction);
    }

    VarConstantValueInfo *Copy(JitArenaAllocator *const allocator) const
    {
        return JitAnew(allocator, VarConstantValueInfo, *this);
    }

public:
    Js::Var VarValue() const
    {
        return this->varValue;
    }

    bool IsFunction() const
    {
        return this->isFunction;
    }
};

struct ObjectTypePropertyEntry
{
    Js::ObjTypeSpecFldInfo* fldInfo;
    uint blockNumber;
};

typedef JsUtil::BaseDictionary<Js::PropertyId, ObjectTypePropertyEntry, JitArenaAllocator> ObjectTypePropertyMap;

class JsTypeValueInfo : public ValueInfo
{
private:
    const Js::Type * jsType;
    Js::EquivalentTypeSet * jsTypeSet;
    bool isShared;

public:
    JsTypeValueInfo(Js::Type * type)
        : ValueInfo(Uninitialized, ValueStructureKind::JsType),
        jsType(type), jsTypeSet(nullptr), isShared(false)
    {
    }

    JsTypeValueInfo(Js::EquivalentTypeSet * typeSet)
        : ValueInfo(Uninitialized, ValueStructureKind::JsType),
        jsType(nullptr), jsTypeSet(typeSet), isShared(false)
    {
    }

    JsTypeValueInfo(const JsTypeValueInfo& other)
        : ValueInfo(Uninitialized, ValueStructureKind::JsType),
        jsType(other.jsType), jsTypeSet(other.jsTypeSet)
    {
    }

    static JsTypeValueInfo * New(JitArenaAllocator *const allocator, Js::Type * typeSet)
    {
        return JitAnew(allocator, JsTypeValueInfo, typeSet);
    }

    static JsTypeValueInfo * New(JitArenaAllocator *const allocator, Js::EquivalentTypeSet * typeSet)
    {
        return JitAnew(allocator, JsTypeValueInfo, typeSet);
    }

    JsTypeValueInfo(const Js::Type* type, Js::EquivalentTypeSet * typeSet)
        : ValueInfo(Uninitialized, ValueStructureKind::JsType),
        jsType(type), jsTypeSet(typeSet), isShared(false)
    {
    }

    static JsTypeValueInfo * New(JitArenaAllocator *const allocator, const Js::Type* type, Js::EquivalentTypeSet * typeSet)
    {
        return JitAnew(allocator, JsTypeValueInfo, type, typeSet);
    }

public:
    JsTypeValueInfo * Copy(JitArenaAllocator *const allocator) const
    {
        JsTypeValueInfo * newInfo = JitAnew(allocator, JsTypeValueInfo, *this);
        newInfo->isShared = false;
        return newInfo;
    }

    const Js::Type * GetJsType() const
    {
        return this->jsType;
    }

    void SetJsType(const Js::Type * value)
    {
        Assert(!this->isShared);
        this->jsType = value;
    }

    Js::EquivalentTypeSet * GetJsTypeSet() const
    {
        return this->jsTypeSet;
    }

    void SetJsTypeSet(Js::EquivalentTypeSet * value)
    {
        Assert(!this->isShared);
        this->jsTypeSet = value;
    }

    bool GetIsShared() const { return this->isShared; }
    void SetIsShared() { this->isShared = true; }
};

class ArrayValueInfo : public ValueInfo
{
private:
    StackSym *const headSegmentSym;
    StackSym *const headSegmentLengthSym;
    StackSym *const lengthSym;

private:
    ArrayValueInfo(
        const ValueType valueType,
        StackSym *const headSegmentSym,
        StackSym *const headSegmentLengthSym,
        StackSym *const lengthSym,
        Sym *const symStore = nullptr)
        : ValueInfo(valueType, ValueStructureKind::Array),
        headSegmentSym(headSegmentSym),
        headSegmentLengthSym(headSegmentLengthSym),
        lengthSym(lengthSym)
    {
        Assert(valueType.IsAnyOptimizedArray());
        Assert(!(valueType.IsLikelyTypedArray() && !valueType.IsOptimizedTypedArray()));

        // For typed arrays, the head segment length is the same as the array length. For objects with internal arrays, the
        // length behaves like a regular object's property rather than like an array length.
        Assert(!lengthSym || valueType.IsLikelyArray());
        Assert(!lengthSym || lengthSym != headSegmentLengthSym);

        if(symStore)
        {
            SetSymStore(symStore);
        }
    }

public:
    static ArrayValueInfo *New(
        JitArenaAllocator *const allocator,
        const ValueType valueType,
        StackSym *const headSegmentSym,
        StackSym *const headSegmentLengthSym,
        StackSym *const lengthSym,
        Sym *const symStore = nullptr)
    {
        Assert(allocator);

        return JitAnew(allocator, ArrayValueInfo, valueType, headSegmentSym, headSegmentLengthSym, lengthSym, symStore);
    }

    ValueInfo *Copy(
        JitArenaAllocator *const allocator,
        const bool copyHeadSegment = true,
        const bool copyHeadSegmentLength = true,
        const bool copyLength = true) const
    {
        Assert(allocator);

        return
            copyHeadSegment && headSegmentSym || copyHeadSegmentLength && headSegmentLengthSym || copyLength && lengthSym
                ? New(
                    allocator,
                    Type(),
                    copyHeadSegment ? headSegmentSym : nullptr,
                    copyHeadSegmentLength ? headSegmentLengthSym : nullptr,
                    copyLength ? lengthSym : nullptr,
                    GetSymStore())
                : CopyWithGenericStructureKind(allocator);
    }

public:
    StackSym *HeadSegmentSym() const
    {
        return headSegmentSym;
    }

    StackSym *HeadSegmentLengthSym() const
    {
        return headSegmentLengthSym;
    }

    StackSym *LengthSym() const
    {
        return lengthSym;
    }

    IR::ArrayRegOpnd *CreateOpnd(
        IR::RegOpnd *const previousArrayOpnd,
        const bool needsHeadSegment,
        const bool needsHeadSegmentLength,
        const bool needsLength,
        const bool eliminatedLowerBoundCheck,
        const bool eliminatedUpperBoundCheck,
        Func *const func) const
    {
        Assert(previousArrayOpnd);
        Assert(func);

        return
            IR::ArrayRegOpnd::New(
                previousArrayOpnd,
                Type(),
                needsHeadSegment ? headSegmentSym : nullptr,
                needsHeadSegmentLength ? headSegmentLengthSym : nullptr,
                needsLength ? lengthSym : nullptr,
                eliminatedLowerBoundCheck,
                eliminatedUpperBoundCheck,
                func);
    }
};

class ExprAttributes
{
protected:
    uint32 attributes;

public:
    ExprAttributes(const uint32 attributes = 0) : attributes(attributes)
    {
    }

    uint32 Attributes() const
    {
        return attributes;
    }

private:
    static const uint32 BitMask(const uint index)
    {
        return 1u << index;
    }

protected:
    void SetBitAttribute(const uint index, const bool bit)
    {
        if(bit)
        {
            attributes |= BitMask(index);
        }
        else
        {
            attributes &= ~BitMask(index);
        }
    }
};

class IntMathExprAttributes : public ExprAttributes
{
private:
    static const uint IgnoredIntOverflowIndex = 0;
    static const uint IgnoredNegativeZeroIndex = 1;

public:
    IntMathExprAttributes(const ExprAttributes &exprAttributes) : ExprAttributes(exprAttributes)
    {
    }

    IntMathExprAttributes(const bool ignoredIntOverflow, const bool ignoredNegativeZero)
    {
        SetBitAttribute(IgnoredIntOverflowIndex, ignoredIntOverflow);
        SetBitAttribute(IgnoredNegativeZeroIndex, ignoredNegativeZero);
    }
};

class DstIsIntOrNumberAttributes : public ExprAttributes
{
private:
    static const uint DstIsIntOnlyIndex = 0;
    static const uint DstIsNumberOnlyIndex = 1;

public:
    DstIsIntOrNumberAttributes(const ExprAttributes &exprAttributes) : ExprAttributes(exprAttributes)
    {
    }

    DstIsIntOrNumberAttributes(const bool dstIsIntOnly, const bool dstIsNumberOnly)
    {
        SetBitAttribute(DstIsIntOnlyIndex, dstIsIntOnly);
        SetBitAttribute(DstIsNumberOnlyIndex, dstIsNumberOnly);
    }
};


class ExprHash
{
public:
    ExprHash() { this->opcode = 0; }
    ExprHash(int init) { Assert(init == 0); this->opcode = 0; }

    void Init(Js::OpCode opcode, ValueNumber src1Val, ValueNumber src2Val, ExprAttributes exprAttributes)
    {
        extern uint8 OpCodeToHash[(int)Js::OpCode::Count];

        uint32 opCodeHash = OpCodeToHash[(int)opcode];
        this->opcode = opCodeHash;
        this->src1Val = src1Val;
        this->src2Val = src2Val;
        this->attributes = exprAttributes.Attributes();

        // Assert too many opcodes...
        AssertMsg(this->opcode == (uint32)opCodeHash, "Opcode value too large for CSEs");
        AssertMsg(this->attributes == exprAttributes.Attributes(), "Not enough bits for expr attributes");

        // If value numbers are too large, just give up
        if (this->src1Val != src1Val || this->src2Val != src2Val)
        {
            this->opcode = 0;
            this->src1Val = 0;
            this->src2Val = 0;
            this->attributes = 0;
        }
    }

    Js::OpCode  GetOpcode()             { return (Js::OpCode)this->opcode; }
    ValueNumber GetSrc1ValueNumber()    { return this->src1Val; }
    ValueNumber GetSrc2ValueNumber()    { return this->src2Val; }
    ExprAttributes GetExprAttributes()  { return this->attributes; }
    bool        IsValid()               { return this->opcode != 0; }

    operator    uint()                  { return *(uint*)this; }

private:
    uint32  opcode: 8;
    uint32  src1Val: 11;
    uint32  src2Val: 11;
    uint32  attributes: 2;
};

enum class PathDependentRelationship : uint8
{
    Equal,
    NotEqual,
    GreaterThanOrEqual,
    GreaterThan,
    LessThanOrEqual,
    LessThan
};

class PathDependentInfo
{
private:
    Value *leftValue, *rightValue;
    int32 rightConstantValue;
    PathDependentRelationship relationship;

public:
    PathDependentInfo(const PathDependentRelationship relationship, Value *const leftValue, Value *const rightValue)
        : relationship(relationship), leftValue(leftValue), rightValue(rightValue)
    {
        Assert(leftValue);
        Assert(rightValue);
    }

    PathDependentInfo(
        const PathDependentRelationship relationship,
        Value *const leftValue,
        Value *const rightValue,
        const int32 rightConstantValue)
        : relationship(relationship), leftValue(leftValue), rightValue(rightValue), rightConstantValue(rightConstantValue)
    {
        Assert(leftValue);
    }

public:
    bool HasInfo() const
    {
        return !!leftValue;
    }

    PathDependentRelationship Relationship() const
    {
        Assert(HasInfo());
        return relationship;
    }

    Value *LeftValue() const
    {
        Assert(HasInfo());
        return leftValue;
    }

    Value *RightValue() const
    {
        Assert(HasInfo());
        return rightValue;
    }

    int32 RightConstantValue() const
    {
        Assert(!RightValue());
        return rightConstantValue;
    }
};

class PathDependentInfoToRestore
{
private:
    ValueInfo *leftValueInfo, *rightValueInfo;

public:
    PathDependentInfoToRestore() : leftValueInfo(nullptr), rightValueInfo(nullptr)
    {
    }

    PathDependentInfoToRestore(ValueInfo *const leftValueInfo, ValueInfo *const rightValueInfo)
        : leftValueInfo(leftValueInfo), rightValueInfo(rightValueInfo)
    {
    }

public:
    ValueInfo *LeftValueInfo() const
    {
        return leftValueInfo;
    }

    ValueInfo *RightValueInfo() const
    {
        return rightValueInfo;
    }

public:
    void Clear()
    {
        leftValueInfo = nullptr;
        rightValueInfo = nullptr;
    }
};

typedef JsUtil::List<IR::Opnd *, JitArenaAllocator> OpndList;
typedef JsUtil::BaseDictionary<Sym *, ValueInfo *, JitArenaAllocator> SymToValueInfoMap;
typedef JsUtil::BaseDictionary<SymID, IR::Instr *, JitArenaAllocator> SymIdToInstrMap;
typedef JsUtil::BaseHashSet<Value *, JitArenaAllocator> ValueSet;
typedef JsUtil::BaseHashSet<Value *, JitArenaAllocator, PowerOf2SizePolicy, ValueNumber> ValueSetByValueNumber;
typedef JsUtil::BaseDictionary<SymID, StackSym *, JitArenaAllocator> SymIdToStackSymMap;
typedef JsUtil::Pair<ValueNumber, ValueNumber> ValueNumberPair;
typedef JsUtil::BaseDictionary<ValueNumberPair, Value *, JitArenaAllocator> ValueNumberPairToValueMap;

#include "GlobHashTable.h"

typedef ValueHashTable<Sym *, Value *> GlobHashTable;
typedef HashBucket<Sym *, Value *> GlobHashBucket;

typedef ValueHashTable<ExprHash, Value *> ExprHashTable;
typedef HashBucket<ExprHash, Value *> ExprHashBucket;

struct StackLiteralInitFldData
{
    const Js::PropertyIdArray * propIds;
    uint currentInitFldCount;
};

namespace JsUtil
{
    template <>
    class ValueEntry<StackLiteralInitFldData> : public BaseValueEntry<StackLiteralInitFldData>
    {
    public:
        void Clear()
        {
#if DBG
            this->value.propIds = nullptr;
            this->value.currentInitFldCount = (uint)-1;
#endif
        }
    };
};

typedef JsUtil::BaseDictionary<StackSym *, StackLiteralInitFldData, JitArenaAllocator> StackLiteralInitFldDataMap;

typedef SList<GlobHashBucket*, JitArenaAllocator> PRECandidatesList;

class GlobOptBlockData
{
public:
    GlobOptBlockData(Func *func) :
        symToValueMap(nullptr),
        exprToValueMap(nullptr),
        liveFields(nullptr),
        liveArrayValues(nullptr),
        maybeWrittenTypeSyms(nullptr),
        liveVarSyms(nullptr),
        liveInt32Syms(nullptr),
        liveLossyInt32Syms(nullptr),
        liveFloat64Syms(nullptr),
        liveSimd128F4Syms(nullptr),
        liveSimd128I4Syms(nullptr),
        hoistableFields(nullptr),
        argObjSyms(nullptr),
        maybeTempObjectSyms(nullptr),
        canStoreTempObjectSyms(nullptr),
        valuesToKillOnCalls(nullptr),
        inductionVariables(nullptr),
        availableIntBoundChecks(nullptr),
        startCallCount(0),
        argOutCount(0),
        totalOutParamCount(0),
        callSequence(nullptr),
        hasCSECandidates(false),
        curFunc(func),
        hasDataRef(nullptr),
        stackLiteralInitFldDataMap(nullptr)
    {
    }

    // Data
    GlobHashTable *                         symToValueMap;
    ExprHashTable *                         exprToValueMap;
    BVSparse<JitArenaAllocator> *           liveFields;
    BVSparse<JitArenaAllocator> *           liveArrayValues;
    BVSparse<JitArenaAllocator> *           maybeWrittenTypeSyms;
    BVSparse<JitArenaAllocator> *           isTempSrc;
    BVSparse<JitArenaAllocator> *           liveVarSyms;
    BVSparse<JitArenaAllocator> *           liveInt32Syms;
    // 'liveLossyInt32Syms' includes only syms that contain an int value that may not fully represent the value of the
    // equivalent var sym. The set (liveInt32Syms - liveLossyInt32Syms) includes only syms that contain an int value that fully
    // represents the value of the equivalent var sym, such as when (a + 1) is type-specialized. Among other things, this
    // bit-vector is used, based on the type of conversion that is needed, to determine whether conversion is necessary, and if
    // so, whether a bailout is needed. For instance, after type-specializing (a | 0), the int32 sym of 'a' cannot be reused in
    // (a + 1) during type-specialization. It needs to be converted again using a lossless conversion with a bailout.
    // Conversely, a lossless int32 sym can be reused to avoid a lossy conversion.
    BVSparse<JitArenaAllocator> *           liveLossyInt32Syms;
    BVSparse<JitArenaAllocator> *           liveFloat64Syms;

    // SIMD_JS
    BVSparse<JitArenaAllocator> *           liveSimd128F4Syms;
    BVSparse<JitArenaAllocator> *           liveSimd128I4Syms;

    BVSparse<JitArenaAllocator> *           hoistableFields;
    BVSparse<JitArenaAllocator> *           argObjSyms;
    BVSparse<JitArenaAllocator> *           maybeTempObjectSyms;
    BVSparse<JitArenaAllocator> *           canStoreTempObjectSyms;
    Func *                                  curFunc;

    // 'valuesToKillOnCalls' includes values whose value types need to be killed upon a call. Upon a call, the value types of
    // values in the set are updated and removed from the set as appropriate.
    ValueSet *                              valuesToKillOnCalls;

    InductionVariableSet *                  inductionVariables;
    IntBoundCheckSet *                      availableIntBoundChecks;

    // Bailout data
    uint                                    startCallCount;
    uint                                    argOutCount;
    uint                                    totalOutParamCount;
    SListBase<IR::Opnd *> *                 callSequence;
    StackLiteralInitFldDataMap *            stackLiteralInitFldDataMap;

    uint                                    inlinedArgOutCount;

    bool                                    hasCSECandidates;

private:
    bool *                                  hasDataRef;

public:
    void OnDataInitialized(JitArenaAllocator *const allocator)
    {
        Assert(allocator);

        hasDataRef = JitAnew(allocator, bool, true);
    }

    void OnDataReused(GlobOptBlockData *const fromData)
    {
        // If a block's data is deleted, *hasDataRef will be set to false. Since these two blocks are pointing to the same data,
        // they also need to point to the same has-data info.
        hasDataRef = fromData->hasDataRef;
    }

    void OnDataUnreferenced()
    {
        // Other blocks may still be using the data, we should only un-reference the previous data
        hasDataRef = nullptr;
    }

    void OnDataDeleted()
    {
        if(hasDataRef)
            *hasDataRef = false;
        OnDataUnreferenced();
    }

    bool HasData()
    {
        if(!hasDataRef)
            return false;
        if(*hasDataRef)
            return true;
        OnDataUnreferenced();
        return false;
    }

    // SIMD_JS
    BVSparse<JitArenaAllocator> * GetSimd128LivenessBV(IRType type)
    {
        switch (type)
        {
        case TySimd128F4:
            return liveSimd128F4Syms;
        case TySimd128I4:
            return liveSimd128I4Syms;
        default:
            Assert(UNREACHED);
            return nullptr;
        }
    }
};

typedef JsUtil::BaseDictionary<IntConstType, StackSym *, JitArenaAllocator> IntConstantToStackSymMap;
typedef JsUtil::BaseDictionary<IntConstType, Value *, JitArenaAllocator> IntConstantToValueMap;

typedef JsUtil::BaseDictionary<Js::Var, Value *, JitArenaAllocator> AddrConstantToValueMap;
typedef JsUtil::BaseDictionary<Js::InternalString, Value *, JitArenaAllocator> StringConstantToValueMap;

class JsArrayKills
{
private:
    union
    {
        struct
        {
            bool killsAllArrays : 1;
            bool killsArraysWithNoMissingValues : 1;
            bool killsNativeArrays : 1;
            bool killsArrayHeadSegments : 1;
            bool killsArrayHeadSegmentLengths : 1;
            bool killsArrayLengths : 1;
        };
        byte bits;
    };

public:
    JsArrayKills() : bits(0)
    {
    }

private:
    JsArrayKills(const byte bits) : bits(bits)
    {
    }

public:
    bool KillsAllArrays() const { return killsAllArrays; }
    void SetKillsAllArrays() { killsAllArrays = true; }

    bool KillsArraysWithNoMissingValues() const { return killsArraysWithNoMissingValues; }
    void SetKillsArraysWithNoMissingValues() { killsArraysWithNoMissingValues = true; }

    bool KillsNativeArrays() const { return killsNativeArrays; }
    void SetKillsNativeArrays() { killsNativeArrays = true; }

    bool KillsArrayHeadSegments() const { return killsArrayHeadSegments; }
    void SetKillsArrayHeadSegments() { killsArrayHeadSegments = true; }

    bool KillsArrayHeadSegmentLengths() const { return killsArrayHeadSegmentLengths; }
    void SetKillsArrayHeadSegmentLengths() { killsArrayHeadSegmentLengths = true; }

    bool KillsTypedArrayHeadSegmentLengths() const { return KillsAllArrays(); }

    bool KillsArrayLengths() const { return killsArrayLengths; }
    void SetKillsArrayLengths() { killsArrayLengths = true; }

public:
    bool KillsValueType(const ValueType valueType) const
    {
        Assert(valueType.IsArrayOrObjectWithArray());

        return
            killsAllArrays ||
            killsArraysWithNoMissingValues && valueType.HasNoMissingValues() ||
            killsNativeArrays && !valueType.HasVarElements();
    }

    bool AreSubsetOf(const JsArrayKills &other) const
    {
        return (bits & other.bits) == bits;
    }

    JsArrayKills Merge(const JsArrayKills &other)
    {
        return bits | other.bits;
    }
};

class InvariantBlockBackwardIterator
{
private:
    GlobOpt *const globOpt;
    BasicBlock *const exclusiveEndBlock;
    StackSym *const invariantSym;
    const ValueNumber invariantSymValueNumber;
    BasicBlock *block;
    Value *invariantSymValue;

#if DBG
    BasicBlock *const inclusiveEndBlock;
#endif

public:
    InvariantBlockBackwardIterator(GlobOpt *const globOpt, BasicBlock *const exclusiveBeginBlock, BasicBlock *const inclusiveEndBlock, StackSym *const invariantSym, const ValueNumber invariantSymValueNumber = InvalidValueNumber);

public:
    bool IsValid() const;
    void MoveNext();
    BasicBlock *Block() const;
    Value *InvariantSymValue() const;

    PREVENT_ASSIGN(InvariantBlockBackwardIterator);
};

class GlobOpt
{
private:
    class AddSubConstantInfo;
    class ArrayLowerBoundCheckHoistInfo;
    class ArrayUpperBoundCheckHoistInfo;

    friend BackwardPass;
#if DBG
    friend class ObjectTempVerify;
#endif

private:
    SparseArray<Value>       *  byteCodeConstantValueArray;
    // Global bitvectors
    BVSparse<JitArenaAllocator> * byteCodeConstantValueNumbersBv;

    // Global bitvectors
    IntConstantToStackSymMap *  intConstantToStackSymMap;
    IntConstantToValueMap*      intConstantToValueMap;
    AddrConstantToValueMap *    addrConstantToValueMap;
    StringConstantToValueMap *  stringConstantToValueMap;
#if DBG
    // We can still track the finished stack literal InitFld lexically.
    BVSparse<JitArenaAllocator> * finishedStackLiteralInitFld;
#endif

    BVSparse<JitArenaAllocator> *  byteCodeUses;
    BVSparse<JitArenaAllocator> *  tempBv;            // Bit vector for temporary uses
    BVSparse<JitArenaAllocator> *  objectTypeSyms;
    BVSparse<JitArenaAllocator> *  prePassCopyPropSym;  // Symbols that were copy prop'd during loop prepass

    PropertySym *               propertySymUse;

    BVSparse<JitArenaAllocator> *  lengthEquivBv;
    BVSparse<JitArenaAllocator> *  argumentsEquivBv;

    GlobOptBlockData            blockData;

    JitArenaAllocator *             alloc;
    JitArenaAllocator *             tempAlloc;

    Func *                  func;
    ValueNumber             currentValue;
    BasicBlock *            currentBlock;
    Region *                currentRegion;
    IntOverflowDoesNotMatterRange *intOverflowDoesNotMatterRange;
    Loop *                  prePassLoop;
    Loop *                  rootLoopPrePass;
    uint                    instrCountSinceLastCleanUp;
    SymIdToInstrMap *       prePassInstrMap;
    SymID                   maxInitialSymID;
    bool                    isCallHelper: 1;
    bool                    intOverflowCurrentlyMattersInRange : 1;
    bool                    ignoredIntOverflowForCurrentInstr : 1;
    bool                    ignoredNegativeZeroForCurrentInstr : 1;
    bool                    inInlinedBuiltIn : 1;
    bool                    isRecursiveCallOnLandingPad : 1;
    bool                    updateInductionVariableValueNumber : 1;
    bool                    isPerformingLoopBackEdgeCompensation : 1;

    bool                    doTypeSpec : 1;
    bool                    doAggressiveIntTypeSpec : 1;
    bool                    doAggressiveMulIntTypeSpec : 1;
    bool                    doDivIntTypeSpec : 1;
    bool                    doLossyIntTypeSpec : 1;
    bool                    doFloatTypeSpec : 1;
    bool                    doArrayCheckHoist : 1;
    bool                    doArrayMissingValueCheckHoist : 1;

    bool                    doArraySegmentHoist : 1;
    bool                    doJsArraySegmentHoist : 1;
    bool                    doArrayLengthHoist : 1;
    bool                    doEliminateArrayAccessHelperCall : 1;
    bool                    doTrackRelativeIntBounds : 1;
    bool                    doBoundCheckElimination : 1;
    bool                    doBoundCheckHoist : 1;
    bool                    doLoopCountBasedBoundCheckHoist : 1;

    bool                    isAsmJSFunc : 1;
    OpndList *              noImplicitCallUsesToInsert;

    ValueSetByValueNumber * valuesCreatedForClone;
    ValueNumberPairToValueMap *valuesCreatedForMerge;

#if DBG
    BVSparse<JitArenaAllocator> * byteCodeUsesBeforeOpt;
#endif
public:
    GlobOpt(Func * func);

    void                    Optimize();

    // Return whether the instruction transfer value from the src to the dst for copy prop
    static bool             TransferSrcValue(IR::Instr * instr);

    // Function used by the backward pass as well.
    // GlobOptBailout.cpp
    static void             TrackByteCodeSymUsed(IR::Instr * instr, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed, PropertySym **pPropertySym);

    // GlobOptFields.cpp
    void                    ProcessFieldKills(IR::Instr *instr, BVSparse<JitArenaAllocator> * bv, bool inGlobOpt);
    static bool             DoFieldHoisting(Loop * loop);

    IR::ByteCodeUsesInstr * ConvertToByteCodeUses(IR::Instr * isntr);
    bool GetIsAsmJSFunc()const{ return isAsmJSFunc; };
private:
    bool                    IsLoopPrePass() const { return this->prePassLoop != nullptr; }
    void                    OptBlock(BasicBlock *block);
    void                    BackwardPass(Js::Phase tag);
    void                    ForwardPass();
    void                    OptLoops(Loop *loop);
    void                    TailDupPass();
    bool                    TryTailDup(IR::BranchInstr *tailBranch);
    void                    CleanUpValueMaps();
    PRECandidatesList *     FindBackEdgePRECandidates(BasicBlock *block, JitArenaAllocator *alloc);
    PRECandidatesList *     RemoveUnavailableCandidates(BasicBlock *block, PRECandidatesList *candidates, JitArenaAllocator *alloc);
    PRECandidatesList *     FindPossiblePRECandidates(Loop *loop, JitArenaAllocator *alloc);
    void                    PreloadPRECandidates(Loop *loop, PRECandidatesList *candidates);
    BOOL                    PreloadPRECandidate(Loop *loop, GlobHashBucket* candidate);
    void                    SetLoopFieldInitialValue(Loop *loop, IR::Instr *instr, PropertySym *propertySym, PropertySym *originalPropertySym);
    void                    FieldPRE(Loop *loop);
    void                    MergePredBlocksValueMaps(BasicBlock *block);
    void                    NulloutBlockData(GlobOptBlockData *data);
    void                    InitBlockData();
    void                    ReuseBlockData(GlobOptBlockData *toData, GlobOptBlockData *fromData);
    void                    CopyBlockData(GlobOptBlockData *toData, GlobOptBlockData *fromData);
    void                    CloneBlockData(BasicBlock *const toBlock, BasicBlock *const fromBlock);
    void                    CloneBlockData(BasicBlock *const toBlock, GlobOptBlockData *const toData, BasicBlock *const fromBlock);
    void                    CloneValues(BasicBlock *const toBlock, GlobOptBlockData *toData, GlobOptBlockData *fromData);
    void                    MergeBlockData(GlobOptBlockData *toData, BasicBlock *toBlock, BasicBlock *fromBlock, BVSparse<JitArenaAllocator> *const symsRequiringCompensation, BVSparse<JitArenaAllocator> *const symsCreatedForMerge);
    void                    DeleteBlockData(GlobOptBlockData *data);
    IR::Instr *             OptInstr(IR::Instr *&instr, bool* isInstrCleared);
    Value*                  OptDst(IR::Instr **pInstr, Value *dstVal, Value *src1Val, Value *src2Val, Value *dstIndirIndexVal, Value *src1IndirIndexVal);
    void                    CopyPropDstUses(IR::Opnd *opnd, IR::Instr *instr, Value *src1Val);
    Value *                 OptSrc(IR::Opnd *opnd, IR::Instr * *pInstr, Value **indirIndexValRef = nullptr, IR::IndirOpnd *parentIndirOpnd = nullptr);
    void                    MarkArgumentsUsedForBranch(IR::Instr *inst);
    bool                    OptTagChecks(IR::Instr *instr);
    void                    TryOptimizeInstrWithFixedDataProperty(IR::Instr * * const pInstr);
    bool                    CheckIfPropOpEmitsTypeCheck(IR::Instr *instr, IR::PropertySymOpnd *opnd);
    bool                    FinishOptPropOp(IR::Instr *instr, IR::PropertySymOpnd *opnd, BasicBlock* block = nullptr, bool updateExistingValue = false, bool* emitsTypeCheckOut = nullptr, bool* changesTypeValueOut = nullptr);
    void                    FinishOptHoistedPropOps(Loop * loop);
    IR::Instr *             SetTypeCheckBailOut(IR::Opnd *opnd, IR::Instr *instr, BailOutInfo *bailOutInfo);
    void                    OptArguments(IR::Instr *Instr);
    BOOLEAN                 IsArgumentsOpnd(IR::Opnd* opnd);
    bool                    AreFromSameBytecodeFunc(IR::RegOpnd* src1, IR::RegOpnd* dst);
    void                    TrackArgumentsSym(IR::RegOpnd* opnd);
    void                    ClearArgumentsSym(IR::RegOpnd* opnd);
    BOOLEAN                 TestAnyArgumentsSym();
    BOOLEAN                 IsArgumentsSymID(SymID id, const GlobOptBlockData& blockData);
    Value *                 ValueNumberDst(IR::Instr **pInstr, Value *src1Val, Value *src2Val);
    Value *                 ValueNumberLdElemDst(IR::Instr **pInstr, Value *srcVal);
    ValueType               GetPrepassValueTypeForDst(const ValueType desiredValueType, IR::Instr *const instr, Value *const src1Value, Value *const src2Value, bool *const isValueInfoPreciseRef = nullptr) const;
    bool                    IsPrepassSrcValueInfoPrecise(IR::Opnd *const src, Value *const srcValue) const;
    Value *                 CreateDstUntransferredIntValue(const int32 min, const int32 max, IR::Instr *const instr, Value *const src1Value, Value *const src2Value);
    Value *                 CreateDstUntransferredValue(const ValueType desiredValueType, IR::Instr *const instr, Value *const src1Value, Value *const src2Value);
    Value *                 ValueNumberTransferDst(IR::Instr *const instr, Value *src1Val);
    bool                    IsSafeToTransferInPrePass(IR::Opnd *src, Value *srcValue);
    Value *                 ValueNumberTransferDstInPrepass(IR::Instr *const instr, Value *const src1Val);
    Value *                 FindValue(Sym *sym);
    Value *                 FindValue(GlobHashTable *valueNumberMap, Sym *sym);
    ValueNumber             FindValueNumber(GlobHashTable *valueNumberMap, Sym *sym);
    Value *                 FindValueFromHashTable(GlobHashTable *valueNumberMap, SymID symId);
    ValueNumber             FindPropertyValueNumber(GlobHashTable *valueNumberMap, SymID symId);
    Value *                 FindPropertyValue(GlobHashTable *valueNumberMap, SymID symId);
    Value *                 FindObjectTypeValue(StackSym* typeSym);
    Value *                 FindObjectTypeValue(StackSym* typeSym, GlobHashTable *valueNumberMap);
    Value *                 FindObjectTypeValue(SymID typeSymId, GlobHashTable *valueNumberMap);
    Value *                 FindObjectTypeValue(StackSym* typeSym, BasicBlock* block);
    Value*                  FindObjectTypeValue(SymID typeSymId, BasicBlock* block);
    Value *                 FindObjectTypeValue(StackSym* typeSym, GlobHashTable *valueNumberMap, BVSparse<JitArenaAllocator>* liveFields);
    Value *                 FindObjectTypeValue(SymID typeSymId, GlobHashTable *valueNumberMap, BVSparse<JitArenaAllocator>* liveFields);
    Value *                 FindFuturePropertyValue(PropertySym *const propertySym);
    IR::Opnd *              CopyProp(IR::Opnd *opnd, IR::Instr *instr, Value *val, IR::IndirOpnd *parentIndirOpnd = nullptr);
    IR::Opnd *              CopyPropReplaceOpnd(IR::Instr * instr, IR::Opnd * opnd, StackSym * copySym, IR::IndirOpnd *parentIndirOpnd = nullptr);
    StackSym *              GetCopyPropSym(Sym * sym, Value * val);
    StackSym *              GetCopyPropSym(BasicBlock * block, Sym * sym, Value * val);
    void                    MarkTempLastUse(IR::Instr *instr, IR::RegOpnd *regOpnd);

    ValueNumber             NewValueNumber();
    Value *                 NewValue(ValueInfo *const valueInfo);
    Value *                 NewValue(const ValueNumber valueNumber, ValueInfo *const valueInfo);
    Value *                 CopyValue(Value *const value);
    Value *                 CopyValue(Value *const value, const ValueNumber valueNumber);

    Value *                 NewGenericValue(const ValueType valueType);
    Value *                 NewGenericValue(const ValueType valueType, IR::Opnd *const opnd);
    Value *                 NewGenericValue(const ValueType valueType, Sym *const sym);
    Value *                 GetIntConstantValue(const int32 intConst, IR::Instr * instr, IR::Opnd *const opnd = nullptr);
    Value *                 NewIntConstantValue(const int32 intConst, IR::Instr * instr, bool isTaggable);
    ValueInfo *             NewIntRangeValueInfo(const int32 min, const int32 max, const bool wasNegativeZeroPreventedByBailout);
    ValueInfo *             NewIntRangeValueInfo(const ValueInfo *const originalValueInfo, const int32 min, const int32 max) const;
    Value *                 NewIntRangeValue(const int32 min, const int32 max, const bool wasNegativeZeroPreventedByBailout, IR::Opnd *const opnd = nullptr);
    IntBoundedValueInfo *   NewIntBoundedValueInfo(const ValueInfo *const originalValueInfo, const IntBounds *const bounds) const;
    Value *                 NewIntBoundedValue(const ValueType valueType, const IntBounds *const bounds, const bool wasNegativeZeroPreventedByBailout, IR::Opnd *const opnd = nullptr);
    Value *                 NewFloatConstantValue(const FloatConstType floatValue, IR::Opnd *const opnd = nullptr);
    Value *                 GetVarConstantValue(IR::AddrOpnd *addrOpnd);
    Value *                 NewVarConstantValue(IR::AddrOpnd *addrOpnd, bool isString);
    Value *                 HoistConstantLoadAndPropagateValueBackward(Js::Var varConst, IR::Instr * origInstr, Value * value);
    Value *                 NewFixedFunctionValue(Js::JavascriptFunction *functionValue, IR::AddrOpnd *addrOpnd);

    StackSym *              GetTaggedIntConstantStackSym(const int32 intConstantValue) const;
    StackSym *              GetOrCreateTaggedIntConstantStackSym(const int32 intConstantValue) const;
    Sym *                   SetSymStore(ValueInfo *valueInfo, Sym *sym);
    Value *                 InsertNewValue(Value *val, IR::Opnd *opnd);
    Value *                 InsertNewValue(GlobOptBlockData * blockData, Value *val, IR::Opnd *opnd);
    Value *                 SetValue(GlobOptBlockData * blockData, Value *val, IR::Opnd *opnd);
    void                    SetValue(GlobOptBlockData * blockData, Value *val, Sym * sym);
    void                    SetValueToHashTable(GlobHashTable * valueNumberMap, Value *val, Sym *sym);
    IR::Instr *             TypeSpecialization(IR::Instr *instr, Value **pSrc1Val, Value **pSrc2Val, Value **pDstVal, bool *redoTypeSpecRef, bool *const forceInvariantHoistingRef);

    // SIMD_JS
    bool                    TypeSpecializeSimd128(IR::Instr *instr, Value **pSrc1Val, Value **pSrc2Val, Value **pDstVal);
    bool                    Simd128DoTypeSpec(IR::Instr *instr, const Value *src1Val, const Value *src2Val, const Value *dstVal);
    bool                    Simd128CanTypeSpecOpnd(const ValueType opndType, const ValueType expectedType);
    IRType                  GetIRTypeFromValueType(const ValueType &valueType);
    ValueType               GetValueTypeFromIRType(const IRType &type);
    IR::BailOutKind         GetBailOutKindFromValueType(const ValueType &valueType);
    IR::Instr *             GetExtendedArg(IR::Instr *instr);


    IR::Instr *             OptNewScObject(IR::Instr** instrPtr, Value* srcVal);
    bool                    OptConstFoldBinary(IR::Instr * *pInstr, const IntConstantBounds &src1IntConstantBounds, const IntConstantBounds &src2IntConstantBounds, Value **pDstVal);
    bool                    OptConstFoldUnary(IR::Instr * *pInstr, const int32 intConstantValue, const bool isUsingOriginalSrc1Value, Value **pDstVal);
    bool                    OptConstPeep(IR::Instr *instr, IR::Opnd *constSrc, Value **pDstVal, ValueInfo *vInfo);
    bool                    OptConstFoldBranch(IR::Instr *instr, Value *src1Val, Value*src2Val, Value **pDstVal);
    Js::Var                 GetConstantVar(IR::Opnd *opnd, Value *val);
    bool                    IsWorthSpecializingToInt32DueToSrc(IR::Opnd *const src, Value *const val);
    bool                    IsWorthSpecializingToInt32DueToDst(IR::Opnd *const dst);
    bool                    IsWorthSpecializingToInt32(IR::Instr *const instr, Value *const src1Val, Value *const src2Val = nullptr);
    bool                    TypeSpecializeNumberUnary(IR::Instr *instr, Value *src1Val, Value **pDstVal);
    bool                    TypeSpecializeIntUnary(IR::Instr **pInstr, Value **pSrc1Val, Value **pDstVal, int32 min, int32 max, Value *const src1OriginalVal, bool *redoTypeSpecRef, bool skipDst = false);
    bool                    TypeSpecializeIntBinary(IR::Instr **pInstr, Value *src1Val, Value *src2Val, Value **pDstVal, int32 min, int32 max, bool skipDst = false);
    void                    TypeSpecializeInlineBuiltInUnary(IR::Instr **pInstr, Value **pSrc1Val, Value **pDstVal, Value *const src1OriginalVal, bool *redoTypeSpecRef);
    void                    TypeSpecializeInlineBuiltInBinary(IR::Instr **pInstr, Value *src1Val, Value* src2Val, Value **pDstVal, Value *const src1OriginalVal, Value *const src2OriginalVal);
    void                    TypeSpecializeInlineBuiltInDst(IR::Instr **pInstr, Value **pDstVal);
    bool                    TypeSpecializeUnary(IR::Instr **pInstr, Value **pSrc1Val, Value **pDstVal, Value *const src1OriginalVal, bool *redoTypeSpecRef, bool *const forceInvariantHoistingRef);
    bool                    TypeSpecializeBinary(IR::Instr **pInstr, Value **pSrc1Val, Value **pSrc2Val, Value **pDstVal, Value *const src1OriginalVal, Value *const src2OriginalVal, bool *redoTypeSpecRef);
    bool                    TypeSpecializeFloatUnary(IR::Instr **pInstr, Value *src1Val, Value **pDstVal, bool skipDst = false);
    bool                    TypeSpecializeFloatBinary(IR::Instr *instr, Value *src1Val, Value *src2Val, Value **pDstVal);
    void                    TypeSpecializeFloatDst(IR::Instr *instr, Value *valToTransfer, Value *const src1Value, Value *const src2Value, Value **pDstVal);
    bool                    TypeSpecializeLdLen(IR::Instr * *const instrRef, Value * *const src1ValueRef, Value * *const dstValueRef, bool *const forceInvariantHoistingRef);
    void                    TypeSpecializeIntDst(IR::Instr* instr, Js::OpCode originalOpCode, Value* valToTransfer, Value *const src1Value, Value *const src2Value, const IR::BailOutKind bailOutKind, int32 newMin, int32 newMax, Value** pDstVal, const AddSubConstantInfo *const addSubConstantInfo = nullptr);
    void                    TypeSpecializeIntDst(IR::Instr* instr, Js::OpCode originalOpCode, Value* valToTransfer, Value *const src1Value, Value *const src2Value, const IR::BailOutKind bailOutKind, ValueType valueType, Value** pDstVal, const AddSubConstantInfo *const addSubConstantInfo = nullptr);
    void                    TypeSpecializeIntDst(IR::Instr* instr, Js::OpCode originalOpCode, Value* valToTransfer, Value *const src1Value, Value *const src2Value, const IR::BailOutKind bailOutKind, ValueType valueType, int32 newMin, int32 newMax, Value** pDstVal, const AddSubConstantInfo *const addSubConstantInfo = nullptr);

    bool                    TryTypeSpecializeUnaryToFloatHelper(IR::Instr** pInstr, Value** pSrc1Val, Value* const src1OriginalVal, Value **pDstVal);
    bool                    TypeSpecializeBailoutExpectedInteger(IR::Instr* instr, Value* src1Val, Value** dstVal);
    bool                    TypeSpecializeStElem(IR::Instr **pInstr, Value *src1Val, Value **pDstVal);
    bool                    TryGetIntConstIndexValue(IR::Instr *instr, IR::IndirOpnd *indirOpnd, bool checkSym, int32 *pValue, bool *isNotInt);
    bool                    ShouldExpectConventionalArrayIndexValue(IR::IndirOpnd *const indirOpnd);
    ValueType               GetDivValueType(IR::Instr* instr, Value* src1Val, Value* src2Val, bool specialize);

    bool                    CollectMemOpInfo(IR::Instr *, Value *, Value *);
    bool                    CollectMemOpStElementI(IR::Instr *, Loop *);
    bool                    CollectMemsetStElementI(IR::Instr *, Loop *);
    bool                    CollectMemcopyStElementI(IR::Instr *, Loop *);
    bool                    CollectMemOpLdElementI(IR::Instr *, Loop *);
    bool                    CollectMemcopyLdElementI(IR::Instr *, Loop *);
    SymID                   GetVarSymID(StackSym *);
    const InductionVariable* GetInductionVariable(SymID, Loop *);
    bool                    IsSymIDInductionVariable(SymID, Loop *);
    bool                    IsAllowedForMemOpt(IR::Instr* instr, bool isMemset, IR::RegOpnd *baseOpnd, IR::Opnd *indexOpnd);

    void                    ProcessMemOp();
    bool                    InspectInstrForMemSetCandidate(Loop* loop, IR::Instr* instr, struct MemSetEmitData* emitData, bool& errorInInstr);
    bool                    InspectInstrForMemCopyCandidate(Loop* loop, IR::Instr* instr, struct MemCopyEmitData* emitData, bool& errorInInstr);
    bool                    ValidateMemOpCandidates(Loop * loop, struct MemOpEmitData** emitData, int& iEmitData);
    void                    HoistHeadSegmentForMemOp(IR::Instr *instr, IR::ArrayRegOpnd *arrayRegOpnd, IR::Instr *insertBeforeInstr);
    void                    EmitMemop(Loop * loop, LoopCount *loopCount, const struct MemOpEmitData* emitData);
    IR::Opnd*               GenerateInductionVariableChangeForMemOp(Loop *loop, byte unroll, IR::Instr *insertBeforeInstr = nullptr);
    IR::RegOpnd*            GenerateStartIndexOpndForMemop(Loop *loop, IR::Opnd *indexOpnd, IR::Opnd *sizeOpnd, bool isInductionVariableChangeIncremental, bool bIndexAlreadyChanged, IR::Instr *insertBeforeInstr = nullptr);
    LoopCount*              GetOrGenerateLoopCountForMemOp(Loop *loop);
    IR::Instr*              FindUpperBoundsCheckInstr(IR::Instr* instr);
    IR::Instr*              FindArraySegmentLoadInstr(IR::Instr* instr);
    void                    RemoveMemOpSrcInstr(IR::Instr* memopInstr, IR::Instr* srcInstr, BasicBlock* block);
    void                    GetMemOpSrcInfo(Loop* loop, IR::Instr* instr, IR::RegOpnd*& base, IR::RegOpnd*& index, IRType& arrayType);
    bool                    DoMemOp(Loop * loop);

private:
    void                    ChangeValueType(BasicBlock *const block, Value *const value, const ValueType newValueType, const bool preserveSubclassInfo, const bool allowIncompatibleType = false) const;
    void                    ChangeValueInfo(BasicBlock *const block, Value *const value, ValueInfo *const newValueInfo, const bool allowIncompatibleType = false) const;
    bool                    AreValueInfosCompatible(const ValueInfo *const v0, const ValueInfo *const v1) const;

private:
    void                    VerifyArrayValueInfoForTracking(const ValueInfo *const valueInfo, const bool isJsArray, const BasicBlock *const block, const bool ignoreKnownImplicitCalls = false) const;
    void                    TrackNewValueForKills(Value *const value);
    void                    DoTrackNewValueForKills(Value *const value);
    void                    TrackCopiedValueForKills(Value *const value);
    void                    DoTrackCopiedValueForKills(Value *const value);
    void                    TrackMergedValueForKills(Value *const value, GlobOptBlockData *const blockData, BVSparse<JitArenaAllocator> *const mergedValueTypesTrackedForKills) const;
    void                    DoTrackMergedValueForKills(Value *const value, GlobOptBlockData *const blockData, BVSparse<JitArenaAllocator> *const mergedValueTypesTrackedForKills) const;
    void                    TrackValueInfoChangeForKills(BasicBlock *const block, Value *const value, ValueInfo *const newValueInfo) const;
    void                    ProcessValueKills(IR::Instr *const instr);
    void                    ProcessValueKills(BasicBlock *const block, GlobOptBlockData *const blockData);
    void                    ProcessValueKillsForLoopHeaderAfterBackEdgeMerge(BasicBlock *const block, GlobOptBlockData *const blockData);
    bool                    NeedBailOnImplicitCallForLiveValues(BasicBlock *const block, const bool isForwardPass) const;
    IR::Instr*              CreateBoundsCheckInstr(IR::Opnd* lowerBound, IR::Opnd* upperBound, int offset, Func* func);
    IR::Instr*              CreateBoundsCheckInstr(IR::Opnd* lowerBound, IR::Opnd* upperBound, int offset, IR::BailOutKind bailoutkind, BailOutInfo* bailoutInfo, Func* func);
    IR::Instr*              AttachBoundsCheckData(IR::Instr* instr, IR::Opnd* lowerBound, IR::Opnd* upperBound, int offset);
    void                    OptArraySrc(IR::Instr * *const instrRef);

private:
    void                    TrackIntSpecializedAddSubConstant(IR::Instr *const instr, const AddSubConstantInfo *const addSubConstantInfo, Value *const dstValue, const bool updateSourceBounds);
    void                    CloneBoundCheckHoistBlockData(BasicBlock *const toBlock, GlobOptBlockData *const toData, BasicBlock *const fromBlock, GlobOptBlockData *const fromData);
    void                    MergeBoundCheckHoistBlockData(BasicBlock *const toBlock, GlobOptBlockData *const toData, BasicBlock *const fromBlock, GlobOptBlockData *const fromData);
    void                    DetectUnknownChangesToInductionVariables(GlobOptBlockData *const blockData);
    void                    SetInductionVariableValueNumbers(GlobOptBlockData *const blockData);
    void                    FinalizeInductionVariables(Loop *const loop, GlobOptBlockData *const headerData);
    bool                    DetermineSymBoundOffsetOrValueRelativeToLandingPad(StackSym *const sym, const bool landingPadValueIsLowerBound, ValueInfo *const valueInfo, const IntBounds *const bounds, GlobHashTable *const landingPadSymToValueMap, int *const boundOffsetOrValueRef);

private:
    void                    DetermineDominatingLoopCountableBlock(Loop *const loop, BasicBlock *const headerBlock);
    void                    DetermineLoopCount(Loop *const loop);
    void                    GenerateLoopCount(Loop *const loop, LoopCount *const loopCount);
    void                    GenerateLoopCountPlusOne(Loop *const loop, LoopCount *const loopCount);
    void                    GenerateSecondaryInductionVariableBound(Loop *const loop, StackSym *const inductionVariableSym, const LoopCount *const loopCount, const int maxMagnitudeChange, StackSym *const boundSym);

private:
    void                    DetermineArrayBoundCheckHoistability(bool needLowerBoundCheck, bool needUpperBoundCheck, ArrayLowerBoundCheckHoistInfo &lowerHoistInfo, ArrayUpperBoundCheckHoistInfo &upperHoistInfo, const bool isJsArray, StackSym *const indexSym, Value *const indexValue, const IntConstantBounds &indexConstantBounds, StackSym *const headSegmentLengthSym, Value *const headSegmentLengthValue, const IntConstantBounds &headSegmentLengthConstantBounds, Loop *const headSegmentLengthInvariantLoop, bool &failedToUpdateCompatibleLowerBoundCheck, bool &failedToUpdateCompatibleUpperBoundCheck);

private:
    void                    CaptureNoImplicitCallUses(IR::Opnd *opnd, const bool usesNoMissingValuesInfo, IR::Instr *const includeCurrentInstr = nullptr);
    void                    InsertNoImplicitCallUses(IR::Instr *const instr);
    void                    PrepareLoopArrayCheckHoist();

public:
    JsArrayKills            CheckJsArrayKills(IR::Instr *const instr);

private:
    bool                    IsOperationThatLikelyKillsJsArraysWithNoMissingValues(IR::Instr *const instr);
    bool                    NeedBailOnImplicitCallForArrayCheckHoist(BasicBlock *const block, const bool isForwardPass) const;

private:
    bool                    PrepareForIgnoringIntOverflow(IR::Instr *const instr);
    void                    VerifyIntSpecForIgnoringIntOverflow(IR::Instr *const instr);

    void                    PreLowerCanonicalize(IR::Instr *instr, Value **pSrc1Val, Value **pSrc2Val);
    void                    ProcessKills(IR::Instr *instr);
    void                    MergeValueMaps(GlobOptBlockData *toData, BasicBlock *toBlock, BasicBlock *fromBlock, BVSparse<JitArenaAllocator> *const symsRequiringCompensation, BVSparse<JitArenaAllocator> *const symsCreatedForMerge);
    Value *                 MergeValues(Value *toDataValueMap, Value *fromDataValueMap, Sym *fromDataSym, GlobOptBlockData *toData, GlobOptBlockData *fromData, bool isLoopBackEdge, BVSparse<JitArenaAllocator> *const symsRequiringCompensation, BVSparse<JitArenaAllocator> *const symsCreatedForMerge);
    ValueInfo *             MergeValueInfo(Value *toDataVal, Value *fromDataVal, Sym *fromDataSym, GlobOptBlockData *fromData, bool isLoopBackEdge, bool sameValueNumber, BVSparse<JitArenaAllocator> *const symsRequiringCompensation, BVSparse<JitArenaAllocator> *const symsCreatedForMerge);
    ValueInfo *             MergeLikelyIntValueInfo(Value *toDataVal, Value *fromDataVal, const ValueType newValueType);
    JsTypeValueInfo *       MergeJsTypeValueInfo(JsTypeValueInfo * toValueInfo, JsTypeValueInfo * fromValueInfo, bool isLoopBackEdge, bool sameValueNumber);
    ValueInfo *             MergeArrayValueInfo(const ValueType mergedValueType, const ArrayValueInfo *const toDataValueInfo, const ArrayValueInfo *const fromDataValueInfo, Sym *const arraySym, BVSparse<JitArenaAllocator> *const symsRequiringCompensation, BVSparse<JitArenaAllocator> *const symsCreatedForMerge);
    void                    InsertCloneStrs(BasicBlock *toBlock, GlobOptBlockData *toData, GlobOptBlockData *fromData);
    void                    InsertValueCompensation(BasicBlock *const predecessor, const SymToValueInfoMap &symsRequiringCompensationToMergedValueInfoMap);
    IR::Instr *             ToVarUses(IR::Instr *instr, IR::Opnd *opnd, bool isDst, Value *val);
    void                    ToVar(BVSparse<JitArenaAllocator> *bv, BasicBlock *block);
    IR::Instr *             ToVar(IR::Instr *instr, IR::RegOpnd *regOpnd, BasicBlock *block, Value *val, bool needsUpdate);
    void                    ToInt32(BVSparse<JitArenaAllocator> *bv, BasicBlock *block, bool lossy, IR::Instr *insertBeforeInstr = nullptr);
    void                    ToFloat64(BVSparse<JitArenaAllocator> *bv, BasicBlock *block);
    void                    ToTypeSpec(BVSparse<JitArenaAllocator> *bv, BasicBlock *block, IRType toType, IR::BailOutKind bailOutKind = IR::BailOutInvalid, bool lossy = false, IR::Instr *insertBeforeInstr = nullptr);
    IR::Instr *             ToInt32(IR::Instr *instr, IR::Opnd *opnd, BasicBlock *block, Value *val, IR::IndirOpnd *indir, bool lossy);
    IR::Instr *             ToFloat64(IR::Instr *instr, IR::Opnd *opnd, BasicBlock *block, Value *val, IR::IndirOpnd *indir, IR::BailOutKind bailOutKind);
    IR::Instr *             ToTypeSpecUse(IR::Instr *instr, IR::Opnd *opnd, BasicBlock *block, Value *val, IR::IndirOpnd *indir,
        IRType toType, IR::BailOutKind bailOutKind, bool lossy = false, IR::Instr *insertBeforeInstr = nullptr);
    void                    ToVarRegOpnd(IR::RegOpnd *dst, BasicBlock *block);
    void                    ToVarStackSym(StackSym *varSym, BasicBlock *block);
    void                    ToInt32Dst(IR::Instr *instr, IR::RegOpnd *dst, BasicBlock *block);
    void                    ToUInt32Dst(IR::Instr *instr, IR::RegOpnd *dst, BasicBlock *block);
    void                    ToFloat64Dst(IR::Instr *instr, IR::RegOpnd *dst, BasicBlock *block);
    // SIMD_JS
    void                    TypeSpecializeSimd128Dst(IRType type, IR::Instr *instr, Value *valToTransfer, Value *const src1Value, Value **pDstVal);
    void                    ToSimd128Dst(IRType toType, IR::Instr *instr, IR::RegOpnd *dst, BasicBlock *block);

    static BOOL             IsInt32TypeSpecialized(Sym *sym, BasicBlock *block);
    static BOOL             IsInt32TypeSpecialized(Sym *sym, GlobOptBlockData *data);
    static BOOL             IsSwitchInt32TypeSpecialized(IR::Instr * instr, BasicBlock * block);
    static BOOL             IsFloat64TypeSpecialized(Sym *sym, BasicBlock *block);
    static BOOL             IsFloat64TypeSpecialized(Sym *sym, GlobOptBlockData *data);
    // SIMD_JS
    static BOOL             IsSimd128TypeSpecialized(Sym *sym, BasicBlock *block);
    static BOOL             IsSimd128TypeSpecialized(Sym *sym, GlobOptBlockData *data);
    static BOOL             IsSimd128TypeSpecialized(IRType type, Sym *sym, BasicBlock *block);
    static BOOL             IsSimd128TypeSpecialized(IRType type, Sym *sym, GlobOptBlockData *data);
    static BOOL             IsSimd128F4TypeSpecialized(Sym *sym, BasicBlock *block);
    static BOOL             IsSimd128F4TypeSpecialized(Sym *sym, GlobOptBlockData *data);
    static BOOL             IsSimd128I4TypeSpecialized(Sym *sym, BasicBlock *block);
    static BOOL             IsSimd128I4TypeSpecialized(Sym *sym, GlobOptBlockData *data);
    static BOOL             IsLiveAsSimd128(Sym *sym, GlobOptBlockData *data);
    static BOOL             IsLiveAsSimd128F4(Sym *sym, GlobOptBlockData *data);
    static BOOL             IsLiveAsSimd128I4(Sym *sym, GlobOptBlockData *data);

    static BOOL             IsTypeSpecialized(Sym *sym, BasicBlock *block);
    static BOOL             IsTypeSpecialized(Sym *sym, GlobOptBlockData *data);
    static BOOL             IsLive(Sym *sym, BasicBlock *block);
    static BOOL             IsLive(Sym *sym, GlobOptBlockData *data);
    void                    MakeLive(StackSym *const sym, GlobOptBlockData *const blockData, const bool lossy) const;
    void                    OptConstFoldBr(bool test, IR::Instr *instr, Value * intTypeSpecSrc1Val = nullptr, Value * intTypeSpecSrc2Val = nullptr);
    void                    PropagateIntRangeForNot(int32 minimum, int32 maximum, int32 *pNewMin, int32 * pNewMax);
    void                    PropagateIntRangeBinary(IR::Instr *instr, int32 min1, int32 max1,
                            int32 min2, int32 max2, int32 *pNewMin, int32 *pNewMax);
    bool                    OptIsInvariant(IR::Opnd *src, BasicBlock *block, Loop *loop, Value *srcVal, bool isNotTypeSpecConv, bool allowNonPrimitives);
    bool                    OptIsInvariant(Sym *sym, BasicBlock *block, Loop *loop, Value *srcVal, bool isNotTypeSpecConv, bool allowNonPrimitives, Value **loopHeadValRef = nullptr);
    bool                    OptDstIsInvariant(IR::RegOpnd *dst);
    bool                    OptIsInvariant(IR::Instr *instr, BasicBlock *block, Loop *loop, Value *src1Val, Value *src2Val, bool isNotTypeSpecConv, const bool forceInvariantHoisting = false);
    void                    OptHoistInvariant(IR::Instr *instr, BasicBlock *block, Loop *loop, Value *dstVal, Value *const src1Val, bool isNotTypeSpecConv, bool lossy = false);
    bool                    TryHoistInvariant(IR::Instr *instr, BasicBlock *block, Value *dstVal, Value *src1Val, Value *src2Val, bool isNotTypeSpecConv, const bool lossy = false, const bool forceInvariantHoisting = false);
    void                    HoistInvariantValueInfo(ValueInfo *const invariantValueInfoToHoist, Value *const valueToUpdate, BasicBlock *const targetBlock);
public:
    static bool             IsTypeSpecPhaseOff(Func* func);
    static bool             DoAggressiveIntTypeSpec(Func* func);
    static bool             DoLossyIntTypeSpec(Func* func);
    static bool             DoFloatTypeSpec(Func* func);
    static bool             DoStringTypeSpec(Func* func);
    static bool             DoArrayCheckHoist(Func *const func);
    static bool             DoArrayMissingValueCheckHoist(Func *const func);
    static bool             DoArraySegmentHoist(const ValueType baseValueType, Func *const func);
    static bool             DoArrayLengthHoist(Func *const func);
    static bool             DoTypedArrayTypeSpec(Func* func);
    static bool             DoNativeArrayTypeSpec(Func* func);
    static bool             IsSwitchOptEnabled(Func* func);
    static bool             DoEquivObjTypeSpec(Func* func);
    static bool             DoInlineArgsOpt(Func* func);
    static bool             IsPREInstrCandidateLoad(Js::OpCode opcode);
    static bool             IsPREInstrCandidateStore(Js::OpCode opcode);
    static bool             ImplicitCallFlagsAllowOpts(Loop * loop);
    static bool             ImplicitCallFlagsAllowOpts(Func *func);

private:
    bool                    DoConstFold() const;
    bool                    DoTypeSpec() const;
    bool                    DoAggressiveIntTypeSpec() const;
    bool                    DoAggressiveMulIntTypeSpec() const;
    bool                    DoDivIntTypeSpec() const;
    bool                    DoLossyIntTypeSpec() const;
    bool                    DoFloatTypeSpec() const;
    bool                    DoStringTypeSpec() const { return GlobOpt::DoStringTypeSpec(this->func); }
    bool                    DoArrayCheckHoist() const;
    bool                    DoArrayCheckHoist(const ValueType baseValueType, Loop* loop, IR::Instr *const instr = nullptr) const;
    bool                    DoArrayMissingValueCheckHoist() const;
    bool                    DoArraySegmentHoist(const ValueType baseValueType) const;
    bool                    DoTypedArraySegmentLengthHoist(Loop *const loop) const;
    bool                    DoArrayLengthHoist() const;
    bool                    DoEliminateArrayAccessHelperCall() const;
    bool                    DoTypedArrayTypeSpec() const { return GlobOpt::DoTypedArrayTypeSpec(this->func); }
    bool                    DoNativeArrayTypeSpec() const { return GlobOpt::DoNativeArrayTypeSpec(this->func); }
    bool                    DoLdLenIntSpec(IR::Instr *const instr, const ValueType baseValueType) const;
    bool                    IsSwitchOptEnabled() const { return GlobOpt::IsSwitchOptEnabled(this->func); }
    bool                    DoPathDependentValues() const;
    bool                    DoTrackRelativeIntBounds() const;
    bool                    DoBoundCheckElimination() const;
    bool                    DoBoundCheckHoist() const;
    bool                    DoLoopCountBasedBoundCheckHoist() const;

private:
    // GlobOptBailout.cpp
    bool                    MayNeedBailOut(Loop * loop) const;
    static void             TrackByteCodeSymUsed(IR::Opnd * opnd, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed, PropertySym **pPropertySymUse);
    static void             TrackByteCodeSymUsed(IR::RegOpnd * opnd, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed);
    static void             TrackByteCodeSymUsed(StackSym * sym, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed);
    void                    CaptureValues(BasicBlock *block, BailOutInfo * bailOutInfo);
    void                    CaptureValue(BasicBlock *block, StackSym * stackSym, Value * value, BailOutInfo * bailOutInfo);
    void                    CaptureArguments(BasicBlock *block, BailOutInfo * bailOutInfo, JitArenaAllocator *allocator);
    void                    CaptureByteCodeSymUses(IR::Instr * instr);
    IR::ByteCodeUsesInstr * InsertByteCodeUses(IR::Instr * instr, bool includeDef = false);
    void                    TrackCalls(IR::Instr * instr);
    void                    RecordInlineeFrameInfo(IR::Instr* instr);
    void                    EndTrackCall(IR::Instr * instr);
    void                    EndTrackingOfArgObjSymsForInlinee();
    void                    FillBailOutInfo(BasicBlock *block, BailOutInfo *bailOutInfo);

    static void             MarkNonByteCodeUsed(IR::Instr * instr);
    static void             MarkNonByteCodeUsed(IR::Opnd * opnd);

    bool                    IsImplicitCallBailOutCurrentlyNeeded(IR::Instr * instr, Value *src1Val, Value *src2Val);
    bool                    IsImplicitCallBailOutCurrentlyNeeded(IR::Instr * instr, Value *src1Val, Value *src2Val, BasicBlock * block, bool hasLiveFields, bool mayNeedImplicitCallBailOut, bool isForwardPass);
    static bool             IsTypeCheckProtected(const IR::Instr * instr);
    static bool             MayNeedBailOnImplicitCall(const IR::Instr * instr, Value *src1Val, Value *src2Val);
    static bool             MayNeedBailOnImplicitCall(IR::Opnd * opnd, Value *val, bool callsToPrimitive);

    void                    GenerateBailAfterOperation(IR::Instr * *const pInstr, IR::BailOutKind kind);
public:
    void                    GenerateBailAtOperation(IR::Instr * *const pInstr, const IR::BailOutKind bailOutKind);

private:
    IR::Instr *             EnsureBailTarget(Loop * loop);

    // GlobOptFields.cpp
    void                    ProcessFieldKills(IR::Instr * instr);
    void                    KillLiveFields(StackSym * stackSym, BVSparse<JitArenaAllocator> * bv);
    void                    KillLiveFields(PropertySym * propertySym, BVSparse<JitArenaAllocator> * bv);
    void                    KillLiveFields(BVSparse<JitArenaAllocator> *const propertyEquivSet, BVSparse<JitArenaAllocator> *const bv) const;
    void                    KillLiveElems(IR::IndirOpnd * indirOpnd, BVSparse<JitArenaAllocator> * bv, bool inGlobOpt, Func *func);
    void                    KillAllFields(BVSparse<JitArenaAllocator> * bv);
    void                    SetAnyPropertyMayBeWrittenTo();
    void                    AddToPropertiesWrittenTo(Js::PropertyId propertyId);

    bool                    DoFieldCopyProp() const;
    bool                    DoFieldCopyProp(Loop * loop) const;
    bool                    DoFunctionFieldCopyProp() const;

    bool                    DoFieldHoisting() const;
    bool                    DoObjTypeSpec() const;
    bool                    DoObjTypeSpec(Loop * loop) const;
    bool                    DoEquivObjTypeSpec() const { return DoEquivObjTypeSpec(this->func); }
    bool                    DoFieldRefOpts() const { return DoObjTypeSpec(); }
    bool                    DoFieldRefOpts(Loop * loop) const { return DoObjTypeSpec(loop); }
    bool                    DoFieldOpts(Loop * loop) const;
    bool                    DoFieldPRE() const;
    bool                    DoFieldPRE(Loop *loop) const;

    bool                    FieldHoistOptSrc(IR::Opnd *opnd, IR::Instr *instr, PropertySym * propertySym);
    void                    FieldHoistOptDst(IR::Instr * instr, PropertySym * propertySym, Value * src1Val);

    bool                    TrackHoistableFields() const;
    void                    PreparePrepassFieldHoisting(Loop * loop);
    void                    PrepareFieldHoisting(Loop * loop);
    void                    CheckFieldHoistCandidate(IR::Instr * instr, PropertySym * sym);
    Loop *                  FindFieldHoistStackSym(Loop * startLoop, SymID propertySymId, StackSym ** copySym, IR::Instr * instrToHoist = nullptr) const;
    bool                    CopyPropHoistedFields(PropertySym * sym, IR::Opnd ** ppOpnd, IR::Instr * instr);
    void                    HoistFieldLoad(PropertySym * sym, Loop * loop, IR::Instr * instr, Value * oldValue, Value * newValue);
    void                    HoistNewFieldLoad(PropertySym * sym, Loop * loop, IR::Instr * instr, Value * oldValue, Value * newValue);
    void                    GenerateHoistFieldLoad(PropertySym * sym, Loop * loop, IR::Instr * instr, StackSym * newStackSym, Value * oldValue, Value * newValue);
    void                    HoistFieldLoadValue(Loop * loop, Value * newValue, SymID symId, Js::OpCode opcode, IR::Opnd * srcOpnd);
    void                    ReloadFieldHoistStackSym(IR::Instr * instr, PropertySym * propertySym);
    void                    CopyStoreFieldHoistStackSym(IR::Instr * storeFldInstr, PropertySym * sym, Value * src1Val);
    Value *                 CreateFieldSrcValue(PropertySym * sym, PropertySym * originalSym, IR::Opnd **ppOpnd, IR::Instr * instr);

    static bool             HasHoistableFields(BasicBlock * block);
    static bool             HasHoistableFields(GlobOptBlockData const * globOptData);
    bool                    IsHoistablePropertySym(SymID symId) const;
    bool                    NeedBailOnImplicitCallWithFieldOpts(Loop *loop, bool hasLiveFields) const;
    IR::Instr *             EnsureDisableImplicitCallRegion(Loop * loop);
    void                    UpdateObjPtrValueType(IR::Opnd * opnd, IR::Instr * instr);

    bool                    TrackArgumentsObject();
    void                    CannotAllocateArgumentsObjectOnStack();

#if DBG
    bool                    IsPropertySymId(SymID symId) const;
    bool                    IsHoistedPropertySym(PropertySym * sym) const;
    bool                    IsHoistedPropertySym(SymID symId, Loop * loop) const;

    static void             AssertCanCopyPropOrCSEFieldLoad(IR::Instr * instr);
#endif

    StackSym *              EnsureObjectTypeSym(StackSym * objectSym);
    PropertySym *           EnsurePropertyWriteGuardSym(PropertySym * propertySym);
    void                    PreparePropertySymForTypeCheckSeq(PropertySym *propertySym);
    bool                    IsPropertySymPreparedForTypeCheckSeq(PropertySym *propertySym);
    bool                    PreparePropertySymOpndForTypeCheckSeq(IR::PropertySymOpnd *propertySymOpnd, IR::Instr * instr, Loop *loop);
    static bool             AreTypeSetsIdentical(Js::EquivalentTypeSet * leftTypeSet, Js::EquivalentTypeSet * rightTypeSet);
    static bool             IsSubsetOf(Js::EquivalentTypeSet * leftTypeSet, Js::EquivalentTypeSet * rightTypeSet);
    bool                    ProcessPropOpInTypeCheckSeq(IR::Instr* instr, IR::PropertySymOpnd *opnd);
    bool                    CheckIfInstrInTypeCheckSeqEmitsTypeCheck(IR::Instr* instr, IR::PropertySymOpnd *opnd);
    template<bool makeChanges>
    bool                    ProcessPropOpInTypeCheckSeq(IR::Instr* instr, IR::PropertySymOpnd *opnd, BasicBlock* block, bool updateExistingValue, bool* emitsTypeCheckOut = nullptr, bool* changesTypeValueOut = nullptr, bool *isObjTypeChecked = nullptr);
    void                    KillObjectHeaderInlinedTypeSyms(BasicBlock *block, bool isObjTypeSpecialized, SymID symId = (SymID)-1);
    void                    ValueNumberObjectType(IR::Opnd *dstOpnd, IR::Instr *instr);
    void                    SetSingleTypeOnObjectTypeValue(Value* value, const Js::Type* type);
    void                    SetTypeSetOnObjectTypeValue(Value* value, Js::EquivalentTypeSet* typeSet);
    void                    UpdateObjectTypeValue(Value* value, const Js::Type* type, bool setType, Js::EquivalentTypeSet* typeSet, bool setTypeSet);
    void                    SetObjectTypeFromTypeSym(StackSym *typeSym, Value* value, BasicBlock* block = nullptr);
    void                    SetObjectTypeFromTypeSym(StackSym *typeSym, const Js::Type *type, Js::EquivalentTypeSet * typeSet, BasicBlock* block = nullptr, bool updateExistingValue = false);
    void                    SetObjectTypeFromTypeSym(StackSym *typeSym, const Js::Type *type, Js::EquivalentTypeSet * typeSet, GlobOptBlockData *blockData, bool updateExistingValue = false);
    void                    KillObjectType(StackSym *objectSym, BVSparse<JitArenaAllocator>* liveFields = nullptr);
    void                    KillAllObjectTypes(BVSparse<JitArenaAllocator>* liveFields = nullptr);
    void                    EndFieldLifetime(IR::SymOpnd *symOpnd);
    PropertySym *           CopyPropPropertySymObj(IR::SymOpnd *opnd, IR::Instr *instr);
    static bool             NeedsTypeCheckBailOut(const IR::Instr *instr, IR::PropertySymOpnd *propertySymOpnd, bool isStore, bool* pIsTypeCheckProtected, IR::BailOutKind *pBailOutKind);
    IR::Instr *             PreOptPeep(IR::Instr *instr);
    IR::Instr *             OptPeep(IR::Instr *instr, Value *src1Val, Value *src2Val);
    void                    OptimizeIndirUses(IR::IndirOpnd *indir, IR::Instr * *pInstr, Value **indirIndexValRef);
    void                    RemoveCodeAfterNoFallthroughInstr(IR::Instr *instr);
    void                    ProcessTryCatch(IR::Instr* instr);
    void                    InsertToVarAtDefInTryRegion(IR::Instr * instr, IR::Opnd * dstOpnd);
    void                    RemoveFlowEdgeToCatchBlock(IR::Instr * instr);

    void                    CSEAddInstr(BasicBlock *block, IR::Instr *instr, Value *dstVal, Value *src1Val, Value *src2Val, Value *dstIndirIndexVal, Value *src1IndirIndexVal);
    bool                    CSEOptimize(BasicBlock *block, IR::Instr * *const instrRef, Value **pSrc1Val, Value **pSrc2Val, Value **pSrc1IndirIndexVal, bool intMathExprOnly = false);
    bool                    GetHash(IR::Instr *instr, Value *src1Val, Value *src2Val, ExprAttributes exprAttributes, ExprHash *pHash);
    void                    ProcessArrayValueKills(IR::Instr *instr);
    static bool             NeedBailOnImplicitCallForCSE(BasicBlock *block, bool isForwardPass);
    bool                    DoCSE();
    bool                    CanCSEArrayStore(IR::Instr *instr);

#if DBG_DUMP
    void                    Dump();
    void                    DumpSymToValueMap();
    void                    DumpSymToValueMap(GlobHashTable* symToValueMap);
    void                    DumpSymToValueMap(BasicBlock *block);
    static void             DumpSym(Sym *sym);
    void                    DumpSymVal(int index);

    void                    Trace(BasicBlock * basicBlock, bool before);
    void                    TraceSettings();
#endif

    bool                    IsWorthSpecializingToInt32Branch(IR::Instr * instr, Value * src1Val, Value * src2Val);

    bool                    TryOptConstFoldBrFalse(IR::Instr *const instr, Value *const srcValue, const int32 min, const int32 max);
    bool                    TryOptConstFoldBrEqual(IR::Instr *const instr, const bool branchOnEqual, Value *const src1Value, const int32 min1, const int32 max1, Value *const src2Value, const int32 min2, const int32 max2);
    bool                    TryOptConstFoldBrGreaterThan(IR::Instr *const instr, const bool branchOnGreaterThan, Value *const src1Value, const int32 min1, const int32 max1, Value *const src2Value, const int32 min2, const int32 max2);
    bool                    TryOptConstFoldBrGreaterThanOrEqual(IR::Instr *const instr, const bool branchOnGreaterThanOrEqual, Value *const src1Value, const int32 min1, const int32 max1, Value *const src2Value, const int32 min2, const int32 max2);
    bool                    TryOptConstFoldBrUnsignedLessThan(IR::Instr *const instr, const bool branchOnLessThan, Value *const src1Value, const int32 min1, const int32 max1, Value *const src2Value, const int32 min2, const int32 max2);
    bool                    TryOptConstFoldBrUnsignedGreaterThan(IR::Instr *const instr, const bool branchOnGreaterThan, Value *const src1Value, const int32 min1, const int32 max1, Value *const src2Value, const int32 min2, const int32 max2);

    void                    UpdateIntBoundsForEqualBranch(Value *const src1Value, Value *const src2Value, const int32 src2ConstantValue = 0);
    void                    UpdateIntBoundsForNotEqualBranch(Value *const src1Value, Value *const src2Value, const int32 src2ConstantValue = 0);
    void                    UpdateIntBoundsForGreaterThanOrEqualBranch(Value *const src1Value, Value *const src2Value);
    void                    UpdateIntBoundsForGreaterThanBranch(Value *const src1Value, Value *const src2Value);
    void                    UpdateIntBoundsForLessThanOrEqualBranch(Value *const src1Value, Value *const src2Value);
    void                    UpdateIntBoundsForLessThanBranch(Value *const src1Value, Value *const src2Value);

    IntBounds *             GetIntBoundsToUpdate(const ValueInfo *const valueInfo, const IntConstantBounds &constantBounds, const bool isSettingNewBound, const bool isBoundConstant, const bool isSettingUpperBound, const bool isExplicit);
    ValueInfo *             UpdateIntBoundsForEqual(Value *const value, const IntConstantBounds &constantBounds, Value *const boundValue, const IntConstantBounds &boundConstantBounds, const bool isExplicit);
    ValueInfo *             UpdateIntBoundsForNotEqual(Value *const value, const IntConstantBounds &constantBounds, Value *const boundValue, const IntConstantBounds &boundConstantBounds, const bool isExplicit);
    ValueInfo *             UpdateIntBoundsForGreaterThanOrEqual(Value *const value, const IntConstantBounds &constantBounds, Value *const boundValue, const IntConstantBounds &boundConstantBounds, const bool isExplicit);
    ValueInfo *             UpdateIntBoundsForGreaterThanOrEqual(Value *const value, const IntConstantBounds &constantBounds, Value *const boundValue, const IntConstantBounds &boundConstantBounds, const int boundOffset, const bool isExplicit);
    ValueInfo *             UpdateIntBoundsForGreaterThan(Value *const value, const IntConstantBounds &constantBounds, Value *const boundValue, const IntConstantBounds &boundConstantBounds, const bool isExplicit);
    ValueInfo *             UpdateIntBoundsForLessThanOrEqual(Value *const value, const IntConstantBounds &constantBounds, Value *const boundValue, const IntConstantBounds &boundConstantBounds, const bool isExplicit);
    ValueInfo *             UpdateIntBoundsForLessThanOrEqual(Value *const value, const IntConstantBounds &constantBounds, Value *const boundValue, const IntConstantBounds &boundConstantBounds, const int boundOffset, const bool isExplicit);
    ValueInfo *             UpdateIntBoundsForLessThan(Value *const value, const IntConstantBounds &constantBounds, Value *const boundValue, const IntConstantBounds &boundConstantBounds, const bool isExplicit);

    void                    SetPathDependentInfo(const bool conditionToBranch, const PathDependentInfo &info);
    PathDependentInfoToRestore UpdatePathDependentInfo(PathDependentInfo *const info);
    void                    RestorePathDependentInfo(PathDependentInfo *const info, const PathDependentInfoToRestore infoToRestore);

    IR::Instr *             TrackMarkTempObject(IR::Instr * instrStart, IR::Instr * instrEnd);
    void                    TrackTempObjectSyms(IR::Instr * instr, IR::RegOpnd * opnd);
    IR::Instr *             GenerateBailOutMarkTempObjectIfNeeded(IR::Instr * instr, IR::Opnd * opnd, bool isDst);
    bool                    IsDefinedInCurrentLoopIteration(Loop *loop, Value *val) const;

    void                    KillStateForGeneratorYield();

    static void             InstantiateForceInlinedMembers_GlobOptIntBounds();

    friend class InvariantBlockBackwardIterator;
};
