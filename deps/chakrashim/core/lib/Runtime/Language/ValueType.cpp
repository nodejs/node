//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#define BASE_VALUE_TYPE(t, b) const ValueType ValueType::##t(b);
#include "ValueTypes.h"
#undef BASE_VALUE_TYPE

const ValueType ValueType::AnyNumber(
    Bits::Int | Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged | Bits::Float | Bits::Number);

void ValueType::Initialize()
{
    InitializeTypeIdToBitsMap();
#if DBG
    RunUnitTests();
#endif
}

__inline ValueType::Bits ValueType::BitPattern(const TSize onCount)
{
    CompileAssert(sizeof(TSize) <= sizeof(size_t));
    Assert(onCount && onCount <= sizeof(TSize) * 8);

#pragma prefast(suppress:6235, "Non-Zero Constant TSize and size_t in Condition. This is By Design to allow for TSize to be increased in size to uint32 without breaking anything")
    return
        static_cast<Bits>(
            sizeof(TSize) < sizeof(size_t) || onCount < sizeof(TSize) * 8
                ? (static_cast<size_t>(1) << onCount) - 1
                : static_cast<size_t>(-1));
}

__inline ValueType::Bits ValueType::BitPattern(const TSize onCount, const TSize offCount)
{
    Assert(onCount && onCount <= sizeof(TSize) * 8);
    Assert(offCount && offCount <= sizeof(TSize) * 8);

    return BitPattern(onCount + offCount) - BitPattern(offCount);
}

ValueType ValueType::GetTaggedInt()
{
    return Verify(Int);
}

ValueType ValueType::GetInt(const bool isLikelyTagged)
{
    Bits intBits = Bits::Int | Bits::IntCanBeUntagged | Bits::CanBeTaggedValue;
    if(!isLikelyTagged)
        intBits |= Bits::IntIsLikelyUntagged;
    return Verify(intBits);
}

ValueType ValueType::GetNumberAndLikelyInt(const bool isLikelyTagged)
{
    return Verify(GetInt(isLikelyTagged).bits | Bits::Number);
}

__inline ValueType ValueType::GetObject(const ObjectType objectType)
{
    ValueType valueType(UninitializedObject);
    valueType.SetObjectType(objectType);
    if(objectType == ObjectType::Array || objectType == ObjectType::ObjectWithArray)
    {
        // Default to the most conservative array-specific information. This just a safeguard to guarantee that the returned
        // value type has a valid set of array-specific information. Callers should not rely on these defaults, and should
        // instead always set each piece of information explicitly.
        valueType = valueType.SetHasNoMissingValues(false).SetArrayTypeId(Js::TypeIds_Array);
    }
    return Verify(valueType);
}

ValueType ValueType::GetSimd128(const ObjectType objectType)
{
    Assert(objectType >= ObjectType::Simd128Float32x4 && objectType <= ObjectType::Simd128Float64x2);
    return GetObject(objectType);
}

__inline ValueType ValueType::GetArray(const ObjectType objectType)
{
    // Should typically use GetObject instead. This function should only be used for performance, when the array info is
    // guaranteed to be updated correctly by the caller.

    Assert(objectType == ObjectType::Array || objectType == ObjectType::ObjectWithArray);

    ValueType valueType(UninitializedObject);
    valueType.SetObjectType(objectType);
    return Verify(valueType);
}

ValueType::ValueType() : bits(Uninitialized.bits)
{
    CompileAssert(sizeof(ValueType) == sizeof(TSize));
    CompileAssert(sizeof(ObjectType) == sizeof(TSize));
}

ValueType::ValueType(const Bits bits) : bits(bits)
{
}

ValueType ValueType::Verify(const Bits bits)
{
    return Verify(ValueType(bits));
}

ValueType ValueType::Verify(const ValueType valueType)
{
    Assert(valueType.bits);
    Assert(!valueType.OneOn(Bits::Object) || valueType.GetObjectType() < ObjectType::Count);
    Assert(
        valueType.OneOn(Bits::Object) ||
        valueType.OneOn(Bits::Int) ||
        valueType.AnyOnExcept(Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged));
    Assert(
        valueType.OneOn(Bits::Object) ||
        !valueType.AllEqual(Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged, Bits::IntIsLikelyUntagged));

    return valueType;
}

bool ValueType::OneOn(const Bits b) const
{
    return AnyOn(b);
}

bool ValueType::AnyOn(const Bits b) const
{
    Assert(b);
    return !!(bits & b);
}

bool ValueType::AllEqual(const Bits b, const Bits e) const
{
    Assert(b);
    return (bits & b) == e;
}

bool ValueType::AllOn(const Bits b) const
{
    return AllEqual(b, b);
}

bool ValueType::OneOnOneOff(const Bits on, const Bits off) const
{
    return AllOnAllOff(on, off);
}

bool ValueType::AllOnAllOff(const Bits on, const Bits off) const
{
    return AllEqual(on | off, on);
}

bool ValueType::OneOnOthersOff(const Bits b) const
{
    return AllOnOthersOff(b);
}

bool ValueType::OneOnOthersOff(const Bits b, const Bits ignore) const
{
    return AllOnOthersOff(b, ignore);
}

bool ValueType::AnyOnOthersOff(const Bits b) const
{
    Assert(b);
    return !(bits & ~b);
}

bool ValueType::AnyOnOthersOff(const Bits b, const Bits ignore) const
{
    Assert(b);
    Assert(ignore);
    Assert(!(b & ignore)); // not necessary for this function to work correctly, but generally not expected

    return AnyOn(b) && AnyOnOthersOff(b | ignore);
}

bool ValueType::AllOnOthersOff(const Bits b) const
{
    Assert(b);
    return bits == b;
}

bool ValueType::AllOnOthersOff(const Bits b, const Bits ignore) const
{
    Assert(b);
    Assert(ignore);
    Assert(!(b & ignore));

    return AllEqual(~ignore, b);
}

bool ValueType::AnyOnExcept(const Bits b) const
{
    Assert(!!b && !!~b);
    return !AnyOn(b);
}

bool ValueType::IsUninitialized() const
{
    return AllOnOthersOff(Bits::Likely, Bits::CanBeTaggedValue);
}

bool ValueType::IsDefinite() const
{
    return !OneOn(Bits::Likely);
}

bool ValueType::IsTaggedInt() const
{
    return AllOnOthersOff(Bits::Int | Bits::CanBeTaggedValue);
}

bool ValueType::IsIntAndLikelyTagged() const
{
    return AllOnOthersOff(Bits::Int | Bits::CanBeTaggedValue, Bits::IntCanBeUntagged);
}

bool ValueType::IsLikelyTaggedInt() const
{
    return AllOnOthersOff(Bits::Int | Bits::CanBeTaggedValue, Bits::Likely | Bits::IntCanBeUntagged | Bits::Number);
}

bool ValueType::HasBeenUntaggedInt() const
{
    return OneOnOneOff(Bits::IntIsLikelyUntagged, Bits::Object);
}

