//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Max string size for ToString, in number of characters including the null terminator
#define VALUE_TYPE_MAX_STRING_SIZE (256)

enum class ObjectType : uint16
{
    #define OBJECT_TYPE(ot) ot,
    #include "ValueTypes.h"
    #undef OBJECT_TYPE
};
ENUM_CLASS_HELPERS(ObjectType, uint16);

extern const char *const ObjectTypeNames[];

class ValueType
{
public:
    typedef uint16 TSize;

private:
    enum class Bits : TSize
    {
        #define VALUE_TYPE_BIT(t, b) t = (b),
        #include "ValueTypes.h"
        #undef VALUE_TYPE_BIT
    };

public:
    #define BASE_VALUE_TYPE(t, b) static const ValueType t;
    #include "ValueTypes.h"
    #undef BASE_VALUE_TYPE

    static const ValueType AnyNumber;

public:
    static void Initialize();

private:
    static Bits BitPattern(const TSize onCount);
    static Bits BitPattern(const TSize onCount, const TSize offCount);

public:
    static ValueType GetTaggedInt();
    static ValueType GetInt(const bool isLikelyTagged);
    static ValueType GetNumberAndLikelyInt(const bool isLikelyTagged);
    static ValueType GetObject(const ObjectType objectType);

    // SIMD_JS
    static ValueType GetSimd128(const ObjectType objectType);

private:
    static ValueType GetArray(const ObjectType objectType);

private:
    union
    {
        // Don't use the following directly because they only apply to specific types. They're mostly for debugger-friendliness.
        struct
        {
            TSize : VALUE_TYPE_OBJECT_BIT_INDEX;
            TSize _objectBit : 1;
        };
        struct
        {
            Bits _nonObjectBits : VALUE_TYPE_COMMON_BIT_COUNT + VALUE_TYPE_NONOBJECT_BIT_COUNT;
        };
        struct
        {
            Bits _objectBits : VALUE_TYPE_COMMON_BIT_COUNT + VALUE_TYPE_OBJECT_BIT_COUNT;
            ObjectType _objectType : sizeof(TSize) * 8 - (VALUE_TYPE_COMMON_BIT_COUNT + VALUE_TYPE_OBJECT_BIT_COUNT); // use remaining bits
        };

        Bits bits;
    };

public:
    ValueType();
private:
    ValueType(const Bits bits);
    static ValueType Verify(const Bits bits);
    static ValueType Verify(const ValueType valueType);

private:
    bool OneOn(const Bits b) const;
    bool AnyOn(const Bits b) const;
    bool AllEqual(const Bits b, const Bits e) const;
    bool AllOn(const Bits b) const;
    bool OneOnOneOff(const Bits on, const Bits off) const;
    bool AllOnAllOff(const Bits on, const Bits off) const;
    bool OneOnOthersOff(const Bits b) const;
    bool OneOnOthersOff(const Bits b, const Bits ignore) const;
    bool AnyOnOthersOff(const Bits b) const;
    bool AnyOnOthersOff(const Bits b, const Bits ignore) const;
    bool AllOnOthersOff(const Bits b) const;
    bool AllOnOthersOff(const Bits b, const Bits ignore) const;
    bool AnyOnExcept(const Bits b) const;

public:
    bool IsUninitialized() const;
    bool IsDefinite() const;

    bool IsTaggedInt() const;
    bool IsIntAndLikelyTagged() const;
    bool IsLikelyTaggedInt() const;

    bool HasBeenUntaggedInt() const;
    bool IsIntAndLikelyUntagged() const;
    bool IsLikelyUntaggedInt() const;

    bool IsNotTaggedValue() const;
    bool CanBeTaggedValue() const;
    ValueType SetCanBeTaggedValue(const bool b) const;

    bool HasBeenInt() const;
    bool IsInt() const;
    bool IsLikelyInt() const;

    bool IsNotInt() const;
    bool IsNotNumber() const;

    bool HasBeenFloat() const;
    bool IsFloat() const;
    bool IsLikelyFloat() const;

    bool HasBeenNumber() const;
    bool IsNumber() const;
    bool IsLikelyNumber() const;

    bool HasBeenUnknownNumber() const;
    bool IsUnknownNumber() const;
    bool IsLikelyUnknownNumber() const;

    bool HasBeenUndefined() const;
    bool IsUndefined() const;
    bool IsLikelyUndefined() const;

    bool HasBeenNull() const;
    bool IsNull() const;
    bool IsLikelyNull() const;

    bool HasBeenBoolean() const;
    bool IsBoolean() const;
    bool IsLikelyBoolean() const;

    bool HasBeenString() const;
    bool IsString() const;
    bool IsLikelyString() const;
    bool IsNotString() const;

    bool HasBeenSymbol() const;
    bool IsSymbol() const;
    bool IsLikelySymbol() const;
    bool IsNotSymbol() const;
    