bool ValueType::IsIntAndLikelyUntagged() const
{
    return AllOnOthersOff(Bits::Int | Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged, Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelyUntaggedInt() const
{
    return AllOnOthersOff(Bits::Int | Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged,
                          Bits::Likely | Bits::Number | Bits::CanBeTaggedValue);
}

bool ValueType::IsNotTaggedValue() const
{
    return IsNotNumber() || !OneOn(Bits::CanBeTaggedValue);
}

bool ValueType::CanBeTaggedValue() const
{
    return !IsNotTaggedValue();
}

ValueType ValueType::SetCanBeTaggedValue(const bool b) const
{
    if (b)
    {
        Assert(!IsNotNumber());
        return Verify(bits | Bits::CanBeTaggedValue);
    }
    return Verify(bits & ~Bits::CanBeTaggedValue);
}

bool ValueType::HasBeenInt() const
{
    return OneOnOneOff(Bits::Int, Bits::Object);
}

bool ValueType::IsInt() const
{
    return OneOnOthersOff(Bits::Int, Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged | Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelyInt() const
{
    return OneOnOthersOff(
        Bits::Int,
        Bits::Likely | Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged | Bits::Number | Bits::CanBeTaggedValue);
}

bool ValueType::IsNotInt() const
{
    return
        AnyOnExcept(Bits::Likely | Bits::Object | Bits::Int | Bits::CanBeTaggedValue | Bits::Float | Bits::Number) ||
        OneOnOneOff(Bits::Object, Bits::Likely);
}

bool ValueType::IsNotNumber() const
{
    // These are the same for now.
    return IsNotInt();
}

bool ValueType::HasBeenFloat() const
{
    return OneOnOneOff(Bits::Float, Bits::Object);
}

bool ValueType::IsFloat() const
{
    // TODO: Require that the int bits are off. We can then use (!IsFloat() && IsNumber()) to determine that a tagged int check
    // needs to be done but not a JavascriptNumber/TaggedFloat check.
    return
        OneOnOthersOff(
            Bits::Float,
            (
                Bits::Int |
                Bits::IntCanBeUntagged |
                Bits::IntIsLikelyUntagged |
                Bits::CanBeTaggedValue |
                Bits::Number
            ));
}

bool ValueType::IsLikelyFloat() const
{
    return
        OneOnOthersOff(
            Bits::Float,
            (
                Bits::Likely |
                Bits::Undefined |
                Bits::Int |
                Bits::IntCanBeUntagged |
                Bits::IntIsLikelyUntagged |
                Bits::CanBeTaggedValue |
                Bits::Number
            ));
}

bool ValueType::HasBeenNumber() const
{
    return !OneOn(Bits::Object) && AnyOn(Bits::Int | Bits::Float | Bits::Number);
}

bool ValueType::IsNumber() const
{
    return AnyOnOthersOff(Bits::Int | Bits::Float | Bits::Number,
                          Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged | Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelyNumber() const
{
    return
        AnyOnOthersOff(
            Bits::Int | Bits::Float | Bits::Number,
            Bits::Likely | Bits::Undefined | Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged | Bits::CanBeTaggedValue);
}

bool ValueType::HasBeenUnknownNumber() const
{
    return OneOnOneOff(Bits::Number, Bits::Object);
}

bool ValueType::IsUnknownNumber() const
{
    // Equivalent to IsNumber() && !IsLikelyInt() && !IsLikelyFloat()
    return OneOnOthersOff(Bits::Number, Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelyUnknownNumber() const
{
    // If true, equivalent to IsLikelyNumber() && !IsLikelyInt() && !IsLikelyFloat()
    return OneOnOthersOff(Bits::Number, Bits::Likely | Bits::Undefined | Bits::CanBeTaggedValue);
}

bool ValueType::HasBeenUndefined() const
{
    return OneOn(Bits::Undefined);
}

bool ValueType::IsUndefined() const
{
    return OneOnOthersOff(Bits::Undefined, Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelyUndefined() const
{
    return OneOnOthersOff(Bits::Undefined, Bits::Likely | Bits::CanBeTaggedValue);
}

bool ValueType::HasBeenNull() const
{
    return OneOn(Bits::Null);
}

bool ValueType::IsNull() const
{
    return OneOnOthersOff(Bits::Null, Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelyNull() const
{
    return OneOnOthersOff(Bits::Null, Bits::Likely | Bits::CanBeTaggedValue);
}

bool ValueType::HasBeenBoolean() const
{
    return OneOnOneOff(Bits::Boolean, Bits::Object);
}

bool ValueType::IsBoolean() const
{
    return OneOnOthersOff(Bits::Boolean, Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelyBoolean() const
{
    return OneOnOthersOff(Bits::Boolean, Bits::Likely | Bits::CanBeTaggedValue);
}

bool ValueType::HasBeenString() const
{
    return OneOnOneOff(Bits::String, Bits::Object);
}

bool ValueType::IsString() const
{
    return OneOnOthersOff(Bits::String, Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelyString() const
{
    return OneOnOthersOff(Bits::String, Bits::Likely | Bits::CanBeTaggedValue);
}

bool ValueType::IsNotString() const
{
    return AnyOnExcept(Bits::Likely | Bits::Object | Bits::String | Bits::CanBeTaggedValue)
        || OneOnOneOff(Bits::Object, Bits::Likely);
}

bool ValueType::HasBeenSymbol() const
{
    return OneOnOneOff(Bits::Symbol, Bits::Object);
}

bool ValueType::IsSymbol() const
{
    return OneOnOthersOff(Bits::Symbol, Bits::CanBeTaggedValue);
}

bool ValueType::IsLikelySymbol() const
{
    return OneOnOthersOff(Bits::Symbol, Bits::Likely | Bits::CanBeTaggedValue);
}

bool ValueType::IsNotSymbol() const
{
    return AnyOnExcept(Bits::Likely | Bits::Object | Bits::Symbol | Bits::CanBeTaggedValue)
        || OneOnOneOff(Bits::Object, Bits::Likely);
}

bool ValueType::HasBeenPrimitive() const
{
    return
        OneOn(Bits::Object)
            ?
                AnyOn(Bits::Undefined | Bits::Null)
              || GetObjectType() >= ObjectType::Simd128Float32x4
            :
                AnyOn(
                    Bits::Undefined |
                    Bits::Null |
                    Bits::Int |
                    Bits::Float |
                    Bits::Number |
                    Bits::Boolean |
                    Bits::String |
                    Bits::Symbol |
                    Bits::PrimitiveOrObject);
}

bool ValueType::IsPrimitive() const
{
    bool result =
        AnyOnOthersOff(
            Bits::Undefined | Bits::Null | Bits::Int | Bits::Float | Bits::Number | Bits::Boolean | Bits::String | Bits::Symbol,
            Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged | Bits::CanBeTaggedValue);

#if ENABLE_NATIVE_CODEGEN
    result =  result || IsSimd128();
#endif

    return result;
}

bool ValueType::IsLikelyPrimitive() const
{
    bool result =
        AnyOnOthersOff(
            Bits::Undefined | Bits::Null | Bits::Int | Bits::Float | Bits::Number | Bits::Boolean | Bits::String | Bits::Symbol,
            Bits::Likely | Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged | Bits::CanBeTaggedValue);

#if ENABLE_NATIVE_CODEGEN
    result = result || IsLikelySimd128();
#endif

    return result;
}


bool ValueType::HasBeenObject() const
{
    return AnyOn(Bits::Object | Bits::PrimitiveOrObject);
}

bool ValueType::IsObject() const
{
    return AllOnAllOff(Bits::Object, Bits::Likely | Bits::Undefined | Bits::Null);
}

bool ValueType::IsLikelyObject() const
{
    // For syms that are typically used as objects, they often also have Undefined or Null values, and they are used as objects
    // only after checking for Undefined or Null. So, for the purpose of determining whether a value type is likely object, the
    // Undefined and Null bits are ignored.
    return OneOn(Bits::Object);
}

bool ValueType::IsNotObject() const
{
    return AnyOnExcept(Bits::Likely | Bits::Object | Bits::PrimitiveOrObject);
}

bool ValueType::CanMergeToObject() const
{
    Assert(!IsLikelyObject());
    return AnyOnExcept(BitPattern(VALUE_TYPE_NONOBJECT_BIT_COUNT, VALUE_TYPE_COMMON_BIT_COUNT));
}

bool ValueType::CanMergeToSpecificObjectType() const
{
    return IsLikelyObject() ? GetObjectType() == ObjectType::UninitializedObject : CanMergeToObject();
}

bool ValueType::IsRegExp() const
{
    return IsObject() && GetObjectType() == ObjectType::RegExp;
}

bool ValueType::IsLikelyRegExp() const
{
    return IsLikelyObject() && GetObjectType() == ObjectType::RegExp;
}

bool ValueType::IsArray() const
{
    return IsObject() && GetObjectType() == ObjectType::Array;
}

bool ValueType::IsLikelyArray() const
{
    return IsLikelyObject() && GetObjectType() == ObjectType::Array;
}

bool ValueType::IsNotArray() const
{
    return IsNotObject() || IsObject() && GetObjectType() > ObjectType::Object && GetObjectType() != ObjectType::Array;
}

bool ValueType::IsArrayOrObjectWithArray() const
{
    return IsObject() && (GetObjectType() == ObjectType::ObjectWithArray || GetObjectType() == ObjectType::Array);
}

bool ValueType::IsLikelyArrayOrObjectWithArray() const
{
    return IsLikelyObject() && (GetObjectType() == ObjectType::ObjectWithArray || GetObjectType() == ObjectType::Array);
}

bool ValueType::IsNotArrayOrObjectWithArray() const
{
    return
        IsNotObject() ||
        IsObject() && GetObjectType() != ObjectType::ObjectWithArray && GetObjectType() != ObjectType::Array;
}

bool ValueType::IsNativeArray() const
{
    return IsArrayOrObjectWithArray() && !HasVarElements();
}

bool ValueType::IsLikelyNativeArray() const
{
    return IsLikelyArrayOrObjectWithArray() && !HasVarElements();
}

bool ValueType::IsNotNativeArray() const
{
    return
        IsNotObject() ||
        (
            IsObject() &&
            (GetObjectType() != ObjectType::ObjectWithArray && GetObjectType() != ObjectType::Array || HasVarElements())
        );
}

bool ValueType::IsNativeIntArray() const
{
    return IsArrayOrObjectWithArray() && HasIntElements();
}

bool ValueType::IsLikelyNativeIntArray() const
{
    return IsLikelyArrayOrObjectWithArray() && HasIntElements();
}

bool ValueType::IsNativeFloatArray() const
{
    return IsArrayOrObjectWithArray() && HasFloatElements();
}

bool ValueType::IsLikelyNativeFloatArray() const
{
    return IsLikelyArrayOrObjectWithArray() && HasFloatElements();
}

bool ValueType::IsTypedIntArray() const
{
    return IsObject() && GetObjectType() >= ObjectType::Int8Array && GetObjectType() <= ObjectType::Uint32Array;
}

bool ValueType::IsLikelyTypedIntArray() const
{
    return IsLikelyObject() && GetObjectType() >= ObjectType::Int8Array && GetObjectType() <= ObjectType::Uint32Array;
}

bool ValueType::IsTypedArray() const
{
    return IsObject() && GetObjectType() >= ObjectType::Int8Array && GetObjectType() <= ObjectType::CharArray;
}

bool ValueType::IsLikelyTypedArray() const
{
    return IsLikelyObject() && GetObjectType() >= ObjectType::Int8Array && GetObjectType() <= ObjectType::CharArray;
}

bool ValueType::IsTypedIntOrFloatArray() const
{
    return IsObject() && ((GetObjectType() >= ObjectType::Int8Array  && GetObjectType() <= ObjectType::Float64Array));
}

bool ValueType::IsOptimizedTypedArray() const
{
    return IsObject() && ((GetObjectType() >= ObjectType::Int8Array  && GetObjectType() <= ObjectType::Float64MixedArray));
}

bool ValueType::IsLikelyOptimizedTypedArray() const
{
    return IsLikelyObject() && ((GetObjectType() >= ObjectType::Int8Array  &&  GetObjectType() <= ObjectType::Float64MixedArray));
}

bool ValueType::IsLikelyOptimizedVirtualTypedArray() const
{
    return IsLikelyObject() && (GetObjectType() >= ObjectType::Int8VirtualArray && GetObjectType() <= ObjectType::Float64VirtualArray);
}

bool ValueType::IsAnyArrayWithNativeFloatValues() const
{
    if(!IsObject())
        return false;
    switch(GetObjectType())
    {
        case ObjectType::ObjectWithArray:
        case ObjectType::Array:
            return HasFloatElements();

        case ObjectType::Float32Array:
        case ObjectType::Float32VirtualArray:
        case ObjectType::Float32MixedArray:
        case ObjectType::Float64Array:
        case ObjectType::Float64VirtualArray:
        case ObjectType::Float64MixedArray:
            return true;
    }
    return false;
}

bool ValueType::IsLikelyAnyArrayWithNativeFloatValues() const
{
    if(!IsLikelyObject())
        return false;
    switch(GetObjectType())
    {
        case ObjectType::ObjectWithArray:
        case ObjectType::Array:
            return HasFloatElements();

        case ObjectType::Float32Array:
        case ObjectType::Float32VirtualArray:
        case ObjectType::Float32MixedArray:
        case ObjectType::Float64Array:
        case ObjectType::Float64VirtualArray:
        case ObjectType::Float64MixedArray:
            return true;
    }
    return false;
}

bool ValueType::IsAnyArray() const
{
    return IsObject() && GetObjectType() >= ObjectType::ObjectWithArray && GetObjectType() <= ObjectType::CharArray;
}

bool ValueType::IsLikelyAnyArray() const
{
    return IsLikelyObject() && GetObjectType() >= ObjectType::ObjectWithArray && GetObjectType() <= ObjectType::CharArray;
}

bool ValueType::IsAnyOptimizedArray() const
{
    return IsObject() && ((GetObjectType() >= ObjectType::ObjectWithArray &&  GetObjectType() <= ObjectType::Float64MixedArray));
}

bool ValueType::IsLikelyAnyOptimizedArray() const
{
    return IsLikelyObject() && ((GetObjectType() >= ObjectType::ObjectWithArray && GetObjectType() <= ObjectType::Float64MixedArray));
}

bool ValueType::IsLikelyAnyUnOptimizedArray() const
{
    return IsLikelyObject() && GetObjectType() >= ObjectType::Int64Array && GetObjectType() <= ObjectType::CharArray;
}

#if ENABLE_NATIVE_CODEGEN
// Simd128 values
// Note that SIMD types are primitives
bool ValueType::IsSimd128() const
{
    return IsObject() && (GetObjectType() >= ObjectType::Simd128Float32x4 && GetObjectType() <= ObjectType::Simd128Float64x2);
}

bool ValueType::IsSimd128(IRType type) const
{
    switch (type)
    {
    case TySimd128F4:
        return IsSimd128Float32x4();
    case TySimd128I4:
        return IsSimd128Int32x4();
    case TySimd128D2:
        return IsSimd128Float64x2();
    default:
        Assert(UNREACHED);
        return false;
    }
}

bool ValueType::IsSimd128Float32x4() const
{
    return IsObject() && GetObjectType() == ObjectType::Simd128Float32x4;
}

bool ValueType::IsSimd128Int32x4() const
{
    return IsObject() && GetObjectType() == ObjectType::Simd128Int32x4;
}

bool ValueType::IsSimd128Int8x16() const
{
    return IsObject() && GetObjectType() == ObjectType::Simd128Int8x16;
}

bool ValueType::IsSimd128Float64x2() const
{
    return IsObject() && GetObjectType() == ObjectType::Simd128Float64x2;
}

bool ValueType::IsLikelySimd128() const
{
    return IsLikelyObject() && (GetObjectType() >= ObjectType::Simd128Float32x4 && GetObjectType() <= ObjectType::Simd128Float64x2);
}

bool ValueType::IsLikelySimd128Float32x4() const
{
    return IsLikelyObject() && GetObjectType() == ObjectType::Simd128Float32x4;
}

bool ValueType::IsLikelySimd128Int32x4() const
{
    return IsLikelyObject() && GetObjectType() == ObjectType::Simd128Int32x4;
}

bool ValueType::IsLikelySimd128Int8x16() const
{
    return IsLikelyObject() && GetObjectType() == ObjectType::Simd128Int8x16;
}

bool ValueType::IsLikelySimd128Float64x2() const
{
    return IsLikelyObject() && GetObjectType() == ObjectType::Simd128Float64x2;
}
#endif

ObjectType ValueType::GetObjectType() const
{
    Assert(OneOn(Bits::Object));
    return _objectType;
}

void ValueType::SetObjectType(const ObjectType objectType)
{
    Assert(OneOn(Bits::Object));
    Assert(objectType < ObjectType::Count);

    _objectType = objectType;
}

ValueType ValueType::SetIsNotAnyOf(const ValueType other) const
{
    Assert(other.IsDefinite());
    Assert(!other.HasBeenObject());

    return bits & ~other.bits;
}

bool ValueType::HasNoMissingValues() const
{
    Assert(IsLikelyArrayOrObjectWithArray());
    return OneOn(Bits::NoMissingValues);
}

ValueType ValueType::SetHasNoMissingValues(const bool noMissingValues) const
{
    Assert(IsLikelyArrayOrObjectWithArray());

    if(noMissingValues)
        return Verify(bits | Bits::NoMissingValues);
    return Verify(bits & ~Bits::NoMissingValues);
}

bool ValueType::HasNonInts() const
{
    Assert(IsLikelyArrayOrObjectWithArray());
    return OneOn(Bits::NonInts);
}

bool ValueType::HasNonFloats() const
{
    Assert(IsLikelyArrayOrObjectWithArray());
    return OneOn(Bits::NonFloats);
}

bool ValueType::HasIntElements() const
{
    Assert(IsLikelyArrayOrObjectWithArray());
    return !OneOn(Bits::NonInts);
}

bool ValueType::HasFloatElements() const
{
    Assert(IsLikelyArrayOrObjectWithArray());
    return OneOnOneOff(Bits::NonInts, Bits::NonFloats);
}

bool ValueType::HasVarElements() const
{
    Assert(IsLikelyArrayOrObjectWithArray());
    return AllOn(Bits::NonInts | Bits::NonFloats);
}

ValueType ValueType::SetArrayTypeId(const Js::TypeId typeId) const
{
    using namespace Js;
    Assert(IsLikelyArrayOrObjectWithArray());
    Assert(JavascriptArray::Is(typeId));
    Assert(typeId == TypeIds_Array || IsLikelyObject() && GetObjectType() == ObjectType::Array); // objects with native arrays are currently not supported

    Bits newBits = bits & ~(Bits::NonInts | Bits::NonFloats);
    switch(typeId)
    {
        case TypeIds_Array:
            newBits |= Bits::NonFloats;
            // fall through

        case TypeIds_NativeFloatArray:
            newBits |= Bits::NonInts;
            break;
    }
    return Verify(newBits);
}

bool ValueType::IsSubsetOf(
    const ValueType other,
    const bool isAggressiveIntTypeSpecEnabled,
    const bool isFloatSpecEnabled,
    const bool isArrayMissingValueCheckHoistEnabled,
    const bool isNativeArrayEnabled) const
{
    if(IsUninitialized())
        return other.IsUninitialized();
    if(other.IsUninitialized())
        return true;
    if(IsLikelyNumber() && other.IsLikelyNumber())
    {
        // Special case for numbers since there are multiple combinations of bits and a bit-subset produces incorrect results in
        // some cases

        // When type specialization is enabled, a value type that is likely int or float is considered to be more specific than
        // a value type that is definitely number but unknown as to whether it was int or float, because the former can
        // participate in type specialization while the latter cannot. When type specialization is disabled, the definite value
        // type is considered to be a subset of the indefinite value type because neither will participate in type
        // specialization.
        if(other.IsUnknownNumber() &&
            (isAggressiveIntTypeSpecEnabled && IsLikelyInt() || isFloatSpecEnabled && IsLikelyFloat()))
        {
            return true;
        }

        // The following number types are listed in order of most specific type to least specific type. Types are considered to
        // be subsets of the less specific types below it, with the exception that int types are not considered to be subsets of
        // float types.
        //     TaggedInt
        //     IntAndLikelyTagged
        //     Int
        //     Float
        //     Number

        // This logic doesn't play well with Bits::Undefined, so remove it before proceeding.
        ValueType _this = this->bits & ~Bits::Undefined;
        ValueType _other = other.bits & ~Bits::Undefined;
        return
            (!_this.OneOn(Bits::Likely) || _other.OneOn(Bits::Likely)) &&
            (
                (
                    _this.IsTaggedInt() ||
                    _this.IsLikelyTaggedInt() && !_other.IsTaggedInt() ||
                    _this.IsLikelyInt() && !_other.IsLikelyTaggedInt()
                ) && !_other.IsLikelyFloat() ||
                _this.IsLikelyFloat() && !_other.IsLikelyInt() ||
                _other.IsLikelyUnknownNumber()
            );
    }

    const Bits commonBits = bits & (BitPattern(VALUE_TYPE_COMMON_BIT_COUNT) - Bits::Object);
    if(!!commonBits && !other.AllOn(commonBits))
        return false;
    if(OneOn(Bits::Object))
    {
        if(!other.OneOn(Bits::Object))
            return other.OneOn(Bits::PrimitiveOrObject) || !other.IsDefinite() && other.CanMergeToObject();
    }
    else
    {
        if(!other.OneOn(Bits::Object))
            return other.AllOn(bits);
        return CanMergeToObject();
    }
    if(other.GetObjectType() == ObjectType::UninitializedObject && GetObjectType() != ObjectType::UninitializedObject)
        return true; // object types other than UninitializedObject are a subset of UninitializedObject regardless of the Likely bit
    if(GetObjectType() != other.GetObjectType())
        return false;
    if(!OneOn(Bits::Likely) && other.OneOn(Bits::Likely) ||
        other.GetObjectType() != ObjectType::ObjectWithArray && other.GetObjectType() != ObjectType::Array)
    {
        return true;
    }

    // The following javascript array types are listed in the order of most specific type to least specific type. Types are
    // considered to be subsets of the less specific types below it.
    //     Int32            !HasNonInts() (!HasNonFloats() is implied)
    //     Float64          HasNonInts() && !HasNonFloats()
    //     Var              HasNonFloats() (HasNonInts() is implied)
    return
        (HasNoMissingValues() || !other.HasNoMissingValues() || !isArrayMissingValueCheckHoistEnabled) &&
        (
            (!HasNonInts() || other.HasNonInts()) && (!HasNonFloats() || other.HasNonFloats()) ||
            !isNativeArrayEnabled
        );
}

ValueType ValueType::ToDefinite() const
{
    Assert(!IsUninitialized());
    return Verify(bits & ~Bits::Likely);
}

ValueType ValueType::ToLikelyUntaggedInt() const
{
    Assert(IsLikelyInt());
    Assert(!IsInt());

    return Verify(bits | (Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged));
}

ValueType ValueType::ToDefiniteNumber_PreferFloat() const
{
    return IsNumber() ? *this : ToDefiniteAnyFloat();
}

ValueType ValueType::ToDefiniteAnyFloat() const
{
    // Not asserting on expected value type because float specialization allows specializing values of arbitrary types, even
    // values that are definitely not float
    return
        Verify(
            OneOn(Bits::Object)
                ? (Bits::Float | Bits::CanBeTaggedValue)
                : bits & (Bits::Int | Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged | Bits::CanBeTaggedValue | Bits::Number) | Bits::Float);
}

ValueType ValueType::ToDefiniteNumber() const
{
    Assert(IsLikelyNumber());
    return IsNumber() ? *this : ToDefiniteAnyNumber();
}

ValueType ValueType::ToDefiniteAnyNumber() const
{
    // Not asserting on expected value type because Conv_Num allows converting values of arbitrary types to number
    if(OneOn(Bits::Object))
        return Verify(Bits::Number | Bits::CanBeTaggedValue);
    Bits numberBits =
        bits &
        (
            Bits::Int |
            Bits::IntCanBeUntagged |
            Bits::IntIsLikelyUntagged |
            Bits::CanBeTaggedValue |
            Bits::Float |
            Bits::Number
        );
    if(!(numberBits & (Bits::Float | Bits::Number)))
        numberBits |= Bits::Number | Bits::CanBeTaggedValue;
    return Verify(numberBits);
}

ValueType ValueType::ToDefinitePrimitiveSubset() const
{
    // This function does not do a safe conversion of an arbitrary type to a definitely-primitive type. It only obtains the
    // primitive subset of bits from the type.

    // When Undefined (or Null) merge with Object, the resulting value type is still likely Object (IsLikelyObject() returns
    // true). ToDefinite() on the merged type would return a type that is definitely Undefined or Object. Usually, that type is
    // not interesting and a test for one or more of the primitive types may have been done. ToDefinitePrimitive() removes the
    // object-specific bits.

    Assert(HasBeenPrimitive());
    Assert(HasBeenObject());

    // If we have an object format of a type (object bit = 1) that represents a primitive (e.g. SIMD128),
    // we want to keep the object bit and type along with other merged primitives (Undefined and/or Null).
    if (IsLikelyObject() && IsLikelyPrimitive())
        return Verify(bits & (Bits::Undefined | Bits::Null) | ToDefiniteObject().bits);

    return
        Verify(
        bits &
        (
            OneOn(Bits::Object)
                ?
                        BitPattern(VALUE_TYPE_COMMON_BIT_COUNT) - (Bits::Likely | Bits::Object)
                    :
                        BitPattern(VALUE_TYPE_COMMON_BIT_COUNT + VALUE_TYPE_NONOBJECT_BIT_COUNT) -
                        (Bits::Likely | Bits::Object | Bits::PrimitiveOrObject)
            ));
}

ValueType ValueType::ToDefiniteObject() const
{
    // When Undefined (or Null) merge with Object, the resulting value type is still likely Object (IsLikelyObject() returns
    // true). ToDefinite() on the merged type would return a type that is definitely Undefined or Object. Usually, that type is
    // not interesting and a test for the Object type may have been done to ensure the Object type. ToDefiniteObject() removes
    // the Undefined and Null bits as well.
    Assert(IsLikelyObject());
    return Verify(bits & ~(BitPattern(VALUE_TYPE_COMMON_BIT_COUNT) - Bits::Object));
}

ValueType ValueType::ToLikely() const
{
    return Verify(bits | Bits::Likely | Bits::CanBeTaggedValue);
}

ValueType ValueType::ToArray() const
{
    Assert(GetObjectType() == ObjectType::ObjectWithArray);

    ValueType valueType(*this);
    valueType.SetObjectType(ObjectType::Array);
    return Verify(valueType);
}

ValueType ValueType::ToPrimitiveOrObject() const
{
    // When an object type is merged with a non-object type, the PrimitiveOrObject bit is set in the merged type by converting
    // the object type to a PrimitiveOrObject type (preserving only the common bits other than Object) and merging it with the
    // non-object type. The PrimitiveOrObject type will not have the Object bit set, so that it can still be queried for whether
    // it was any of the other types.
    Assert(OneOn(Bits::Object));
    return Verify(bits & (BitPattern(VALUE_TYPE_COMMON_BIT_COUNT) - Bits::Object) | Bits::PrimitiveOrObject);
}

__forceinline ValueType ValueType::Merge(const ValueType other) const
{
    Verify(*this);
    Verify(other);

    if(*this == other)
        return *this;

    const ValueType merged(bits | other.bits);
    if(!merged.OneOn(Bits::Object)) // neither has the Object bit set
        return Verify(merged);

    return MergeWithObject(other);
}

ValueType ValueType::MergeWithObject(const ValueType other) const
{
    ValueType merged(bits | other.bits);
    Assert(merged.OneOn(Bits::Object));

    if(ValueType(bits & other.bits).OneOn(Bits::Object)) // both have the Object bit set
    {
        if (GetObjectType() == other.GetObjectType())
            return Verify(merged);
        const ObjectType typedArrayMergedObjectType =
            TypedArrayMergeMap[static_cast<uint16>(GetObjectType())][static_cast<uint16>(other.GetObjectType())];
        if (typedArrayMergedObjectType != ObjectType::UninitializedObject)
        {
            merged.SetObjectType(typedArrayMergedObjectType);
            return Verify(merged);
        }
        if(GetObjectType() != ObjectType::UninitializedObject && other.GetObjectType() != ObjectType::UninitializedObject)
        {
            // Any two different specific object types (excludes UninitializedObject and Object, which don't indicate any
            // specific type of object) merge to Object since the resulting type is not guaranteed to indicate any specific type
            merged.SetObjectType(ObjectType::Object);
            return Verify(merged);
        }

        // Since UninitializedObject is a generic object type, when merged with a specific object type, the resulting object
        // type is not guaranteed to be any specific object type. However, since UninitializedObject means that we don't have
        // info about the object type, we can assume that the merged type is likely an object of the specific type.
        //
        // If both were definitely object, we lose the information that the merged type is also definitely object. For now
        // though, it's better to have "likely object of a specific type" than "definitely object of an unknown type". It may
        // eventually become necessary to distinguish these and have a "definitely object, likely of a specific type".
        return
            Verify(
                GetObjectType() > ObjectType::Object || other.GetObjectType() > ObjectType::Object
                    ? merged.ToLikely()
                    : merged);
    }

    if(OneOn(Bits::Object))
    {
        if(other.CanMergeToObject())
            return Verify(merged);
        return Verify(ToPrimitiveOrObject().bits | other.bits); // see ToPrimitiveOrObject
    }
    Assert(other.OneOn(Bits::Object));
    if(CanMergeToObject())
        return Verify(merged);
    return Verify(bits | other.ToPrimitiveOrObject().bits); // see ToPrimitiveOrObject
}

__inline ValueType ValueType::Merge(const Js::Var var) const
{
    using namespace Js;
    Assert(var);

    if(TaggedInt::Is(var))
        return Merge(GetTaggedInt());
    if(JavascriptNumber::Is_NoTaggedIntCheck(var))
    {
        return
            Merge(
                (IsUninitialized() || IsLikelyInt()) && JavascriptNumber::IsInt32_NoChecks(var)
                    ? GetInt(false)
                    : ValueType::Float);
    }
    return Merge(FromObject(RecyclableObject::FromVar(var)));
}

ValueType::Bits ValueType::TypeIdToBits[Js::TypeIds_Limit];
ValueType::Bits ValueType::VirtualTypeIdToBits[Js::TypeIds_Limit];
INT_PTR ValueType::TypeIdToVtable[Js::TypeIds_Limit];
ObjectType ValueType::VirtualTypedArrayPair[(uint16)ObjectType::Count];
ObjectType ValueType::MixedTypedArrayPair[(uint16)ObjectType::Count];
ObjectType ValueType::TypedArrayMergeMap[(uint16)ObjectType::Count][(uint16)ObjectType::Count];
ObjectType ValueType::MixedTypedToVirtualTypedArray[(uint16)ObjectType::Count];


void ValueType::InitializeTypeIdToBitsMap()
{
    using namespace Js;

    // Initialize all static types to Uninitialized first, so that a zero will indicate that it's a dynamic type
    for (TypeId typeId = static_cast<TypeId>(0); typeId <= TypeIds_LastStaticType; typeId = static_cast<TypeId>(typeId + 1))
    {
        TypeIdToBits[typeId] = ValueType::Uninitialized.bits;
        VirtualTypeIdToBits[typeId] = ValueType::Uninitialized.bits;
        TypeIdToVtable[typeId] = (INT_PTR)nullptr;
    }

    for (ObjectType objType = static_cast<ObjectType>(0); objType <ObjectType::Count; objType = static_cast<ObjectType>((uint16)(objType) + 1))
    {
        VirtualTypedArrayPair[(uint16)objType] = ObjectType::UninitializedObject;
        MixedTypedArrayPair[(uint16)objType] = ObjectType::UninitializedObject;
        MixedTypedToVirtualTypedArray[(uint16)objType] = ObjectType::UninitializedObject;
    }

    for (ObjectType objType = static_cast<ObjectType>(0); objType < ObjectType::Count; objType = static_cast<ObjectType>((uint16)(objType)+1))
    {
        for (ObjectType objTypeInner = static_cast<ObjectType>(0); objTypeInner < ObjectType::Count; objTypeInner = static_cast<ObjectType>((uint16)(objTypeInner)+1))
            TypedArrayMergeMap[(uint16)objType][(uint16)objTypeInner] = ObjectType::UninitializedObject;
    }

    TypeIdToBits[TypeIds_Undefined         ] = ValueType::Undefined.bits;
    TypeIdToBits[TypeIds_Null              ] = ValueType::Null.bits;
    TypeIdToBits[TypeIds_Boolean           ] = ValueType::Boolean.bits;
    TypeIdToBits[TypeIds_String            ] = ValueType::String.bits;
    TypeIdToBits[TypeIds_Symbol            ] = ValueType::Symbol.bits;
    TypeIdToBits[TypeIds_RegEx             ] = GetObject(ObjectType::RegExp).bits;
    TypeIdToBits[TypeIds_Int8Array         ] = GetObject(ObjectType::Int8Array).bits;
    TypeIdToBits[TypeIds_Uint8Array        ] = GetObject(ObjectType::Uint8Array).bits;
    TypeIdToBits[TypeIds_Uint8ClampedArray ] = GetObject(ObjectType::Uint8ClampedArray).bits;
    TypeIdToBits[TypeIds_Int16Array        ] = GetObject(ObjectType::Int16Array).bits;
    TypeIdToBits[TypeIds_Uint16Array       ] = GetObject(ObjectType::Uint16Array).bits;
    TypeIdToBits[TypeIds_Int32Array        ] = GetObject(ObjectType::Int32Array).bits;
    TypeIdToBits[TypeIds_Uint32Array       ] = GetObject(ObjectType::Uint32Array).bits;
    TypeIdToBits[TypeIds_Float32Array      ] = GetObject(ObjectType::Float32Array).bits;
    TypeIdToBits[TypeIds_Float64Array      ] = GetObject(ObjectType::Float64Array).bits;
    TypeIdToBits[TypeIds_Int64Array        ] = GetObject(ObjectType::Int64Array).bits;
    TypeIdToBits[TypeIds_Uint64Array       ] = GetObject(ObjectType::Uint64Array).bits;
    TypeIdToBits[TypeIds_CharArray         ] = GetObject(ObjectType::CharArray).bits;
    TypeIdToBits[TypeIds_BoolArray         ] = GetObject(ObjectType::BoolArray).bits;

    TypeIdToBits[TypeIds_SIMDFloat32x4     ] = GetObject(ObjectType::Simd128Float32x4).bits;
    TypeIdToBits[TypeIds_SIMDInt32x4       ] = GetObject(ObjectType::Simd128Int32x4).bits;
    TypeIdToBits[TypeIds_SIMDInt8x16       ] = GetObject(ObjectType::Simd128Int8x16).bits;
    TypeIdToBits[TypeIds_SIMDFloat64x2     ] = GetObject(ObjectType::Simd128Float64x2).bits;


    VirtualTypeIdToBits[TypeIds_Int8Array] = GetObject(ObjectType::Int8VirtualArray).bits;
    VirtualTypeIdToBits[TypeIds_Uint8Array] = GetObject(ObjectType::Uint8VirtualArray).bits;
    VirtualTypeIdToBits[TypeIds_Uint8ClampedArray] = GetObject(ObjectType::Uint8ClampedArray).bits;
    VirtualTypeIdToBits[TypeIds_Int16Array] = GetObject(ObjectType::Int16VirtualArray).bits;
    VirtualTypeIdToBits[TypeIds_Uint16Array] = GetObject(ObjectType::Uint16VirtualArray).bits;
    VirtualTypeIdToBits[TypeIds_Int32Array] = GetObject(ObjectType::Int32VirtualArray).bits;
    VirtualTypeIdToBits[TypeIds_Uint32Array] = GetObject(ObjectType::Uint32VirtualArray).bits;
    VirtualTypeIdToBits[TypeIds_Float32Array] = GetObject(ObjectType::Float32VirtualArray).bits;
    VirtualTypeIdToBits[TypeIds_Float64Array] = GetObject(ObjectType::Float64VirtualArray).bits;


    TypeIdToVtable[TypeIds_Int8Array] = VirtualTableInfo<Int8VirtualArray>::Address;
    TypeIdToVtable[TypeIds_Uint8Array] = VirtualTableInfo<Uint8VirtualArray>::Address;
    TypeIdToVtable[TypeIds_Uint8ClampedArray] = VirtualTableInfo<Uint8ClampedVirtualArray>::Address;
    TypeIdToVtable[TypeIds_Int16Array] = VirtualTableInfo<Int16VirtualArray>::Address;
    TypeIdToVtable[TypeIds_Uint16Array] = VirtualTableInfo<Uint16VirtualArray>::Address;
    TypeIdToVtable[TypeIds_Int32Array] = VirtualTableInfo<Int32VirtualArray>::Address;
    TypeIdToVtable[TypeIds_Uint32Array] = VirtualTableInfo<Uint32VirtualArray>::Address;
    TypeIdToVtable[TypeIds_Float32Array] = VirtualTableInfo<Float32VirtualArray>::Address;
    TypeIdToVtable[TypeIds_Float64Array] = VirtualTableInfo<Float64VirtualArray>::Address;

    VirtualTypedArrayPair[(int)ObjectType::Int8VirtualArray] = ObjectType::Int8Array;
    VirtualTypedArrayPair[(int)ObjectType::Int8Array] = ObjectType::Int8VirtualArray;
    VirtualTypedArrayPair[(int)ObjectType::Uint8VirtualArray] = ObjectType::Uint8Array;
    VirtualTypedArrayPair[(int)ObjectType::Uint8Array] = ObjectType::Uint8VirtualArray;
    VirtualTypedArrayPair[(int)ObjectType::Uint8ClampedVirtualArray] = ObjectType::Uint8ClampedArray;
    VirtualTypedArrayPair[(int)ObjectType::Uint8ClampedArray] = ObjectType::Uint8ClampedVirtualArray;
    VirtualTypedArrayPair[(int)ObjectType::Int16VirtualArray] = ObjectType::Int16Array;
    VirtualTypedArrayPair[(int)ObjectType::Int16Array] = ObjectType::Int16VirtualArray;
    VirtualTypedArrayPair[(int)ObjectType::Uint16VirtualArray] = ObjectType::Uint16Array;
    VirtualTypedArrayPair[(int)ObjectType::Uint16Array] = ObjectType::Uint16VirtualArray;
    VirtualTypedArrayPair[(int)ObjectType::Int32VirtualArray] = ObjectType::Int32Array;
    VirtualTypedArrayPair[(int)ObjectType::Int32Array] = ObjectType::Int32VirtualArray;
    VirtualTypedArrayPair[(int)ObjectType::Uint32VirtualArray] = ObjectType::Uint32Array;
    VirtualTypedArrayPair[(int)ObjectType::Uint32Array] = ObjectType::Uint32VirtualArray;
    VirtualTypedArrayPair[(int)ObjectType::Float32VirtualArray] = ObjectType::Float32Array;
    VirtualTypedArrayPair[(int)ObjectType::Float32Array] = ObjectType::Float32VirtualArray;
    VirtualTypedArrayPair[(int)ObjectType::Float64VirtualArray] = ObjectType::Float64Array;
    VirtualTypedArrayPair[(int)ObjectType::Float64Array] = ObjectType::Float64VirtualArray;

    MixedTypedArrayPair[(int)ObjectType::Int8VirtualArray] = ObjectType::Int8MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Int8Array] = ObjectType::Int8MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Uint8VirtualArray] = ObjectType::Uint8MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Uint8Array] = ObjectType::Uint8MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Uint8ClampedVirtualArray] = ObjectType::Uint8ClampedMixedArray;
    MixedTypedArrayPair[(int)ObjectType::Uint8ClampedArray] = ObjectType::Uint8ClampedMixedArray;
    MixedTypedArrayPair[(int)ObjectType::Int16VirtualArray] = ObjectType::Int16MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Int16Array] = ObjectType::Int16MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Uint16VirtualArray] = ObjectType::Uint16MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Uint16Array] = ObjectType::Uint16MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Int32VirtualArray] = ObjectType::Int32MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Int32Array] = ObjectType::Int32MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Uint32VirtualArray] = ObjectType::Uint32MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Uint32Array] = ObjectType::Uint32MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Float32VirtualArray] = ObjectType::Float32MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Float32Array] = ObjectType::Float32MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Float64VirtualArray] = ObjectType::Float64MixedArray;
    MixedTypedArrayPair[(int)ObjectType::Float64Array] = ObjectType::Float64MixedArray;


    MixedTypedToVirtualTypedArray[(int)ObjectType::Int8MixedArray] = ObjectType::Int8VirtualArray;
    MixedTypedToVirtualTypedArray[(int)ObjectType::Uint8MixedArray] = ObjectType::Uint8VirtualArray;
    MixedTypedToVirtualTypedArray[(int)ObjectType::Uint8ClampedMixedArray] = ObjectType::Uint8ClampedVirtualArray;
    MixedTypedToVirtualTypedArray[(int)ObjectType::Int16MixedArray] = ObjectType::Int16VirtualArray;
    MixedTypedToVirtualTypedArray[(int)ObjectType::Uint16MixedArray] = ObjectType::Uint16VirtualArray;
    MixedTypedToVirtualTypedArray[(int)ObjectType::Int32MixedArray] = ObjectType::Int32VirtualArray;
    MixedTypedToVirtualTypedArray[(int)ObjectType::Uint32MixedArray] = ObjectType::Uint32VirtualArray;
    MixedTypedToVirtualTypedArray[(int)ObjectType::Float32MixedArray] = ObjectType::Float32VirtualArray;
    MixedTypedToVirtualTypedArray[(int)ObjectType::Float64MixedArray] = ObjectType::Float64VirtualArray;

    TypedArrayMergeMap[(int)ObjectType::Int8Array][(int)ObjectType::Int8VirtualArray] = ObjectType::Int8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int8VirtualArray][(int)ObjectType::Int8Array] = ObjectType::Int8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int8MixedArray][(int)ObjectType::Int8Array] = ObjectType::Int8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int8MixedArray][(int)ObjectType::Int8VirtualArray] = ObjectType::Int8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int8Array][(int)ObjectType::Int8MixedArray] = ObjectType::Int8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int8VirtualArray][(int)ObjectType::Int8MixedArray] = ObjectType::Int8MixedArray;

    TypedArrayMergeMap[(int)ObjectType::Uint8Array][(int)ObjectType::Uint8VirtualArray] = ObjectType::Uint8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8VirtualArray][(int)ObjectType::Uint8Array] = ObjectType::Uint8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8MixedArray][(int)ObjectType::Uint8VirtualArray] = ObjectType::Uint8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8MixedArray][(int)ObjectType::Uint8Array] = ObjectType::Uint8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8Array][(int)ObjectType::Uint8MixedArray] = ObjectType::Uint8MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8VirtualArray][(int)ObjectType::Uint8MixedArray] = ObjectType::Uint8MixedArray;

    TypedArrayMergeMap[(int)ObjectType::Uint8ClampedArray][(int)ObjectType::Uint8ClampedVirtualArray] = ObjectType::Uint8ClampedMixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8ClampedVirtualArray][(int)ObjectType::Uint8ClampedArray] = ObjectType::Uint8ClampedMixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8ClampedMixedArray][(int)ObjectType::Uint8ClampedVirtualArray] = ObjectType::Uint8ClampedMixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8ClampedMixedArray][(int)ObjectType::Uint8ClampedArray] = ObjectType::Uint8ClampedMixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8ClampedArray][(int)ObjectType::Uint8ClampedMixedArray] = ObjectType::Uint8ClampedMixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint8ClampedVirtualArray][(int)ObjectType::Uint8ClampedMixedArray] = ObjectType::Uint8ClampedMixedArray;

    TypedArrayMergeMap[(int)ObjectType::Int16Array][(int)ObjectType::Int16VirtualArray] = ObjectType::Int16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int16VirtualArray][(int)ObjectType::Int16Array] = ObjectType::Int16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int16MixedArray][(int)ObjectType::Int16VirtualArray] = ObjectType::Int16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int16MixedArray][(int)ObjectType::Int16Array] = ObjectType::Int16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int16Array][(int)ObjectType::Int16MixedArray] = ObjectType::Int16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int16VirtualArray][(int)ObjectType::Int16MixedArray] = ObjectType::Int16MixedArray;

    TypedArrayMergeMap[(int)ObjectType::Uint16Array][(int)ObjectType::Uint16VirtualArray] = ObjectType::Uint16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint16VirtualArray][(int)ObjectType::Uint16Array] = ObjectType::Uint16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint16MixedArray][(int)ObjectType::Uint16VirtualArray] = ObjectType::Uint16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint16MixedArray][(int)ObjectType::Uint16Array] = ObjectType::Uint16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint16Array][(int)ObjectType::Uint16MixedArray] = ObjectType::Uint16MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint16VirtualArray][(int)ObjectType::Uint16MixedArray] = ObjectType::Uint16MixedArray;

    TypedArrayMergeMap[(int)ObjectType::Int32Array][(int)ObjectType::Int32VirtualArray] = ObjectType::Int32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int32VirtualArray][(int)ObjectType::Int32Array] = ObjectType::Int32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int32MixedArray][(int)ObjectType::Int32VirtualArray] = ObjectType::Int32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int32MixedArray][(int)ObjectType::Int32Array] = ObjectType::Int32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int32Array][(int)ObjectType::Int32MixedArray] = ObjectType::Int32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Int32VirtualArray][(int)ObjectType::Int32MixedArray] = ObjectType::Int32MixedArray;

    TypedArrayMergeMap[(int)ObjectType::Uint32Array][(int)ObjectType::Uint32VirtualArray] = ObjectType::Uint32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint32VirtualArray][(int)ObjectType::Uint32Array] = ObjectType::Uint32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint32MixedArray][(int)ObjectType::Uint32VirtualArray] = ObjectType::Uint32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint32MixedArray][(int)ObjectType::Uint32Array] = ObjectType::Uint32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint32Array][(int)ObjectType::Uint32MixedArray] = ObjectType::Uint32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Uint32VirtualArray][(int)ObjectType::Uint32MixedArray] = ObjectType::Uint32MixedArray;

    TypedArrayMergeMap[(int)ObjectType::Float32Array][(int)ObjectType::Float32VirtualArray] = ObjectType::Float32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float32VirtualArray][(int)ObjectType::Float32Array] = ObjectType::Float32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float32MixedArray][(int)ObjectType::Float32VirtualArray] = ObjectType::Float32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float32MixedArray][(int)ObjectType::Float32Array] = ObjectType::Float32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float32Array][(int)ObjectType::Float32MixedArray] = ObjectType::Float32MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float32VirtualArray][(int)ObjectType::Float32MixedArray] = ObjectType::Float32MixedArray;

    TypedArrayMergeMap[(int)ObjectType::Float64Array][(int)ObjectType::Float64VirtualArray] = ObjectType::Float64MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float64VirtualArray][(int)ObjectType::Float64Array] = ObjectType::Float64MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float64MixedArray][(int)ObjectType::Float64VirtualArray] = ObjectType::Float64MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float64MixedArray][(int)ObjectType::Float64Array] = ObjectType::Float64MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float64Array][(int)ObjectType::Float64MixedArray] = ObjectType::Float64MixedArray;
    TypedArrayMergeMap[(int)ObjectType::Float64VirtualArray][(int)ObjectType::Float64MixedArray] = ObjectType::Float64MixedArray;

}

INT_PTR ValueType::GetVirtualTypedArrayVtable(const Js::TypeId typeId)
{
    if (typeId < _countof(TypeIdToVtable))
    {
        return TypeIdToVtable[typeId];
    }
    return NULL;
}

ValueType ValueType::FromTypeId(const Js::TypeId typeId, bool useVirtual)
{
    if(typeId < _countof(TypeIdToBits))
    {
        if (useVirtual)
        {
            const Bits bits = VirtualTypeIdToBits[typeId];
            if (!!bits)
                return bits;
        }
        else
        {
            const Bits bits = TypeIdToBits[typeId];
            if (!!bits)
                return bits;
        }
    }
    return Uninitialized;
}

ValueType ValueType::FromObject(Js::RecyclableObject *const recyclableObject)
{
    using namespace Js;
    Assert(recyclableObject);
    const TypeId typeId = recyclableObject->GetTypeId();
    if (typeId < _countof(TypeIdToBits))
    {
        const Bits bits = TypeIdToBits[typeId];
        if (!!bits)
        {
            const ValueType valueType = Verify(bits);
            if (!valueType.IsLikelyOptimizedTypedArray())
                return valueType;
            bool isVirtual = (VirtualTableInfoBase::GetVirtualTable(recyclableObject) == ValueType::GetVirtualTypedArrayVtable(typeId));
            if (!isVirtual)
                return valueType;
            return GetObject(VirtualTypedArrayPair[static_cast<uint16>(valueType.GetObjectType())]);
        }
    }
    Assert(DynamicType::Is(typeId)); // all static type IDs have nonzero values in TypeIdToBits

    if(!JavascriptArray::Is(typeId))
    {
        // TODO: Once the issue with loop bodies and uninitialized interpreter local slots is fixed, use FromVar
        DynamicObject *const object = static_cast<DynamicObject *>(recyclableObject);
        if(!VirtualTableInfo<DynamicObject>::HasVirtualTable(object) || !object->HasObjectArray())
            return GetObject(ObjectType::Object);
        return FromObjectWithArray(object);
    }

    return FromArray(ObjectType::Array, static_cast<JavascriptArray *>(recyclableObject), typeId);
}