    bool HasBeenPrimitive() const;
    bool IsPrimitive() const;
    bool IsLikelyPrimitive() const;

#if ENABLE_NATIVE_CODEGEN
// SIMD_JS
    bool IsSimd128() const;
    bool IsSimd128(IRType type) const;
    bool IsSimd128Float32x4() const;
    bool IsSimd128Int32x4() const;
    bool IsSimd128Int8x16() const;
    bool IsSimd128Float64x2() const;

    bool IsLikelySimd128() const;
    bool IsLikelySimd128Float32x4() const;
    bool IsLikelySimd128Int32x4() const;
    bool IsLikelySimd128Int8x16() const;
    bool IsLikelySimd128Float64x2() const;
#endif

    bool HasBeenObject() const;
    bool IsObject() const;
    bool IsLikelyObject() const;
    bool IsNotObject() const;
    bool CanMergeToObject() const;
    bool CanMergeToSpecificObjectType() const;

    bool IsRegExp() const;
    bool IsLikelyRegExp() const;

    bool IsArray() const;
    bool IsLikelyArray() const;
    bool IsNotArray() const;

    bool IsArrayOrObjectWithArray() const;
    bool IsLikelyArrayOrObjectWithArray() const;
    bool IsNotArrayOrObjectWithArray() const;

    bool IsNativeArray() const;
    bool IsLikelyNativeArray() const;
    bool IsNotNativeArray() const;

    bool IsNativeIntArray() const;
    bool IsLikelyNativeIntArray() const;

    bool IsNativeFloatArray() const;
    bool IsLikelyNativeFloatArray() const;

    bool IsTypedArray() const;
    bool IsLikelyTypedArray() const;

    bool IsTypedIntArray() const;
    bool IsLikelyTypedIntArray() const;

    bool IsTypedIntOrFloatArray() const;

    bool IsOptimizedTypedArray() const;
    bool IsLikelyOptimizedTypedArray() const;
    bool IsLikelyOptimizedVirtualTypedArray() const;

    bool IsAnyArrayWithNativeFloatValues() const;
    bool IsLikelyAnyArrayWithNativeFloatValues() const;

    bool IsAnyArray() const;
    bool IsLikelyAnyArray() const;

    bool IsAnyOptimizedArray() const;
    bool IsLikelyAnyOptimizedArray() const;
    bool IsLikelyAnyUnOptimizedArray() const;

    // The following apply to object types only
public:
    ObjectType GetObjectType() const;
private:
    void SetObjectType(const ObjectType objectType);

public:
    ValueType SetIsNotAnyOf(const ValueType other) const;

    // The following apply to javascript array types only
public:
    bool HasNoMissingValues() const;
    ValueType SetHasNoMissingValues(const bool noMissingValues) const;
private:
    bool HasNonInts() const;
    bool HasNonFloats() const;
public:
    bool HasIntElements() const;
    bool HasFloatElements() const;
    bool HasVarElements() const;
    ValueType SetArrayTypeId(const Js::TypeId typeId) const;

public:
    bool IsSubsetOf(const ValueType other, const bool isAggressiveIntTypeSpecEnabled, const bool isFloatSpecEnabled, const bool isArrayMissingValueCheckHoistEnabled, const bool isNativeArrayEnabled) const;

public:
    ValueType ToDefinite() const;
    ValueType ToLikelyUntaggedInt() const;
    ValueType ToDefiniteNumber_PreferFloat() const;
    ValueType ToDefiniteAnyFloat() const;
    ValueType ToDefiniteNumber() const;
    ValueType ToDefiniteAnyNumber() const;
    ValueType ToDefinitePrimitiveSubset() const;
    ValueType ToDefiniteObject() const;
    ValueType ToLikely() const;
    ValueType ToArray() const;
private:
    ValueType ToPrimitiveOrObject() const;

public:
    ValueType Merge(const ValueType other) const;
private:
    ValueType MergeWithObject(const ValueType other) const;

public:
    ValueType Merge(const Js::Var var) const;
private:
    static Bits TypeIdToBits[Js::TypeIds_Limit];
    static Bits VirtualTypeIdToBits[Js::TypeIds_Limit];
    static INT_PTR TypeIdToVtable[Js::TypeIds_Limit];
    static ObjectType VirtualTypedArrayPair[(size_t)ObjectType::Count];
    static ObjectType MixedTypedArrayPair[(size_t)ObjectType::Count];
    static ObjectType MixedTypedToVirtualTypedArray[(size_t)ObjectType::Count];
    static ObjectType TypedArrayMergeMap[(size_t)ObjectType::Count][(size_t)ObjectType::Count];
    static void InitializeTypeIdToBitsMap();
public:
    static ValueType FromTypeId(const Js::TypeId typeId, bool useVirtual);
    static INT_PTR GetVirtualTypedArrayVtable(const Js::TypeId typeId);
    static ValueType FromObject(Js::RecyclableObject *const recyclableObject);
    static ValueType FromObjectWithArray(Js::DynamicObject *const object);
    static ValueType FromObjectArray(Js::JavascriptArray *const objectArray);
    static ValueType FromArray(const ObjectType objectType, Js::JavascriptArray *const array, const Js::TypeId arrayTypeId);
public:
    bool operator ==(const ValueType other) const;
    bool operator !=(const ValueType other) const;
    uint GetHashCode() const;

public:
    template<class F> static void MapInitialDefiniteValueTypesUntil(const F f);
    template<class F> static void MapInitialIndefiniteValueTypesUntil(const F f);
    template<class F> static void MapInitialValueTypesUntil(const F f);

private:
    static const char *const BitNames[];
    static size_t GetLowestBitIndex(const Bits b);
    void ToVerboseString(char (&str)[VALUE_TYPE_MAX_STRING_SIZE]) const;
public:
    void ToString(wchar (&str)[VALUE_TYPE_MAX_STRING_SIZE]) const;
    void ToString(char (&str)[VALUE_TYPE_MAX_STRING_SIZE]) const;
    void ToStringDebug(__out_ecount(strSize) char *const str, const size_t strSize) const;
    static bool FromString(const wchar *const str, ValueType *valueType);
    static bool FromString(const char *const str, ValueType *valueType);

public:
    TSize GetRawData() const;
    static ValueType FromRawData(const TSize rawData);

public:
    bool IsVirtualTypedArrayPair(const ObjectType other) const;
    bool IsLikelyMixedTypedArrayType() const;
    bool IsMixedTypedArrayPair(const ValueType other) const;
    ObjectType GetMixedTypedArrayObjectType() const;
    ObjectType GetMixedToVirtualTypedArrayObjectType() const;
    ValueType ChangeToMixedTypedArrayType() const;
#if DBG
private:
    static void RunUnitTests();
#endif

private:
    static void InstantiateForceInlinedMembers();