ValueType ValueType::FromObjectWithArray(Js::DynamicObject *const object)
{
    using namespace Js;
    Assert(object);
    Assert(VirtualTableInfo<DynamicObject>::HasVirtualTable(object));
    Assert(object->GetTypeId() == TypeIds_Object); // this check should be a superset of the DynamicObject vtable check above
    Assert(object->HasObjectArray());

    ArrayObject *const objectArray = object->GetObjectArray();
    Assert(objectArray);
    if(!VirtualTableInfo<JavascriptArray>::HasVirtualTable(objectArray))
        return GetObject(ObjectType::Object);
    return FromObjectArray(JavascriptArray::FromVar(objectArray));
}

__inline ValueType ValueType::FromObjectArray(Js::JavascriptArray *const objectArray)
{
    using namespace Js;
    Assert(objectArray);

    return FromArray(ObjectType::ObjectWithArray, objectArray, TypeIds_Array); // objects with native arrays are currently not supported
}

__inline ValueType ValueType::FromArray(
    const ObjectType objectType,
    Js::JavascriptArray *const array,
    const Js::TypeId arrayTypeId)
{
    Assert(array);
    Assert(array->GetTypeId() == arrayTypeId);

    // TODO: Once the issue with loop bodies and uninitialized interpreter local slots is fixed, use FromVar and the checked version of HasNoMissingValues
    return
        Verify(
            GetArray(objectType)
                .SetHasNoMissingValues(array->HasNoMissingValues_Unchecked())
                .SetArrayTypeId(arrayTypeId));
}

bool ValueType::operator ==(const ValueType other) const
{
    return bits == other.bits;
}

bool ValueType::operator !=(const ValueType other) const
{
    return !(*this == other);
}

uint ValueType::GetHashCode() const
{
    return static_cast<uint>(bits);
}

const char *const ValueType::BitNames[] =
{
    #define VALUE_TYPE_BIT(t, b) "" STRINGIZE(t) "",
    #include "ValueTypes.h"
    #undef VALUE_TYPE_BIT
};

const char *const ObjectTypeNames[] =
{
    #define OBJECT_TYPE(ot) "" STRINGIZE(ot) "",
    #include "ValueTypes.h"
    #undef OBJECT_TYPE
};

size_t ValueType::GetLowestBitIndex(const Bits b)
{
    Assert(b);

    DWORD i;
    ::GetFirstBitSet(&i, static_cast<UnitWord32>(b));
    return i;
}

void ValueType::ToVerboseString(char (&str)[VALUE_TYPE_MAX_STRING_SIZE]) const
{
    if(IsUninitialized())
    {
        strcpy_s(str, "Uninitialized");
        return;
    }

    Bits b = bits;
    if(OneOn(Bits::Object))
    {
        // Exclude the object type for enumerating bits, and exclude bits specific to a different object type
        b = _objectBits;
        if(IsLikelyArrayOrObjectWithArray())
            b &= ~(Bits::NonInts | Bits::NonFloats); // these are handled separately for better readability
        else
            b &= ~BitPattern(VALUE_TYPE_ARRAY_BIT_COUNT, VALUE_TYPE_COMMON_BIT_COUNT);
    }
    else if(!CONFIG_FLAG(Verbose))
        b &= ~(Bits::IntCanBeUntagged | Bits::IntIsLikelyUntagged); // these will be simplified
    size_t length = 0;
    bool addUnderscore = false;
    size_t nameIndexOffset = 0;
    do
    {
        const char *name;
        switch(b & -b) // bit to be printed
        {
            case Bits::Object:
                if(IsLikelyNativeArray())
                {
                    Assert(GetObjectType() == ObjectType::Array || GetObjectType() == ObjectType::ObjectWithArray);
                    Assert(HasIntElements() || HasFloatElements());
                    name =
                        GetObjectType() == ObjectType::Array
                            ? HasIntElements() ? "NativeIntArray" : "NativeFloatArray"
                            : HasIntElements() ? "ObjectWithNativeIntArray" : "ObjectWithNativeFloatArray";
                    break;
                }
                name = ObjectTypeNames[static_cast<TSize>(GetObjectType())]; // print the object type instead
                break;

            case Bits::Int:
                if(!CONFIG_FLAG(Verbose) && !OneOn(Bits::Object))
                {
                    if(AnyOnExcept(Bits::Likely | Bits::IntCanBeUntagged | Bits::CanBeTaggedValue))
                    {
                        name = "TaggedInt";
                        break;
                    }
                    if(OneOn(Bits::IntIsLikelyUntagged))
                    {
                        name = "IntAndLikelyUntagged";
                        break;
                    }
                }
                // fall through

            default:
                size_t nameIndex = nameIndexOffset + GetLowestBitIndex(b);
                Assert(nameIndex < sizeof(BitNames) / sizeof(BitNames[0]));
                __analysis_assume(nameIndex < sizeof(BitNames) / sizeof(BitNames[0])); // function is not used in shipping builds, satisfy oacr
                name = BitNames[nameIndex];
                break;
        }
        size_t nameLength = strlen(name);
        if(addUnderscore)
            ++nameLength;
        if(length + nameLength >= sizeof(str) / sizeof(str[0]))
            break;

        if(addUnderscore)
        {
            str[length++] = '_';
            --nameLength;
        }
        else
            addUnderscore = !(b & Bits::Likely);

        js_memcpy_s(&str[length], sizeof(str) / sizeof(str[0]) - 1 - length, name, nameLength);
        length += nameLength;

        if((b & -b) == BitPattern(1, VALUE_TYPE_OBJECT_BIT_INDEX)) // if the bit that was just printed is the last common bit
            nameIndexOffset += VALUE_TYPE_NONOBJECT_BIT_COUNT; // skip bit names for bits that only apply when the Object bit is set
        b &= b - 1; // unset the least significant set bit
    } while(!!b);

    Assert(length < sizeof(str) / sizeof(str[0]));
    str[length] = '\0';
}