    ENUM_CLASS_HELPER_FRIENDS(Bits, TSize);
};

ENUM_CLASS_HELPERS(ValueType::Bits, ValueType::TSize);

struct ValueTypeComparer
{
    static bool Equals(const ValueType t0, const ValueType t1);
    static uint GetHashCode(const ValueType t);
};

template<>
struct DefaultComparer<ValueType> : public ValueTypeComparer
{
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Template function definitions

template<class F> void ValueType::MapInitialDefiniteValueTypesUntil(const F f)
{
    size_t i = 0;

    // Enumerate variations of Int types
    if(f(GetTaggedInt(), i++))
        return;
    if(f(GetInt(true), i++))
        return;
    if(f(GetInt(false), i++))
        return;

    // Enumerate base value types except uninitialized, types covered above, and object types
    const ValueType BaseValueTypes[] =
    {
        #define BASE_VALUE_TYPE(t, b) t,
        #include "ValueTypes.h"
        #undef BASE_VALUE_TYPE
    };
    for(size_t j = 0; j < sizeof(BaseValueTypes) / sizeof(BaseValueTypes[0]); ++j)
    {
        if(BaseValueTypes[j] == Uninitialized || BaseValueTypes[j] == Int || BaseValueTypes[j] == UninitializedObject)
            continue;
        if(f(BaseValueTypes[j], i++))
            return;
    }

    // Enumerate object types
    for(ObjectType objectType = ObjectType::UninitializedObject; objectType < ObjectType::Count; ++objectType)
    {
        if(objectType != ObjectType::ObjectWithArray && objectType != ObjectType::Array)
        {
            if(f(GetObject(objectType), i++))
                return;
            continue;
        }

        // Permute all combinations of array information
        for(Js::TypeId arrayTypeId = Js::TypeIds_ArrayFirst;
            arrayTypeId <= Js::TypeIds_ArrayLast;
            arrayTypeId = static_cast<Js::TypeId>(arrayTypeId + 1))
        {
            if(objectType == ObjectType::ObjectWithArray && arrayTypeId != Js::TypeIds_Array) // objects with native arrays are currently not supported
                continue;
            for(TSize noMissingValues = 0; noMissingValues < 2; ++noMissingValues)
            {
                const ValueType valueType(
                    ValueType::GetObject(objectType)
                        .SetHasNoMissingValues(!!noMissingValues)
                        .SetArrayTypeId(arrayTypeId));
                if(f(valueType, i++))
                    return;
            }
        }
    }
}

template<class F> void ValueType::MapInitialIndefiniteValueTypesUntil(const F f)
{
    if(f(Uninitialized, 0))
        return;
    MapInitialDefiniteValueTypesUntil([&](const ValueType valueType, const size_t i) -> bool
    {
        return f(valueType.ToLikely(), i + 1);
    });
}

template<class F> void ValueType::MapInitialValueTypesUntil(const F f)
{
    if(f(Uninitialized, 0))
        return;
    MapInitialDefiniteValueTypesUntil([&](const ValueType valueType, const size_t i) -> bool
    {
        return f(valueType, i * 2 + 1) || f(valueType.ToLikely(), i * 2 + 2);
    });
}