void ValueType::ToString(wchar (&str)[VALUE_TYPE_MAX_STRING_SIZE]) const
{
    char charStr[VALUE_TYPE_MAX_STRING_SIZE];
    ToString(charStr);
    for(int i = 0; i < VALUE_TYPE_MAX_STRING_SIZE; ++i)
    {
        str[i] = charStr[i];
        if(!charStr[i])
            break;
    }
}

void ValueType::ToString(char (&str)[VALUE_TYPE_MAX_STRING_SIZE]) const
{
    if(IsUninitialized() || CONFIG_FLAG(Verbose))
    {
        ToVerboseString(str);
        return;
    }

    bool canBeTaggedValue = CanBeTaggedValue();
    const ValueType definiteType = ToDefinite();
    ValueType generalizedType;
    if(definiteType.IsInt())
        generalizedType = definiteType;
    else if(definiteType.IsFloat())
        generalizedType = Float;
    else if(definiteType.IsNumber())
    {
        generalizedType = Number;
        if(definiteType.IsLikelyInt())
            generalizedType = generalizedType.Merge(GetInt(definiteType.IsLikelyTaggedInt()));
    }
    else if(definiteType.IsUndefined())
        generalizedType = Undefined;
    else if(definiteType.IsNull())
        generalizedType = Null;
    else if(definiteType.IsBoolean())
        generalizedType = Boolean;
    else if (definiteType.IsString())
        generalizedType = String;
    else if (definiteType.IsSymbol())
        generalizedType = Symbol;
    else if(definiteType.IsPrimitive() && !IsLikelyObject())
    {
        strcpy_s(str, IsDefinite() ? "Primitive" : "LikelyPrimitive");
        return;
    }
    else if(definiteType.IsLikelyObject())
    {
        generalizedType = definiteType.ToDefiniteObject();
        if(!definiteType.IsObject())
            generalizedType = generalizedType.ToLikely();
    }
    else
    {
        strcpy_s(str, IsDefinite() ? "Mixed" : "LikelyMixed");
        return;
    }

    if(!IsDefinite())
        generalizedType = generalizedType.ToLikely();
    generalizedType.SetCanBeTaggedValue(canBeTaggedValue).ToVerboseString(str);
}

#if ENABLE_DEBUG_CONFIG_OPTIONS

void ValueType::ToStringDebug(__out_ecount(strSize) char *const str, const size_t strSize) const
{
    Assert(str);

    if(strSize == 0)
        return;

    char generalizedStr[VALUE_TYPE_MAX_STRING_SIZE];
    ToString(generalizedStr);

    char verboseStr[VALUE_TYPE_MAX_STRING_SIZE];
    ToVerboseString(verboseStr);

    const size_t generalizedStrLength = strlen(generalizedStr);
    if(strcmp(generalizedStr, verboseStr) == 0)
    {
        if(generalizedStrLength >= strSize)
        {
            str[0] = '\0';
            return;
        }
        strcpy_s(str, strSize, generalizedStr);
        return;
    }

    const size_t verboseStrLength = strlen(verboseStr);
    if(generalizedStrLength + verboseStrLength + 3 >= strSize)
    {
        str[0] = '\0';
        return;
    }
    sprintf_s(str, strSize, "%s (%s)", generalizedStr, verboseStr);
}

#endif

bool ValueType::FromString(const wchar *const str, ValueType *valueType)
{
    Assert(str);
    Assert(valueType);

    char charStr[VALUE_TYPE_MAX_STRING_SIZE];
    int i = 0;
    for(; i < VALUE_TYPE_MAX_STRING_SIZE - 1 && str[i]; ++i)
    {
        Assert(static_cast<wchar>(static_cast<char>(str[i])) == str[i]);
        charStr[i] = static_cast<char>(str[i]);
    }
    charStr[i] = '\0';
    return FromString(charStr, valueType);
}

bool ValueType::FromString(const char *const str, ValueType *valueType)
{
    Assert(str);
    Assert(valueType);

    bool found = false;
    MapInitialValueTypesUntil([&](const ValueType initialValueType, const size_t) -> bool
    {
        char valueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
        initialValueType.ToString(valueTypeStr);
        if(strcmp(str, valueTypeStr))
            return false;
        *valueType = initialValueType;
        found = true;
        return true;
    });
    return found;
}

ValueType::TSize ValueType::GetRawData() const
{
    return static_cast<TSize>(bits);
}

ValueType ValueType::FromRawData(const TSize rawData)
{
    return Verify(static_cast<Bits>(rawData));
}

// Virtual and Mixed Typed Array Methods

bool ValueType::IsVirtualTypedArrayPair(const ObjectType other) const
{
    return (VirtualTypedArrayPair[(int)GetObjectType()] == other);
}

bool ValueType::IsLikelyMixedTypedArrayType() const
{
    return (IsLikelyObject() && GetObjectType() >= ObjectType::Int8MixedArray && GetObjectType() <= ObjectType::Float64MixedArray);
}

bool ValueType::IsMixedTypedArrayPair(const ValueType other) const
{
    return ( IsLikelyObject() && other.IsLikelyObject() &&
             (    (MixedTypedArrayPair[(int)GetObjectType()] == other.GetObjectType()) ||
                  (MixedTypedArrayPair[(int)other.GetObjectType()] == GetObjectType()) ||
                  (IsLikelyMixedTypedArrayType() && other.IsLikelyMixedTypedArrayType())
             )
           );
}

ValueType ValueType::ChangeToMixedTypedArrayType() const
{
    ObjectType objType = MixedTypedArrayPair[(int)GetObjectType()];
    Assert(objType);
    ValueType valueType(bits);
    valueType.SetObjectType(objType);
    return Verify(valueType);
}


ObjectType ValueType::GetMixedTypedArrayObjectType() const
{
    return MixedTypedArrayPair[(int)GetObjectType()];
}

ObjectType ValueType::GetMixedToVirtualTypedArrayObjectType() const
{
    return MixedTypedToVirtualTypedArray[(int)GetObjectType()];
}


#if DBG

void ValueType::RunUnitTests()
{
    Assert(Uninitialized.bits == (Bits::Likely | Bits::CanBeTaggedValue));
    Assert(!ObjectType::UninitializedObject); // this is assumed in Merge

    const ValueType TaggedInt(GetTaggedInt());
    const ValueType IntAndLikelyTagged(GetInt(true));
    const ValueType IntAndLikelyUntagged(GetInt(false));

    Assert(TaggedInt.IsTaggedInt());
    Assert(TaggedInt.IsIntAndLikelyTagged());
    Assert(TaggedInt.IsLikelyTaggedInt());
    Assert(!TaggedInt.IsLikelyUntaggedInt());
    Assert(TaggedInt.IsInt());
    Assert(TaggedInt.IsLikelyInt());
    Assert(!TaggedInt.IsLikelyFloat());
    Assert(TaggedInt.IsNumber());
    Assert(TaggedInt.IsLikelyNumber());
    Assert(TaggedInt.IsPrimitive());
    Assert(TaggedInt.IsLikelyPrimitive());

    Assert(!IntAndLikelyTagged.IsTaggedInt());
    Assert(IntAndLikelyTagged.IsIntAndLikelyTagged());
    Assert(IntAndLikelyTagged.IsLikelyTaggedInt());
    Assert(!IntAndLikelyTagged.IsLikelyUntaggedInt());
    Assert(IntAndLikelyTagged.IsInt());
    Assert(IntAndLikelyTagged.IsLikelyInt());
    Assert(!IntAndLikelyTagged.IsLikelyFloat());
    Assert(IntAndLikelyTagged.IsNumber());
    Assert(IntAndLikelyTagged.IsLikelyNumber());
    Assert(IntAndLikelyTagged.IsPrimitive());
    Assert(IntAndLikelyTagged.IsLikelyPrimitive());

    Assert(GetNumberAndLikelyInt(true).IsLikelyTaggedInt());
    Assert(!GetNumberAndLikelyInt(true).IsLikelyUntaggedInt());
    Assert(!GetNumberAndLikelyInt(true).IsInt());
    Assert(GetNumberAndLikelyInt(true).IsLikelyInt());
    Assert(!GetNumberAndLikelyInt(true).IsLikelyFloat());
    Assert(GetNumberAndLikelyInt(true).IsNumber());
    Assert(GetNumberAndLikelyInt(true).IsLikelyNumber());
    Assert(GetNumberAndLikelyInt(true).IsPrimitive());
    Assert(GetNumberAndLikelyInt(true).IsLikelyPrimitive());

    Assert(TaggedInt.ToLikely().IsLikelyTaggedInt());
    Assert(!TaggedInt.ToLikely().IsLikelyUntaggedInt());
    Assert(TaggedInt.ToLikely().IsLikelyInt());
    Assert(!TaggedInt.ToLikely().IsLikelyFloat());
    Assert(TaggedInt.ToLikely().IsLikelyNumber());
    Assert(!TaggedInt.ToLikely().IsPrimitive());
    Assert(TaggedInt.ToLikely().IsLikelyPrimitive());

    Assert(IntAndLikelyTagged.ToLikely().IsLikelyTaggedInt());
    Assert(!IntAndLikelyTagged.ToLikely().IsLikelyUntaggedInt());
    Assert(IntAndLikelyTagged.ToLikely().IsLikelyInt());
    Assert(!IntAndLikelyTagged.ToLikely().IsLikelyFloat());
    Assert(IntAndLikelyTagged.ToLikely().IsLikelyNumber());
    Assert(!IntAndLikelyTagged.ToLikely().IsPrimitive());
    Assert(IntAndLikelyTagged.ToLikely().IsLikelyPrimitive());

    Assert(!IntAndLikelyUntagged.IsLikelyTaggedInt());
    Assert(IntAndLikelyUntagged.IsIntAndLikelyUntagged());
    Assert(IntAndLikelyUntagged.IsLikelyUntaggedInt());
    Assert(IntAndLikelyUntagged.IsInt());
    Assert(IntAndLikelyUntagged.IsLikelyInt());
    Assert(!IntAndLikelyUntagged.IsLikelyFloat());
    Assert(IntAndLikelyUntagged.IsNumber());
    Assert(IntAndLikelyUntagged.IsLikelyNumber());
    Assert(IntAndLikelyUntagged.IsPrimitive());
    Assert(IntAndLikelyUntagged.IsLikelyPrimitive());

    Assert(!GetNumberAndLikelyInt(false).IsLikelyTaggedInt());
    Assert(!GetNumberAndLikelyInt(false).IsIntAndLikelyUntagged());
    Assert(GetNumberAndLikelyInt(false).IsLikelyUntaggedInt());
    Assert(!GetNumberAndLikelyInt(false).IsInt());
    Assert(GetNumberAndLikelyInt(false).IsLikelyInt());
    Assert(!GetNumberAndLikelyInt(false).IsLikelyFloat());
    Assert(GetNumberAndLikelyInt(false).IsNumber());
    Assert(GetNumberAndLikelyInt(false).IsLikelyNumber());
    Assert(GetNumberAndLikelyInt(false).IsPrimitive());
    Assert(GetNumberAndLikelyInt(false).IsLikelyPrimitive());

    Assert(!IntAndLikelyUntagged.ToLikely().IsLikelyTaggedInt());
    Assert(IntAndLikelyUntagged.ToLikely().IsLikelyUntaggedInt());
    Assert(IntAndLikelyUntagged.ToLikely().IsLikelyInt());
    Assert(!IntAndLikelyUntagged.ToLikely().IsLikelyFloat());
    Assert(IntAndLikelyUntagged.ToLikely().IsLikelyNumber());
    Assert(!IntAndLikelyUntagged.ToLikely().IsPrimitive());
    Assert(IntAndLikelyUntagged.ToLikely().IsLikelyPrimitive());

    Assert(!Float.IsLikelyInt());
    Assert(Float.IsFloat());
    Assert(Float.IsLikelyFloat());
    Assert(Float.IsNumber());
    Assert(Float.IsLikelyNumber());
    Assert(Float.IsPrimitive());
    Assert(Float.IsLikelyPrimitive());

    Assert(!Float.ToLikely().IsLikelyInt());
    Assert(Float.ToLikely().IsLikelyFloat());
    Assert(Float.ToLikely().IsLikelyNumber());
    Assert(!Float.ToLikely().IsPrimitive());
    Assert(!Float.ToLikely().IsPrimitive());
    Assert(Float.ToLikely().IsLikelyPrimitive());

    Assert(!Number.IsLikelyInt());
    Assert(!Number.IsLikelyFloat());
    Assert(Number.IsNumber());
    Assert(Number.IsUnknownNumber());
    Assert(Number.IsLikelyNumber());
    Assert(Number.IsPrimitive());
    Assert(Number.IsLikelyPrimitive());

    Assert(!Number.ToLikely().IsLikelyInt());
    Assert(!Number.ToLikely().IsLikelyFloat());
    Assert(Number.ToLikely().IsLikelyNumber());
    Assert(Number.ToLikely().IsLikelyUnknownNumber());
    Assert(!Number.ToLikely().IsPrimitive());
    Assert(Number.ToLikely().IsLikelyPrimitive());

    Assert(!UninitializedObject.IsLikelyPrimitive());
    Assert(UninitializedObject.IsObject());
    Assert(UninitializedObject.IsLikelyObject());

    Assert(!UninitializedObject.ToLikely().IsLikelyPrimitive());
    Assert(!UninitializedObject.ToLikely().IsObject());
    Assert(UninitializedObject.ToLikely().IsLikelyObject());

    Assert(Undefined.IsNotInt());
    Assert(!Undefined.ToLikely().IsNotInt());
    Assert(Null.IsNotInt());
    Assert(!Null.ToLikely().IsNotInt());
    Assert(Boolean.IsNotInt());
    Assert(!Boolean.ToLikely().IsNotInt());
    Assert(String.IsNotInt());
    Assert(!String.ToLikely().IsNotInt());
    Assert(UninitializedObject.IsNotInt());
    Assert(!UninitializedObject.ToLikely().IsNotInt());

    {
        const ValueType m(IntAndLikelyUntagged.Merge(Null));
        Assert(m.IsPrimitive());
        Assert(m.IsLikelyPrimitive());
        Assert(IntAndLikelyUntagged.IsSubsetOf(m, true, true, true, true));
        Assert(!m.IsSubsetOf(IntAndLikelyUntagged, true, true, true, true));
    }

    {
        const ValueType m(IntAndLikelyUntagged.Merge(UninitializedObject));
        Assert(m.HasBeenInt());
        Assert(!m.IsLikelyPrimitive());
        Assert(m.HasBeenObject());
        Assert(!m.IsLikelyObject());
    }

    {
        const ValueType m(Uninitialized.Merge(IntAndLikelyTagged));
        Assert(!m.IsPrimitive());
        Assert(!m.IsDefinite());
        Assert(m.IsLikelyTaggedInt());
    }

    {
        const ValueType m(UninitializedObject.Merge(Null));
        Assert(UninitializedObject.IsSubsetOf(m, true, true, true, true));
        Assert(!m.IsSubsetOf(UninitializedObject, true, true, true, true));
        Assert(Null.IsSubsetOf(m, true, true, true, true));
        Assert(!m.IsSubsetOf(Null, true, true, true, true));
        Assert(!TaggedInt.IsSubsetOf(m, true, true, true, true));
        Assert(!m.IsSubsetOf(TaggedInt, true, true, true, true));

        const ValueType po = m.Merge(TaggedInt);
        Assert(m.IsSubsetOf(po, true, true, true, true));
        Assert(!po.IsSubsetOf(m, true, true, true, true));
    }

    MapInitialValueTypesUntil([](const ValueType valueType0, const size_t i) -> bool
    {
        MapInitialValueTypesUntil([=](const ValueType t1, const size_t j) -> bool
        {
            if(j < i)
                return false;

            const ValueType t0(valueType0);
            const ValueType m(t0.Merge(t1));

            Assert(m.bits == t1.Merge(t0).bits);

            Assert(m.IsUninitialized() == (t0.IsUninitialized() && t1.IsUninitialized()));
            const bool isSubsetWithTypeSpecEnabled = t0.IsSubsetOf(t1, true, true, true, true);
            if(t0.IsUninitialized())
            {
                Assert(isSubsetWithTypeSpecEnabled == t1.IsUninitialized());
                return false;
            }
            else if(t1.IsUninitialized())
            {
                Assert(isSubsetWithTypeSpecEnabled);
                return false;
            }

            Assert(m.IsIntAndLikelyTagged() == (t0.IsIntAndLikelyTagged() && t1.IsIntAndLikelyTagged()));
            Assert(
                m.IsLikelyTaggedInt() ==
                (
                    t0.IsLikelyNumber() && t1.IsLikelyNumber() &&               // both are likely number
                    !t0.IsLikelyFloat() && !t1.IsLikelyFloat() &&               // neither is likely float
                    !t0.IsLikelyUntaggedInt() && !t1.IsLikelyUntaggedInt() &&   // neither is likely untagged int
                    (t0.IsLikelyTaggedInt() || t1.IsLikelyTaggedInt())          // one is likely tagged int
                ));

            Assert(m.IsInt() == (t0.IsInt() && t1.IsInt()));
            Assert(
                m.IsLikelyInt() ==
                (
                    t0.IsLikelyNumber() && t1.IsLikelyNumber() &&   // both are likely number
                    !t0.IsLikelyFloat() && !t1.IsLikelyFloat() &&   // neither is likely float
                    (t0.IsLikelyInt() || t1.IsLikelyInt())          // one is likely int
                ));

            if(!(
                    t0.IsObject() && t1.IsObject() &&                                                       // both are objects
                    (
                        t0.GetObjectType() == ObjectType::UninitializedObject ||
                        t1.GetObjectType() == ObjectType::UninitializedObject
                    ) &&                                                                                    // one has an uninitialized object type
                    (t0.GetObjectType() > ObjectType::Object || t1.GetObjectType() > ObjectType::Object)    // one has a specific object type
                ))                                                                                          // then the resulting object type is not guaranteed
            {
                Assert(m.IsNotInt() == (t0.IsNotInt() && t1.IsNotInt()));
            }

            Assert(m.IsFloat() == (t0.IsNumber() && t1.IsNumber() && (t0.IsFloat() || t1.IsFloat())));
            Assert(
                m.IsLikelyFloat() ==
                (
                    (t0.IsLikelyFloat() || t1.IsLikelyFloat()) &&       // one is likely float
                    (t0.IsLikelyUndefined() || t0.IsLikelyNumber()) &&
                    (t1.IsLikelyUndefined() || t1.IsLikelyNumber())     // both are likely undefined or number
                ));

            Assert(m.IsNumber() == (t0.IsNumber() && t1.IsNumber()));
            Assert(
                m.IsLikelyNumber() ==
                (
                    (t0.IsLikelyNumber() || t1.IsLikelyNumber()) &&     // one is likely number
                    (t0.IsLikelyUndefined() || t0.IsLikelyNumber()) &&
                    (t1.IsLikelyUndefined() || t1.IsLikelyNumber())     // both are likely undefined or number
                ));

            Assert(m.IsUnknownNumber() == (m.IsNumber() && !m.IsLikelyInt() && !m.IsLikelyFloat()));
            Assert(!m.IsLikelyUnknownNumber() || m.IsLikelyNumber() && !m.IsLikelyInt() && !m.IsLikelyFloat());

            Assert(m.IsUndefined() == (t0.IsUndefined() && t1.IsUndefined()));
            Assert(m.IsLikelyUndefined() == (t0.IsLikelyUndefined() && t1.IsLikelyUndefined()));

            Assert(m.IsNull() == (t0.IsNull() && t1.IsNull()));
            Assert(m.IsLikelyNull() == (t0.IsLikelyNull() && t1.IsLikelyNull()));

            Assert(m.IsBoolean() == (t0.IsBoolean() && t1.IsBoolean()));
            Assert(m.IsLikelyBoolean() == (t0.IsLikelyBoolean() && t1.IsLikelyBoolean()));

            Assert(m.IsString() == (t0.IsString() && t1.IsString()));
            Assert(m.IsLikelyString() == (t0.IsLikelyString() && t1.IsLikelyString()));

            if(!(
                    t0.IsObject() && t1.IsObject() &&                                                       // both are objects
                    (
                        t0.GetObjectType() == ObjectType::UninitializedObject ||
                        t1.GetObjectType() == ObjectType::UninitializedObject
                    ) &&                                                                                    // one has an uninitialized object type
                    (t0.GetObjectType() > ObjectType::Object || t1.GetObjectType() > ObjectType::Object)    // one has a specific object type
                ))                                                                                          // then the resulting object type is not guaranteed
            {
                Assert(m.IsObject() == (t0.IsObject() && t1.IsObject()));
            }
            Assert(
                m.IsLikelyObject() ==
                (
                    (t0.IsLikelyObject() || t1.IsLikelyObject()) &&                             // one is likely object
                    (t0.IsLikelyUndefined() || t0.IsLikelyNull() || t0.IsLikelyObject()) &&
                    (t1.IsLikelyUndefined() || t1.IsLikelyNull() || t1.IsLikelyObject())        // both are likely undefined, null, or object
                ));

            if(t1.IsUnknownNumber())
            {
                Assert(isSubsetWithTypeSpecEnabled == (t0.IsNumber() || t0.IsLikelyInt() || t0.IsLikelyFloat()));
                Assert(t0.IsSubsetOf(t1, false, true, true, true) == (t0.IsNumber() || t0.IsLikelyFloat()));
                Assert(t0.IsSubsetOf(t1, true, false, true, true) == (t0.IsNumber() || t0.IsLikelyInt()));
            }
            else if(t0.IsLikelyInt() && t1.IsLikelyInt())
            {
                Assert(
                    isSubsetWithTypeSpecEnabled ==
                    (
                        (t0.IsDefinite() || !t1.IsDefinite()) &&
                        (
                            t0.IsTaggedInt() ||
                            t0.IsLikelyTaggedInt() && !t1.IsTaggedInt() ||
                            !t1.IsLikelyTaggedInt()
                        )
                    ));
            }
            else if(t0.IsLikelyFloat() && t1.IsLikelyFloat())
            {
                Assert(isSubsetWithTypeSpecEnabled == (t0.IsDefinite() || !t1.IsDefinite()));
            }
            else if(t0.IsLikelyNumber() && t1.IsLikelyNumber())
            {
                Assert(
                    isSubsetWithTypeSpecEnabled ==
                    (
                        (t0.IsDefinite() || !t1.IsDefinite()) &&
                        (
                            t0.IsLikelyInt() && !t1.IsLikelyFloat() ||
                            t0.IsLikelyFloat() && !t1.IsLikelyInt() ||
                            t1.IsLikelyUnknownNumber()
                        )
                    ));
            }
            else if(t0.IsLikelyObject() && (t1.IsLikelyUndefined() || t1.IsLikelyNull()))
            {
                Assert(isSubsetWithTypeSpecEnabled);
            }
            else if(t0.IsLikelyObject() && t1.IsLikelyObject())
            {
                if(t1.GetObjectType() == ObjectType::UninitializedObject &&
                    t0.GetObjectType() != ObjectType::UninitializedObject)
                {
                    Assert(isSubsetWithTypeSpecEnabled);
                }
                else if(!t0.IsDefinite() && t1.IsDefinite() || t0.GetObjectType() != t1.GetObjectType())
                {
                    Assert(!isSubsetWithTypeSpecEnabled);
                }
                else if(
                    t0.IsDefinite() && !t1.IsDefinite() ||
                    t0.GetObjectType() != ObjectType::ObjectWithArray && t0.GetObjectType() != ObjectType::Array)
                {
                    Assert(isSubsetWithTypeSpecEnabled);
                }
                else
                {
                    Assert(
                        isSubsetWithTypeSpecEnabled ==
                        (
                            (t0.HasNoMissingValues() || !t1.HasNoMissingValues()) &&
                            (
                                (!t0.HasNonInts() || t1.HasNonInts()) && (!t0.HasNonFloats() || t1.HasNonFloats())
                            )
                        ));
                    Assert(
                        t0.IsSubsetOf(t1, true, true, false, true) ==
                        (
                            (!t0.HasNonInts() || t1.HasNonInts()) && (!t0.HasNonFloats() || t1.HasNonFloats())
                        ));
                    Assert(t0.IsSubsetOf(t1, true, true, false, false));
                }
            }
            else
            {
                Assert(
                    isSubsetWithTypeSpecEnabled ==
                    (
                        (t0.IsDefinite() || !t1.IsDefinite()) &&
                        !t0.IsLikelyObject() && !t1.IsLikelyObject() &&
                        t1.AllOn(t0.bits)
                    ));
            }

            return false;
        });
        return false;
    });
}

#endif

void ValueType::InstantiateForceInlinedMembers()
{
    // Force-inlined functions defined in a translation unit need a reference from an extern non-force-inlined function in the
    // same translation unit to force an instantiation of the force-inlined function. Otherwise, if the force-inlined function
    // is not referenced in the same translation unit, it will not be generated and the linker is not able to find the
    // definition to inline the function in other translation units.
    AnalysisAssert(false);

    const Js::Var var = nullptr;

    ValueType *const t = nullptr;
    t->Merge(*t);
    t->Merge(var);
}

bool ValueTypeComparer::Equals(const ValueType t0, const ValueType t1)
{
    return t0 == t1;
}

uint ValueTypeComparer::GetHashCode(const ValueType t)
{
    return t.GetHashCode();
}
