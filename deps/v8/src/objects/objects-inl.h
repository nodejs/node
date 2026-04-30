// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECTS_INL_H_
#define V8_OBJECTS_OBJECTS_INL_H_

// Review notes:
//
// - The use of macros in these inline functions may seem superfluous
// but it is absolutely needed to make sure gcc generates optimal
// code. gcc is not happy when attempting to inline too deep.

#include "src/objects/objects.h"
// Include the non-inl header before the rest of the headers.

#include "include/v8-internal.h"
#include "src/base/bits.h"
#include "src/base/bounds.h"
#include "src/base/memory.h"
#include "src/base/numbers/double.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-verifier.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/read-only-heap-inl.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/allocation-site.h"
#include "src/objects/casting.h"
#include "src/objects/deoptimization-data.h"
#include "src/objects/field-type.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/hole.h"
#include "src/objects/instance-type-checker.h"
#include "src/objects/js-proxy-inl.h"  // TODO(jkummerow): Drop.
#include "src/objects/keys.h"
#include "src/objects/literal-objects.h"
#include "src/objects/lookup-inl.h"  // TODO(jkummerow): Drop.
#include "src/objects/map-word-inl.h"
#include "src/objects/number-string-cache-inl.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/object-predicates-inl.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/property-details.h"
#include "src/objects/property.h"
#include "src/objects/regexp-match-info-inl.h"
#include "src/objects/scope-info-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots-inl.h"
#include "src/objects/slots.h"
#include "src/objects/smi-inl.h"
#include "src/objects/tagged-field-inl.h"
#include "src/objects/tagged-impl-inl.h"
#include "src/objects/tagged-index.h"
#include "src/objects/templates.h"
#include "src/objects/trusted-pointer-inl.h"
#include "src/roots/roots.h"
#include "src/sandbox/bounded-size-inl.h"
#include "src/sandbox/cppheap-pointer-inl.h"
#include "src/sandbox/external-pointer-inl.h"
#include "src/sandbox/indirect-pointer-inl.h"
#include "src/sandbox/isolate-inl.h"
#include "src/sandbox/isolate.h"
#include "src/sandbox/sandboxed-pointer-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Managed;
template <typename T>
class TrustedManaged;

PropertyDetails::PropertyDetails(Tagged<Smi> smi) { value_ = smi.value(); }

Tagged<Smi> PropertyDetails::AsSmi() const {
  // Ensure the upper 2 bits have the same value by sign extending it. This is
  // necessary to be able to use the 31st bit of the property details.
  int value = value_ << 1;
  return Smi::FromInt(value >> 1);
}

int PropertyDetails::field_width_in_words() const {
  DCHECK_EQ(location(), PropertyLocation::kField);
  return 1;
}

// TODO(leszeks): Expand Is<T> to all types.
#define IS_HELPER_DEF(Type, ...)                             \
  template <>                                                \
  struct CastTraits<Type> {                                  \
    static inline bool AllowFrom(Tagged<Object> value) {     \
      return Is##Type(value);                                \
    }                                                        \
    static inline bool AllowFrom(Tagged<HeapObject> value) { \
      return Is##Type(value);                                \
    }                                                        \
  };
HEAP_OBJECT_ORDINARY_TYPE_LIST(IS_HELPER_DEF)
HEAP_OBJECT_TRUSTED_TYPE_LIST(IS_HELPER_DEF)
VIRTUAL_OBJECT_TYPE_LIST(IS_HELPER_DEF)
HOLE_LIST(IS_HELPER_DEF)
ODDBALL_LIST(IS_HELPER_DEF)

#define IS_HELPER_DEF_STRUCT(NAME, Name, name) IS_HELPER_DEF(Name)
STRUCT_LIST(IS_HELPER_DEF_STRUCT)
#undef IS_HELPER_DEF_STRUCT

IS_HELPER_DEF(Number)
#undef IS_HELPER_DEF

template <typename... T>
struct CastTraits<Union<T...>> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return (Is<T>(value) || ...);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return (Is<T>(value) || ...);
  }
};
template <>
struct CastTraits<JSPrimitive> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return IsPrimitive(value);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return IsPrimitive(value);
  }
};
template <>
struct CastTraits<JSAny> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return IsPrimitive(value) || IsJSReceiver(value);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return IsPrimitive(value) || IsJSReceiver(value);
  }
};
template <>
struct CastTraits<AllocationSiteWithWeakNext> {
  template <typename From>
  static inline bool AllowFrom(Tagged<From> value) {
    Tagged<AllocationSite> site;
    return TryCast<AllocationSite>(value, &site) && site->HasWeakNext();
  }
};

template <>
struct CastTraits<FieldType> {
  static inline bool AllowFrom(Tagged<Object> value) {
    return value == FieldType::None() || value == FieldType::Any() ||
           IsMap(value);
  }
  static inline bool AllowFrom(Tagged<HeapObject> value) {
    return IsMap(value);
  }
};

template <typename T>
struct CastTraits<Managed<T>> : public CastTraits<Foreign> {};
template <typename T>
struct CastTraits<TrustedManaged<T>> : public CastTraits<TrustedForeign> {};
template <typename T>
struct CastTraits<PodArray<T>> : public CastTraits<ByteArray> {};
template <typename T>
struct CastTraits<TrustedPodArray<T>> : public CastTraits<TrustedByteArray> {};
template <typename T, typename Base>
struct CastTraits<FixedIntegerArrayBase<T, Base>> : public CastTraits<Base> {};
template <>
struct CastTraits<TrustedFixedAddressArray>
    : public CastTraits<TrustedByteArray> {};

template <>
struct CastTraits<JSRegExpResultIndices> : public CastTraits<JSArray> {};
template <>
struct CastTraits<DeoptimizationLiteralArray>
    : public CastTraits<TrustedWeakFixedArray> {};
template <>
struct CastTraits<FreshlyAllocatedBigInt> : public CastTraits<BigInt> {};
template <>
struct CastTraits<JSIteratorResult> : public CastTraits<JSObject> {};
template <>
struct CastTraits<JSUint8ArraySetFromResult> : public CastTraits<JSObject> {};

template <>
struct CastTraits<DeoptimizationFrameTranslation>
    : public CastTraits<TrustedByteArray> {};

template <class T>
T HeapObject::Relaxed_ReadField(size_t offset) const
  requires((std::is_arithmetic_v<T> || std::is_enum_v<T>) &&
           !std::is_floating_point_v<T>)
{
  // Pointer compression causes types larger than kTaggedSize to be
  // unaligned. Atomic loads must be aligned.
  DCHECK_IMPLIES(COMPRESS_POINTERS_BOOL, sizeof(T) <= kTaggedSize);
  using AtomicT = typename base::AtomicTypeFromByteWidth<sizeof(T)>::type;
  return static_cast<T>(base::AsAtomicImpl<AtomicT>::Relaxed_Load(
      reinterpret_cast<AtomicT*>(field_address(offset))));
}

template <class T>
void HeapObject::Relaxed_WriteField(size_t offset, T value)
  requires((std::is_arithmetic_v<T> || std::is_enum_v<T>) &&
           !std::is_floating_point_v<T>)
{
  // Pointer compression causes types larger than kTaggedSize to be
  // unaligned. Atomic stores must be aligned.
  DCHECK_IMPLIES(COMPRESS_POINTERS_BOOL, sizeof(T) <= kTaggedSize);
  using AtomicT = typename base::AtomicTypeFromByteWidth<sizeof(T)>::type;
  base::AsAtomicImpl<AtomicT>::Relaxed_Store(
      reinterpret_cast<AtomicT*>(field_address(offset)),
      static_cast<AtomicT>(value));
}

template <class T>
T HeapObjectLayout::Relaxed_ReadField(size_t offset) const
  requires((std::is_arithmetic_v<T> || std::is_enum_v<T>) &&
           !std::is_floating_point_v<T>)
{
  DCHECK_IMPLIES(COMPRESS_POINTERS_BOOL, sizeof(T) <= kTaggedSize);
  using AtomicT = typename base::AtomicTypeFromByteWidth<sizeof(T)>::type;
  return static_cast<T>(base::AsAtomicImpl<AtomicT>::Relaxed_Load(
      reinterpret_cast<AtomicT*>(field_address(offset))));
}

template <class T>
void HeapObjectLayout::Relaxed_WriteField(size_t offset, T value)
  requires((std::is_arithmetic_v<T> || std::is_enum_v<T>) &&
           !std::is_floating_point_v<T>)
{
  DCHECK_IMPLIES(COMPRESS_POINTERS_BOOL, sizeof(T) <= kTaggedSize);
  using AtomicT = typename base::AtomicTypeFromByteWidth<sizeof(T)>::type;
  base::AsAtomicImpl<AtomicT>::Relaxed_Store(
      reinterpret_cast<AtomicT*>(field_address(offset)),
      static_cast<AtomicT>(value));
}

template <class T>
T HeapObject::Acquire_ReadField(size_t offset) const
  requires((std::is_arithmetic_v<T> || std::is_enum_v<T>) &&
           !std::is_floating_point_v<T>)
{
  // Pointer compression causes types larger than kTaggedSize to be
  // unaligned. Atomic loads must be aligned.
  DCHECK_IMPLIES(COMPRESS_POINTERS_BOOL, sizeof(T) <= kTaggedSize);
  using AtomicT = typename base::AtomicTypeFromByteWidth<sizeof(T)>::type;
  return static_cast<T>(base::AsAtomicImpl<AtomicT>::Acquire_Load(
      reinterpret_cast<AtomicT*>(field_address(offset))));
}

// static
template <typename CompareAndSwapImpl>
Tagged<Object> HeapObject::SeqCst_CompareAndSwapField(
    Tagged<Object> expected, Tagged<Object> value,
    CompareAndSwapImpl compare_and_swap_impl) {
  Tagged<Object> actual_expected = expected;
  do {
    Tagged<Object> old_value = compare_and_swap_impl(actual_expected, value);
    if (old_value == actual_expected || !IsNumber(old_value) ||
        !IsNumber(actual_expected)) {
      return old_value;
    }
    if (!Object::SameNumberValue(
            Object::NumberValue(Cast<Number>(old_value)),
            Object::NumberValue(Cast<Number>(actual_expected)))) {
      return old_value;
    }
    // The pointer comparison failed, but the numbers are equal. This can
    // happen even if both numbers are HeapNumbers with the same value.
    // Try again in the next iteration.
    actual_expected = old_value;
  } while (true);
}

constexpr bool FastInReadOnlySpaceOrSmallSmi(Tagged_t obj) {
#if V8_STATIC_ROOTS_BOOL && CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
  // The following assert ensures that the page size check covers all our static
  // roots. This is not strictly necessary and can be relaxed in future as the
  // most prominent static roots are anyways allocated at the beginning of the
  // first page.
  // This optimization requires contiguous compressed RO space to ensure RO
  // space is at the beginning of the cage; otherwise, objects from other spaces
  // could alias with low addresses.
  constexpr int kLastStaticRootPage =
      RoundUp<kRegularPageSize>(StaticReadOnlyRoot::kLastAllocatedRoot);
  static_assert(kLastStaticRootPage <= kContiguousReadOnlyReservationSize);
  return obj < kContiguousReadOnlyReservationSize;
#else
  return false;
#endif
}

constexpr bool FastInReadOnlySpaceOrSmallSmi(Tagged<MaybeObject> obj) {
#ifdef V8_COMPRESS_POINTERS
  // This check is only valid for objects in the main cage.
  DCHECK(obj.IsSmi() || obj.IsInMainCageBase());
  return FastInReadOnlySpaceOrSmallSmi(
      V8HeapCompressionScheme::CompressAny(obj.ptr()));
#else   // V8_COMPRESS_POINTERS
  return false;
#endif  // V8_COMPRESS_POINTERS
}

bool OutsideSandboxOrInReadonlySpace(Tagged<HeapObject> obj) {
#ifdef V8_ENABLE_SANDBOX
  return OutsideSandbox(obj.address()) ||
         MemoryChunk::FromHeapObject(obj)->SandboxSafeInReadOnlySpace();
#else
  return true;
#endif
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsUniqueName) {
  return IsInternalizedString(obj, cage_base) || IsSymbol(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsCallable) {
  return obj->map(cage_base)->is_callable();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsCallableJSProxy) {
  return IsCallable(obj, cage_base) && IsJSProxy(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsCallableApiObject) {
  InstanceType type = obj->map(cage_base)->instance_type();
  return IsCallable(obj, cage_base) &&
         (type == JS_API_OBJECT_TYPE || type == JS_SPECIAL_API_OBJECT_TYPE);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsNonNullForeign) {
  return IsForeign(obj, cage_base) &&
         Cast<Foreign>(obj)->foreign_address_unchecked() != kNullAddress;
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsConstructor) {
  return obj->map(cage_base)->is_constructor();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsSourceTextModuleInfo) {
  return obj->map(cage_base) == GetReadOnlyRoots().module_info_map();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsConsString) {
  if (!IsString(obj, cage_base)) return false;
  return StringShape(Cast<String>(obj)->map()).IsCons();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsThinString) {
  if (!IsString(obj, cage_base)) return false;
  return StringShape(Cast<String>(obj)->map()).IsThin();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsSlicedString) {
  if (!IsString(obj, cage_base)) return false;
  return StringShape(Cast<String>(obj)->map()).IsSliced();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsSeqString) {
  if (!IsString(obj, cage_base)) return false;
  return StringShape(Cast<String>(obj)->map()).IsSequential();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsSeqOneByteString) {
  if (!IsString(obj, cage_base)) return false;
  return StringShape(Cast<String>(obj)->map()).IsSequentialOneByte();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsSeqTwoByteString) {
  if (!IsString(obj, cage_base)) return false;
  return StringShape(Cast<String>(obj)->map()).IsSequentialTwoByte();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsExternalOneByteString) {
  if (!IsString(obj, cage_base)) return false;
  return StringShape(Cast<String>(obj)->map()).IsExternalOneByte();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsExternalTwoByteString) {
  if (!IsString(obj, cage_base)) return false;
  return StringShape(Cast<String>(obj)->map()).IsExternalTwoByte();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsTemplateLiteralObject) {
  return IsJSArray(obj, cage_base);
}

#if V8_INTL_SUPPORT
DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsJSSegmentDataObject) {
  return IsJSObject(obj, cage_base);
}
DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsJSSegmentDataObjectWithIsWordLike) {
  return IsJSObject(obj, cage_base);
}
#endif  // V8_INTL_SUPPORT

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsDeoptimizationData) {
  if (!Is<ProtectedFixedArray>(obj)) return false;
  Tagged<ProtectedFixedArray> array = TrustedCast<ProtectedFixedArray>(obj);

  // There's no sure way to detect the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can
  // check that the length is zero or else the fixed size plus a multiple of
  // the entry size.
  uint32_t length = array->ulength().value();
  if (length == 0) return true;

  if (length < DeoptimizationData::kFirstDeoptEntryIndex) return false;
  length -= DeoptimizationData::kFirstDeoptEntryIndex;
  return length % DeoptimizationData::kDeoptEntrySize == 0;
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsHandlerTable) {
  return IsFixedArrayExact(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsDependentCode) {
  return IsWeakArrayList(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsOSROptimizedCodeCache) {
  return IsWeakFixedArray(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsStringWrapper) {
  return IsJSPrimitiveWrapper(obj, cage_base) &&
         IsString(Cast<JSPrimitiveWrapper>(obj)->value(), cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsBooleanWrapper) {
  return IsJSPrimitiveWrapper(obj, cage_base) &&
         IsBoolean(Cast<JSPrimitiveWrapper>(obj)->value(), cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsScriptWrapper) {
  return IsJSPrimitiveWrapper(obj, cage_base) &&
         IsScript(Cast<JSPrimitiveWrapper>(obj)->value(), cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsNumberWrapper) {
  return IsJSPrimitiveWrapper(obj, cage_base) &&
         IsNumber(Cast<JSPrimitiveWrapper>(obj)->value(), cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsBigIntWrapper) {
  return IsJSPrimitiveWrapper(obj, cage_base) &&
         IsBigInt(Cast<JSPrimitiveWrapper>(obj)->value(), cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsSymbolWrapper) {
  return IsJSPrimitiveWrapper(obj, cage_base) &&
         IsSymbol(Cast<JSPrimitiveWrapper>(obj)->value(), cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsStringSet) {
  return IsHashTable(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsObjectHashSet) {
  return IsHashTable(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsCompilationCacheTable) {
  return IsHashTable(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsMapCache) {
  return IsHashTable(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsObjectHashTable) {
  return IsHashTable(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsObjectTwoHashTable) {
  return IsHashTable(obj, cage_base);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsHashTableBase) {
  return IsHashTable(obj, cage_base);
}

// static
Maybe<bool> Object::IsArray(DirectHandle<Object> object) {
  if (IsSmi(*object)) return Just(false);
  auto heap_object = Cast<HeapObject>(object);
  if (IsJSArray(*heap_object)) return Just(true);
  if (!IsJSProxy(*heap_object)) return Just(false);
  return JSProxy::IsArray(Cast<JSProxy>(object));
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsUndetectable) {
  return obj->map(cage_base)->is_undetectable();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsAccessCheckNeeded) {
  if (IsJSGlobalProxy(obj, cage_base)) {
    const Tagged<JSGlobalProxy> proxy = Cast<JSGlobalProxy>(obj);
    Isolate* isolate = Isolate::Current();
    // TODO(ishell): compare security tokens here in order to allow ICs to
    // take fast paths for cross context accesses.
    Tagged<JSGlobalObject> global = isolate->context()->global_object();
    return proxy->IsDetachedFrom(global);
  }
  return obj->map(cage_base)->is_access_check_needed();
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsSmiStringCache) {
  return IsFixedArray(obj);
}

// static
double Object::NumberValue(Tagged<Number> obj) {
  DCHECK(IsNumber(obj));
  return IsSmi(obj) ? static_cast<double>(UncheckedCast<Smi>(obj).value())
                    : UncheckedCast<HeapNumber>(obj)->value();
}
// TODO(leszeks): Remove in favour of Tagged<Number>
// static
double Object::NumberValue(Tagged<Object> obj) {
  return NumberValue(Cast<Number>(obj));
}
double Object::NumberValue(Tagged<HeapNumber> obj) {
  return NumberValue(Cast<Number>(obj));
}
double Object::NumberValue(Tagged<Smi> obj) {
  return NumberValue(Cast<Number>(obj));
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
Maybe<double> Object::IntegerValue(Isolate* isolate, HandleType<T> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ConvertToNumber(isolate, input));
  if (IsSmi(*input)) {
    return Just(static_cast<double>(Cast<Smi>(*input).value()));
  }
  return Just(DoubleToInteger(Cast<HeapNumber>(*input)->value()));
}

// static
bool Object::SameNumberValue(double value1, double value2) {
  // Compare values bitwise, to cover -0 being different from 0 -- we'd need to
  // look at sign bits anyway if we'd done a double comparison, so we may as
  // well compare bitwise immediately.
  uint64_t value1_bits = base::bit_cast<uint64_t>(value1);
  uint64_t value2_bits = base::bit_cast<uint64_t>(value2);
  if (value1_bits == value2_bits) {
    return true;
  }
  // SameNumberValue(NaN, NaN) is true even for NaNs with different bit
  // representations.
  return std::isnan(value1) && std::isnan(value2);
}

// static
bool IsNaN(Tagged<Object> obj) {
  return IsHeapNumber(obj) && std::isnan(Cast<HeapNumber>(obj)->value());
}

// static
bool IsMinusZero(Tagged<Object> obj) {
  return IsHeapNumber(obj) && i::IsMinusZero(Cast<HeapNumber>(obj)->value());
}

// static
bool Object::HasValidElements(Tagged<Object> obj) {
  // Dictionary is covered under FixedArray. ByteArray is used
  // for the JSTypedArray backing stores.
  return IsFixedArray(obj) || IsFixedDoubleArray(obj) || IsByteArray(obj);
}

// static
bool Object::FilterKey(Tagged<Object> obj, PropertyFilter filter) {
  DCHECK(!IsPropertyCell(obj));
  if (filter == PRIVATE_NAMES_ONLY) {
    if (!IsSymbol(obj)) return true;
    return !Cast<Symbol>(obj)->is_any_private_name();
  } else if (IsSymbol(obj)) {
    if (filter & SKIP_SYMBOLS) return true;

    if (Cast<Symbol>(obj)->is_any_private()) return true;
  } else {
    if (filter & SKIP_STRINGS) return true;
  }
  return false;
}

// static
std::pair<Representation, PropertyConstness> Object::OptimalRepresentation(
    Tagged<Object> obj, PropertyConstness constness,
    PtrComprCageBase cage_base) {
  if (IsSmi(obj)) {
    return {Representation::Smi(), constness};
  }
  Tagged<HeapObject> heap_object = Cast<HeapObject>(obj);
  if (IsUninitializedHole(heap_object)) {
    return {Representation::None(), constness};
  }
  if (IsHeapNumber(heap_object, cage_base)) {
    if (constness == PropertyConstness::kConst &&
        Cast<HeapNumber>(heap_object)->is_the_hole()) {
      // Make sure that even an initializing store of a double value with
      // the hole NaN pattern removes constness, otherwise it wouldn't be
      // possible to distinguish whether subsequent stores to a double field
      // is initializing or not.
      constness = PropertyConstness::kMutable;
    }
    return {Representation::Double(), constness};
  }
  return {Representation::HeapObject(), constness};
}

// static
ElementsKind Object::OptimalElementsKind(Tagged<Object> obj,
                                         PtrComprCageBase cage_base) {
  if (IsSmi(obj)) return PACKED_SMI_ELEMENTS;
  Tagged<HeapObject> heap_object = Cast<HeapObject>(obj);
  if (IsHeapNumber(heap_object, cage_base)) return PACKED_DOUBLE_ELEMENTS;
  // if (IsUninitializedHole(heap_object)) {
  //   return PACKED_SMI_ELEMENTS;
  // }
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
  if (IsUndefined(heap_object, GetReadOnlyRoots())) {
    return HOLEY_DOUBLE_ELEMENTS;
  }
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
  return PACKED_ELEMENTS;
}

// static
bool Object::FitsRepresentation(Tagged<Object> obj,
                                Representation representation,
                                bool allow_coercion) {
  if (representation.IsSmi()) {
    return IsSmi(obj);
  } else if (representation.IsDouble()) {
    return allow_coercion ? IsNumber(obj) : IsHeapNumber(obj);
  } else if (representation.IsHeapObject()) {
    return IsHeapObject(obj);
  } else if (representation.IsNone()) {
    return false;
  }
  return true;
}

// static
bool Object::ToUint32(Tagged<Object> obj, uint32_t* value) {
  if (IsSmi(obj)) {
    int num = Smi::ToInt(obj);
    if (num < 0) return false;
    *value = static_cast<uint32_t>(num);
    return true;
  }
  if (IsHeapNumber(obj)) {
    double num = Cast<HeapNumber>(obj)->value();
    return DoubleToUint32IfEqualToSelf(num, value);
  }
  return false;
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<JSReceiver>::MaybeType Object::ToObject(
    Isolate* isolate, HandleType<T> object, const char* method_name) {
  if (V8_LIKELY(IsJSReceiver(*object))) return Cast<JSReceiver>(object);
  return ToObjectImpl(isolate, object, method_name);
}

// static
template <template <typename> typename HandleType>
typename HandleType<Name>::MaybeType Object::ToName(Isolate* isolate,
                                                    HandleType<Object> input)
  requires(std::is_convertible_v<HandleType<Object>, DirectHandle<Object>>)
{
  if (IsName(*input)) return Cast<Name>(input);
  return ConvertToName(isolate, input);
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Object>::MaybeType Object::ToPropertyKey(
    Isolate* isolate, HandleType<T> value) {
  if (IsSmi(*value) || IsName(Cast<HeapObject>(*value))) return value;
  return ConvertToPropertyKey(isolate, value);
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Object>::MaybeType Object::ToPrimitive(
    Isolate* isolate, HandleType<T> input, ToPrimitiveHint hint) {
  if (IsPrimitive(*input)) return input;
  return JSReceiver::ToPrimitive(isolate, Cast<JSReceiver>(input), hint);
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Number>::MaybeType Object::ToNumber(Isolate* isolate,
                                                        HandleType<T> input) {
  if (IsNumber(*input)) return Cast<Number>(input);  // Shortcut.
  return ConvertToNumber(isolate, Cast<Object>(input));
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Object>::MaybeType Object::ToNumeric(Isolate* isolate,
                                                         HandleType<T> input) {
  if (IsNumber(*input) || IsBigInt(*input)) return input;  // Shortcut.
  return ConvertToNumeric(isolate, Cast<Object>(input));
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Number>::MaybeType Object::ToInteger(Isolate* isolate,
                                                         HandleType<T> input) {
  if (IsSmi(*input)) return Cast<Smi>(input);
  return ConvertToInteger(isolate, Cast<Object>(input));
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Number>::MaybeType Object::ToInt32(Isolate* isolate,
                                                       HandleType<T> input) {
  if (IsSmi(*input)) return Cast<Smi>(input);
  return ConvertToInt32(isolate, Cast<Object>(input));
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Number>::MaybeType Object::ToUint32(Isolate* isolate,
                                                        HandleType<T> input) {
  if (IsSmi(*input)) {
    return typename HandleType<Number>::MaybeType(
        Smi::ToUint32Smi(Cast<Smi>(*input)), isolate);
  }
  return ConvertToUint32(isolate, Cast<Object>(input));
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<String>::MaybeType Object::ToString(Isolate* isolate,
                                                        HandleType<T> input) {
  if (IsString(*input)) return Cast<String>(input);
  return ConvertToString(isolate, Cast<Object>(input));
}

// static
MaybeHandle<Object> Object::ToLength(Isolate* isolate,
                                     DirectHandle<Object> input) {
  if (IsSmi(*input)) {
    int value = std::max(Smi::ToInt(*input), 0);
    return handle(Smi::FromInt(value), isolate);
  }
  return ConvertToLength(isolate, input);
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Object>::MaybeType Object::ToIndex(
    Isolate* isolate, HandleType<T> input, MessageTemplate error_index) {
  if (IsSmi(*input) && Smi::ToInt(*input) >= 0) return input;
  return ConvertToIndex(isolate, Cast<Object>(input), error_index);
}

MaybeHandle<Object> Object::GetProperty(Isolate* isolate,
                                        DirectHandle<JSAny> object,
                                        DirectHandle<Name> name) {
  LookupIterator it(isolate, object, name);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetProperty(&it);
}

MaybeHandle<Object> Object::GetElement(Isolate* isolate,
                                       DirectHandle<JSAny> object,
                                       uint32_t index) {
  LookupIterator it(isolate, object, index);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetProperty(&it);
}

MaybeDirectHandle<Object> Object::SetElement(Isolate* isolate,
                                             DirectHandle<JSAny> object,
                                             uint32_t index,
                                             DirectHandle<Object> value,
                                             ShouldThrow should_throw) {
  LookupIterator it(isolate, object, index);
  MAYBE_RETURN_NULL(
      SetProperty(&it, value, StoreOrigin::kMaybeKeyed, Just(should_throw)));
  return value;
}

Address HeapObject::ReadSandboxedPointerField(
    size_t offset, PtrComprCageBase cage_base) const {
  return i::ReadSandboxedPointerField(field_address(offset), cage_base);
}

void HeapObject::WriteSandboxedPointerField(size_t offset,
                                            PtrComprCageBase cage_base,
                                            Address value) {
  i::WriteSandboxedPointerField(field_address(offset), cage_base, value);
}

void HeapObject::WriteSandboxedPointerField(size_t offset, Isolate* isolate,
                                            Address value) {
  i::WriteSandboxedPointerField(field_address(offset),
                                PtrComprCageBase(isolate), value);
}

Address HeapObjectLayout::ReadSandboxedPointerField(
    size_t offset, PtrComprCageBase cage_base) const {
  return i::ReadSandboxedPointerField(field_address(offset), cage_base);
}

void HeapObjectLayout::WriteSandboxedPointerField(size_t offset,
                                                  PtrComprCageBase cage_base,
                                                  Address value) {
  i::WriteSandboxedPointerField(field_address(offset), cage_base, value);
}

void HeapObjectLayout::WriteSandboxedPointerField(size_t offset,
                                                  Isolate* isolate,
                                                  Address value) {
  i::WriteSandboxedPointerField(field_address(offset),
                                PtrComprCageBase(isolate), value);
}

size_t HeapObject::ReadBoundedSizeField(size_t offset) const {
  return i::ReadBoundedSizeField(field_address(offset));
}

void HeapObject::WriteBoundedSizeField(size_t offset, size_t value) {
  i::WriteBoundedSizeField(field_address(offset), value);
}

size_t HeapObjectLayout::ReadBoundedSizeField(size_t offset) const {
  return i::ReadBoundedSizeField(field_address(offset));
}

void HeapObjectLayout::WriteBoundedSizeField(size_t offset, size_t value) {
  i::WriteBoundedSizeField(field_address(offset), value);
}

template <ExternalPointerTag tag>
void HeapObject::InitExternalPointerField(size_t offset,
                                          IsolateForSandbox isolate,
                                          Address value,
                                          WriteBarrierMode mode) {
  i::InitExternalPointerField<tag>(address(), field_address(offset), isolate,
                                   value);
  CONDITIONAL_EXTERNAL_POINTER_WRITE_BARRIER(*this, static_cast<int>(offset),
                                             tag, mode);
}

void HeapObject::InitExternalPointerField(size_t offset,
                                          IsolateForSandbox isolate,
                                          ExternalPointerTag tag, Address value,
                                          WriteBarrierMode mode) {
  i::InitExternalPointerField(address(), field_address(offset), isolate, tag,
                              value);
  CONDITIONAL_EXTERNAL_POINTER_WRITE_BARRIER(*this, static_cast<int>(offset),
                                             tag, mode);
}

template <ExternalPointerTagRange tag_range>
Address HeapObject::ReadExternalPointerField(size_t offset,
                                             IsolateForSandbox isolate) const {
  return i::ReadExternalPointerField<tag_range>(field_address(offset), isolate);
}

Address HeapObject::ReadExternalPointerField(
    size_t offset, IsolateForSandbox isolate,
    ExternalPointerTagRange tag_range) const {
  return i::ReadExternalPointerField(field_address(offset), isolate, tag_range);
}

template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound>
Address HeapObject::ReadCppHeapPointerField(
    size_t offset, IsolateForPointerCompression isolate) const {
  return i::ReadCppHeapPointerField<lower_bound, upper_bound>(
      field_address(offset), isolate);
}

Address HeapObject::ReadCppHeapPointerField(
    size_t offset, IsolateForPointerCompression isolate,
    CppHeapPointerTagRange tag_range) const {
  return i::ReadCppHeapPointerField(field_address(offset), isolate, tag_range);
}

template <ExternalPointerTag tag>
void HeapObject::WriteExternalPointerField(size_t offset,
                                           IsolateForSandbox isolate,
                                           Address value) {
  i::WriteExternalPointerField<tag>(field_address(offset), isolate, value);
}

void HeapObject::WriteExternalPointerField(size_t offset,
                                           IsolateForSandbox isolate,
                                           ExternalPointerTag tag,
                                           Address value) {
  i::WriteExternalPointerField(field_address(offset), isolate, tag, value);
}

void HeapObject::SetupLazilyInitializedExternalPointerField(size_t offset) {
#ifdef V8_ENABLE_SANDBOX
  auto location =
      reinterpret_cast<ExternalPointerHandle*>(field_address(offset));
  base::AsAtomic32::Release_Store(location, kNullExternalPointerHandle);
#else
  WriteMaybeUnalignedValue<Address>(field_address(offset), kNullAddress);
#endif  // V8_ENABLE_SANDBOX
}

bool HeapObject::IsLazilyInitializedExternalPointerFieldInitialized(
    size_t offset) const {
#ifdef V8_ENABLE_SANDBOX
  auto location =
      reinterpret_cast<ExternalPointerHandle*>(field_address(offset));
  ExternalPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  return handle != kNullExternalPointerHandle;
#else
  return ReadMaybeUnalignedValue<Address>(field_address(offset)) !=
         kNullAddress;
#endif  // V8_ENABLE_SANDBOX
}

template <ExternalPointerTag tag>
void HeapObject::WriteLazilyInitializedExternalPointerField(
    size_t offset, IsolateForSandbox isolate, Address value) {
  WriteLazilyInitializedExternalPointerField(offset, isolate, value, tag);
}

void HeapObject::WriteLazilyInitializedExternalPointerField(
    size_t offset, IsolateForSandbox isolate, Address value,
    ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kExternalPointerNullTag);
  ExternalPointerTable& table = isolate.GetExternalPointerTableFor(tag);
  auto location =
      reinterpret_cast<ExternalPointerHandle*>(field_address(offset));
  ExternalPointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  if (handle == kNullExternalPointerHandle) {
    // Field has not been initialized yet.
    handle = table.AllocateAndInitializeEntry(
        isolate.GetExternalPointerTableSpaceFor(tag, address()), value, tag);
    base::AsAtomic32::Release_Store(location, handle);
    // In this case, we're adding a reference from an existing object to a new
    // table entry, so we always require a write barrier.
    EXTERNAL_POINTER_WRITE_BARRIER(*this, static_cast<int>(offset), tag);
  } else {
    table.Set(handle, value, tag);
  }
#else
  WriteMaybeUnalignedValue<Address>(field_address(offset), value);
#endif  // V8_ENABLE_SANDBOX
}

void HeapObject::SetupLazilyInitializedCppHeapPointerField(size_t offset) {
  CppHeapPointerSlot(field_address(offset)).init();
}

void HeapObject::WriteLazilyInitializedCppHeapPointerField(
    size_t offset, IsolateForPointerCompression isolate, Address value,
    CppHeapPointerTag tag) {
  i::WriteLazilyInitializedCppHeapPointerField(field_address(offset), isolate,
                                               value, tag);
}

#if V8_ENABLE_SANDBOX

void HeapObject::InitSelfIndirectPointerField(
    size_t offset, IsolateForSandbox isolate,
    TrustedPointerPublishingScope* opt_publishing_scope) {
  DCHECK(IsExposedTrustedObject(*this));
  InstanceType instance_type = map()->instance_type();
  SharedFlag shared = SharedFlag(HeapLayout::InAnySharedSpace(*this));
  IndirectPointerTag tag =
      IndirectPointerTagFromInstanceType(instance_type, shared);
  i::InitSelfIndirectPointerField(field_address(offset), isolate, *this, tag,
                                  opt_publishing_scope);
}

void HeapObject::InitSelfIndirectPointerFieldWithoutPublishing(
    size_t offset, IsolateForSandbox isolate) {
  DCHECK(IsExposedTrustedObject(*this));
  i::InitSelfIndirectPointerField(field_address(offset), isolate, *this,
                                  kUnpublishedIndirectPointerTag, nullptr);
}

void HeapObjectLayout::InitSelfIndirectPointerField(
    std::atomic<IndirectPointerHandle>* field_ptr, IsolateForSandbox isolate,
    TrustedPointerPublishingScope* opt_publishing_scope) {
  DCHECK(IsExposedTrustedObject(this));
  InstanceType instance_type = map()->instance_type();
  SharedFlag shared = SharedFlag(HeapLayout::InAnySharedSpace(this));
  IndirectPointerTag tag =
      IndirectPointerTagFromInstanceType(instance_type, shared);
  i::InitSelfIndirectPointerField(reinterpret_cast<Address>(field_ptr), isolate,
                                  this, tag, opt_publishing_scope);
}
#endif  // V8_ENABLE_SANDBOX

template <IndirectPointerTagRange tag_range>
inline auto HeapObject::ReadTrustedPointerField(
    size_t offset, IsolateForSandbox isolate) const {
  return TrustedPointerField::ReadTrustedPointerField<tag_range>(*this, offset,
                                                                 isolate);
}

template <IndirectPointerTagRange tag_range>
inline auto HeapObject::ReadTrustedPointerField(
    size_t offset, IsolateForSandbox isolate,
    AcquireLoadTag acquire_load) const {
  return TrustedPointerField::ReadTrustedPointerField<tag_range>(
      *this, offset, isolate, acquire_load);
}

template <IndirectPointerTagRange tag_range>
auto HeapObject::ReadMaybeEmptyTrustedPointerField(
    size_t offset, IsolateForSandbox isolate,
    AcquireLoadTag acquire_load) const {
  return TrustedPointerField::ReadMaybeEmptyTrustedPointerField<tag_range>(
      *this, offset, isolate, acquire_load);
}

template <IndirectPointerTagRange tag_range>
void HeapObject::WriteTrustedPointerField(size_t offset,
                                          Tagged<ExposedTrustedObject> value) {
  TrustedPointerField::WriteTrustedPointerField<tag_range>(*this, offset,
                                                           value);
}

bool HeapObject::IsTrustedPointerFieldEmpty(size_t offset) const {
  return TrustedPointerField::IsTrustedPointerFieldEmpty(*this, offset);
}

bool HeapObject::IsTrustedPointerFieldUnpublished(
    size_t offset, IndirectPointerTagRange tag_range,
    IsolateForSandbox isolate) const {
  return TrustedPointerField::IsTrustedPointerFieldUnpublished(
      *this, offset, tag_range, isolate);
}

void HeapObject::ClearTrustedPointerField(size_t offset) {
  TrustedPointerField::ClearTrustedPointerField(*this, offset);
}

void HeapObject::ClearTrustedPointerField(size_t offset, ReleaseStoreTag) {
  TrustedPointerField::ClearTrustedPointerField(*this, offset, kReleaseStore);
}

Tagged<Code> HeapObject::ReadCodePointerField(size_t offset,
                                              IsolateForSandbox isolate) const {
  return ReadTrustedPointerField<kCodeIndirectPointerTag>(offset, isolate);
}

void HeapObject::WriteCodePointerField(size_t offset, Tagged<Code> value) {
  WriteTrustedPointerField<kCodeIndirectPointerTag>(offset, value);
}

bool HeapObject::IsCodePointerFieldEmpty(size_t offset) const {
  return IsTrustedPointerFieldEmpty(offset);
}

void HeapObject::ClearCodePointerField(size_t offset) {
  ClearTrustedPointerField(offset);
}

// static
template <typename ObjectType>
JSDispatchHandle HeapObject::AllocateAndInstallJSDispatchHandle(
    DirectHandle<ObjectType> host, size_t offset, Isolate* isolate,
    uint16_t parameter_count, DirectHandle<Code> code, WriteBarrierMode mode) {
  JSDispatchTable::Space* space =
      isolate->GetJSDispatchTableSpaceFor(host->field_address(offset));
  JSDispatchHandle handle =
      isolate->factory()->NewJSDispatchHandle(parameter_count, code, space);

  // Use a Release_Store to ensure that the store of the pointer into the table
  // is not reordered after the store of the handle. Otherwise, other threads
  // may access an uninitialized table entry and crash.
  auto location =
      reinterpret_cast<JSDispatchHandle*>(host->field_address(offset));
  base::AsAtomic32::Release_Store(location, handle);
  CONDITIONAL_JS_DISPATCH_HANDLE_WRITE_BARRIER(*host, handle, mode);

  return handle;
}

// static
template <typename ObjectType>
JSDispatchHandle HeapObject::AllocateAndInstallJSDispatchHandle(
    DirectHandle<ObjectType> host, JSDispatchHandle* location, Isolate* isolate,
    uint16_t parameter_count, DirectHandle<Code> code, WriteBarrierMode mode) {
  JSDispatchTable::Space* space = isolate->GetJSDispatchTableSpaceFor(location);
  JSDispatchHandle handle =
      isolate->factory()->NewJSDispatchHandle(parameter_count, code, space);

  // Use a Release_Store to ensure that the store of the pointer into the table
  // is not reordered after the store of the handle. Otherwise, other threads
  // may access an uninitialized table entry and crash.
  base::AsAtomic32::Release_Store(location, handle);
  CONDITIONAL_JS_DISPATCH_HANDLE_WRITE_BARRIER(*host, handle, mode);

  return handle;
}

template <typename ObjectType>
JSDispatchHandle JSDispatchHandleMember::AllocateAndInstall(
    DirectHandle<ObjectType> host, Isolate* isolate, uint16_t parameter_count,
    DirectHandle<Code> code, WriteBarrierMode mode) {
  JSDispatchTable::Space* space =
      isolate->GetJSDispatchTableSpaceFor(reinterpret_cast<Address>(&storage_));
  JSDispatchHandle handle =
      isolate->factory()->NewJSDispatchHandle(parameter_count, code, space);
  storage_.store(static_cast<uint32_t>(handle.value()),
                 std::memory_order_release);
  CONDITIONAL_JS_DISPATCH_HANDLE_WRITE_BARRIER(*host, handle, mode);
  return handle;
}

ObjectSlot HeapObject::RawField(int byte_offset) const {
  return ObjectSlot(field_address(byte_offset));
}

ObjectSlot HeapObjectLayout::RawField(int byte_offset) const {
  return ObjectSlot(field_address(byte_offset));
}

MaybeObjectSlot HeapObjectLayout::RawMaybeWeakField(int byte_offset) const {
  return MaybeObjectSlot(field_address(byte_offset));
}

IndirectPointerSlot HeapObjectLayout::RawIndirectPointerField(
    int byte_offset, IndirectPointerTagRange tag_range) const {
  return IndirectPointerSlot(field_address(byte_offset), tag_range);
}

ExternalPointerSlot HeapObjectLayout::RawExternalPointerField(
    int byte_offset, ExternalPointerTagRange tag_range) const {
  return ExternalPointerSlot(field_address(byte_offset), tag_range);
}

HeapObjectLayout::operator Tagged<HeapObject>() const {
  return Tagged<HeapObject>(this);
}

template <ExternalPointerTag tag>
void HeapObjectLayout::InitExternalPointerField(size_t offset,
                                                IsolateForSandbox isolate,
                                                Address value,
                                                WriteBarrierMode mode) {
  Tagged<HeapObject>(*this)->InitExternalPointerField<tag>(offset, isolate,
                                                           value, mode);
}

template <ExternalPointerTagRange tag_range>
Address HeapObjectLayout::ReadExternalPointerField(
    size_t offset, IsolateForSandbox isolate) const {
  return Tagged<HeapObject>(*this)->ReadExternalPointerField<tag_range>(
      offset, isolate);
}

template <ExternalPointerTag tag>
void HeapObjectLayout::WriteExternalPointerField(size_t offset,
                                                 IsolateForSandbox isolate,
                                                 Address value) {
  Tagged<HeapObject>(*this)->WriteExternalPointerField<tag>(offset, isolate,
                                                            value);
}

void HeapObjectLayout::SetupLazilyInitializedExternalPointerField(
    size_t offset) {
  Tagged<HeapObject>(*this)->SetupLazilyInitializedExternalPointerField(offset);
}

bool HeapObjectLayout::IsLazilyInitializedExternalPointerFieldInitialized(
    size_t offset) const {
  return Tagged<HeapObject>(*this)
      ->IsLazilyInitializedExternalPointerFieldInitialized(offset);
}

template <ExternalPointerTag tag>
void HeapObjectLayout::WriteLazilyInitializedExternalPointerField(
    size_t offset, IsolateForSandbox isolate, Address value) {
  Tagged<HeapObject>(*this)->WriteLazilyInitializedExternalPointerField<tag>(
      offset, isolate, value);
}

MaybeObjectSlot HeapObject::RawMaybeWeakField(int byte_offset) const {
  return MaybeObjectSlot(field_address(byte_offset));
}

InstructionStreamSlot HeapObject::RawInstructionStreamField(
    int byte_offset) const {
  return InstructionStreamSlot(field_address(byte_offset));
}

ExternalPointerSlot HeapObject::RawExternalPointerField(
    int byte_offset, ExternalPointerTagRange tag_range) const {
  return ExternalPointerSlot(field_address(byte_offset), tag_range);
}

CppHeapPointerSlot HeapObject::RawCppHeapPointerField(int byte_offset) const {
  return CppHeapPointerSlot(field_address(byte_offset));
}

IndirectPointerSlot HeapObject::RawIndirectPointerField(
    int byte_offset, IndirectPointerTagRange tag_range) const {
  return IndirectPointerSlot(field_address(byte_offset), tag_range);
}



#ifdef VERIFY_HEAP
void HeapObject::VerifyObjectField(Isolate* isolate, int offset) {
  Object::VerifyPointer(isolate,
                        TaggedField<Object>::load(isolate, *this, offset));
  static_assert(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

void HeapObject::VerifyMaybeObjectField(Isolate* isolate, int offset) {
  Object::VerifyMaybeObjectPointer(
      isolate, TaggedField<MaybeObject>::load(isolate, *this, offset));
  static_assert(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

void HeapObject::VerifySmiField(int offset) {
  CHECK(IsSmi(TaggedField<Object>::load(*this, offset)));
  static_assert(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

void HeapObjectLayout::VerifyObjectField(Isolate* isolate, int offset) {
  Cast<HeapObject>(this)->VerifyObjectField(isolate, offset);
}

void HeapObjectLayout::VerifyMaybeObjectField(Isolate* isolate, int offset) {
  Cast<HeapObject>(this)->VerifyMaybeObjectField(isolate, offset);
}

void HeapObjectLayout::VerifySmiField(int offset) {
  Cast<HeapObject>(this)->VerifySmiField(offset);
}

#endif

bool HeapObjectLayout::NeedsRehashing(InstanceType instance_type) const {
  return Cast<HeapObject>(this)->NeedsRehashing(instance_type);
}

bool HeapObjectLayout::NeedsRehashing(PtrComprCageBase cage_base) const {
  return Cast<HeapObject>(this)->NeedsRehashing(cage_base);
}

bool HeapObjectLayout::CanBeRehashed(PtrComprCageBase cage_base) const {
  return Cast<HeapObject>(this)->CanBeRehashed(cage_base);
}

template <typename IsolateT>
void HeapObjectLayout::RehashBasedOnMap(IsolateT* isolate) {
  Cast<HeapObject>(this)->RehashBasedOnMap(isolate);
}

#ifdef VERIFY_HEAP
void HeapObjectLayout::HeapObjectVerify(Isolate* isolate) {
  Cast<HeapObject>(this)->HeapObjectVerify(isolate);
}
#endif

// static
bool JSArray::MayHaveReadOnlyLength(Tagged<Map> js_array_map) {
  DCHECK(IsJSArrayMap(js_array_map));
  if (V8_UNLIKELY(
          js_array_map->instance_descriptors()->number_of_descriptors() == 0)) {
    return true;
  }
  DCHECK(!js_array_map->is_dictionary_map());

  // Fast path: "length" is the first fast property of arrays with non
  // dictionary properties. Since it's not configurable, it's guaranteed to be
  // the first in the descriptor array.
  InternalIndex first(0);
  DCHECK(js_array_map->instance_descriptors()->GetKey(first) ==
         GetReadOnlyRoots().length_string());
  return V8_UNLIKELY(
      js_array_map->instance_descriptors()->GetDetails(first).IsReadOnly());
}

bool JSArray::HasReadOnlyLength(DirectHandle<JSArray> array) {
  Tagged<Map> map = array->map();

  // If map guarantees that there can't be a read-only length, we are done.
  if (!MayHaveReadOnlyLength(map)) return false;
  return V8_UNLIKELY(HasReadOnlyLengthSlowPath(array));
}

EarlyReadOnlyRoots HeapObject::EarlyGetReadOnlyRoots() const {
  return ReadOnlyHeap::EarlyGetReadOnlyRoots(*this);
}

EarlyReadOnlyRoots HeapObjectLayout::EarlyGetReadOnlyRoots() const {
  return ReadOnlyHeap::EarlyGetReadOnlyRoots(Tagged(this));
}

Tagged<Map> HeapObject::map() const {
  PtrComprCageBase cage_base = GetPtrComprCageBase();
  return HeapObject::map(cage_base);
}

Tagged<Map> HeapObject::map(PtrComprCageBase cage_base) const {
  return map_word(cage_base, kRelaxedLoad).ToMap();
}

Tagged<Map> HeapObjectLayout::map() const {
  // TODO(leszeks): Support MapWord members and access via that instead.
  return Tagged<HeapObject>(this)->map();
}

Tagged<Map> HeapObjectLayout::map(PtrComprCageBase cage_base) const {
  return Tagged<HeapObject>(this)->map(cage_base);
}

Tagged<Map> HeapObjectLayout::map(AcquireLoadTag) const {
  // TODO(leszeks): Support MapWord members and access via that instead.
  return Tagged<HeapObject>(this)->map(kAcquireLoad);
}

Tagged<Map> HeapObjectLayout::map(PtrComprCageBase cage_base,
                                  AcquireLoadTag tag) const {
  return Tagged<HeapObject>(this)->map(cage_base, tag);
}

MapWord HeapObjectLayout::map_word(RelaxedLoadTag) const {
  // TODO(leszeks): Support MapWord members and access via that instead.
  return Tagged<HeapObject>(this)->map_word(kRelaxedLoad);
}

void HeapObjectLayout::set_map(Isolate* isolate, Tagged<Map> value) {
  // TODO(leszeks): Support MapWord members and access via that instead.
  return Tagged<HeapObject>(this)->set_map(isolate, value);
}

template <typename IsolateT>
void HeapObjectLayout::set_map(IsolateT* isolate, Tagged<Map> value,
                               ReleaseStoreTag) {
  // TODO(leszeks): Support MapWord members and access via that instead.
  return Tagged<HeapObject>(this)->set_map(isolate, value, kReleaseStore);
}

template <typename IsolateT>
void HeapObjectLayout::set_map_safe_transition(IsolateT* isolate,
                                               Tagged<Map> value,
                                               ReleaseStoreTag) {
  // TODO(leszeks): Support MapWord members and access via that instead.
  return Tagged<HeapObject>(this)->set_map_safe_transition(isolate, value,
                                                           kReleaseStore);
}

void HeapObject::set_map(Isolate* isolate, Tagged<Map> value) {
  set_map<EmitWriteBarrier::kYes>(isolate, value, kRelaxedStore,
                                  VerificationMode::kPotentialLayoutChange);
}

template <typename IsolateT>
void HeapObject::set_map(IsolateT* isolate, Tagged<Map> value,
                         ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kYes>(isolate, value, kReleaseStore,
                                  VerificationMode::kPotentialLayoutChange);
}

template <typename IsolateT>
void HeapObject::set_map_safe_transition(IsolateT* isolate, Tagged<Map> value) {
  set_map<EmitWriteBarrier::kYes>(isolate, value, kRelaxedStore,
                                  VerificationMode::kSafeMapTransition);
}

template <typename IsolateT>
void HeapObject::set_map_safe_transition(IsolateT* isolate, Tagged<Map> value,
                                         ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kYes>(isolate, value, kReleaseStore,
                                  VerificationMode::kSafeMapTransition);
}

void HeapObjectLayout::set_map_safe_transition_no_write_barrier(
    Isolate* isolate, Tagged<Map> value, RelaxedStoreTag tag) {
  // TODO(leszeks): Support MapWord members and access via that instead.
  return Tagged<HeapObject>(this)->set_map_safe_transition_no_write_barrier(
      isolate, value, tag);
}

void HeapObject::set_map_safe_transition_no_write_barrier(Isolate* isolate,
                                                          Tagged<Map> value,
                                                          RelaxedStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(isolate, value, kRelaxedStore,
                                 VerificationMode::kSafeMapTransition);
}

void HeapObject::set_map_safe_transition_no_write_barrier(Isolate* isolate,
                                                          Tagged<Map> value,
                                                          ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(isolate, value, kReleaseStore,
                                 VerificationMode::kSafeMapTransition);
}

void HeapObjectLayout::set_map_no_write_barrier(Isolate* isolate,
                                                Tagged<Map> value,
                                                RelaxedStoreTag tag) {
  // TODO(leszeks): Support MapWord members and access via that instead.
  Tagged<HeapObject>(this)->set_map_no_write_barrier(isolate, value, tag);
}

// Unsafe accessor omitting write barrier.
void HeapObject::set_map_no_write_barrier(Isolate* isolate, Tagged<Map> value,
                                          RelaxedStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(isolate, value, kRelaxedStore,
                                 VerificationMode::kPotentialLayoutChange);
}

void HeapObject::set_map_no_write_barrier(Isolate* isolate, Tagged<Map> value,
                                          ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(isolate, value, kReleaseStore,
                                 VerificationMode::kPotentialLayoutChange);
}

template <HeapObject::EmitWriteBarrier emit_write_barrier, typename MemoryOrder,
          typename IsolateT>
void HeapObject::set_map(IsolateT* isolate, Tagged<Map> value,
                         MemoryOrder order, VerificationMode mode) {
#if V8_ENABLE_WEBASSEMBLY
  // In {WasmGraphBuilder::SetMap} and {WasmGraphBuilder::LoadMap}, we treat
  // maps as immutable. Therefore we are not allowed to mutate them here.
  DCHECK(!IsWasmStructMap(value) && !IsWasmArrayMap(value));
#endif
  if (v8_flags.verify_heap) {
    if (mode == VerificationMode::kSafeMapTransition) {
      HeapVerifier::VerifySafeMapTransition(isolate->heap()->AsHeap(), *this,
                                            value);
    } else {
      DCHECK_EQ(mode, VerificationMode::kPotentialLayoutChange);
      HeapVerifier::VerifyObjectLayoutChange(isolate->heap()->AsHeap(), *this,
                                             value);
    }
  }
  set_map_word(value, order);
  Heap::NotifyObjectLayoutChangeDone(*this);
#ifndef V8_DISABLE_WRITE_BARRIERS
  if (emit_write_barrier == EmitWriteBarrier::kYes) {
    WriteBarrier::ForValue(*this, MaybeObjectSlot(map_slot()), value,
                           UPDATE_WRITE_BARRIER);
  } else {
    DCHECK_EQ(emit_write_barrier, EmitWriteBarrier::kNo);
#if V8_VERIFY_WRITE_BARRIERS
    DCHECK(!WriteBarrier::IsRequired(*this, value));
#endif
  }
#endif
}

template <typename IsolateT>
void HeapObjectLayout::set_map_after_allocation(IsolateT* isolate,
                                                Tagged<Map> value,
                                                WriteBarrierMode mode) {
  // TODO(leszeks): Support MapWord members and access via that instead.
  Tagged<HeapObject>(this)->set_map_after_allocation(isolate, value, mode);
}

template <typename IsolateT>
void HeapObject::set_map_after_allocation(IsolateT* isolate, Tagged<Map> value,
                                          WriteBarrierMode mode) {
  set_map_word(value, kRelaxedStore);
#ifndef V8_DISABLE_WRITE_BARRIERS
  if (mode != SKIP_WRITE_BARRIER) {
    DCHECK(!value.is_null());
    WriteBarrier::ForValue(*this, MaybeObjectSlot(map_slot()), value, mode);
  } else {
    SLOW_DCHECK(
        // We allow writes of a null map before root initialisation.
        value.is_null() ? !isolate->read_only_heap()->roots_init_complete()
                        : !WriteBarrier::IsRequired(*this, value));
  }
#endif
}

// static
void HeapObject::SetFillerMap(const WritableFreeSpace& writable_space,
                              Tagged<Map> value) {
  writable_space.WriteHeaderSlot<Map, kMapOffset>(value, kRelaxedStore);
}

DEF_ACQUIRE_GETTER(HeapObject, map, Tagged<Map>) {
  return map_word(cage_base, kAcquireLoad).ToMap();
}

ObjectSlot HeapObjectLayout::map_slot() const {
  return Tagged<HeapObject>(this)->map_slot();
}

ObjectSlot HeapObject::map_slot() const {
  return ObjectSlot(MapField::address(*this));
}

MapWord HeapObject::map_word(RelaxedLoadTag tag) const {
  // This method is never used for objects located in code space
  // (InstructionStream and free space fillers) and thus it is fine to use
  // auto-computed cage base value.
  DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL,
                 !TrustedHeapLayout::InCodeSpace(*this));
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return HeapObject::map_word(cage_base, tag);
}
MapWord HeapObject::map_word(PtrComprCageBase cage_base,
                             RelaxedLoadTag tag) const {
  return MapField::Relaxed_Load_Map_Word(cage_base, *this);
}

void HeapObject::set_map_word(Tagged<Map> map, RelaxedStoreTag) {
  MapField::Relaxed_Store_Map_Word(*this, MapWord::FromMap(map));
}

void HeapObject::set_map_word_forwarded(Tagged<HeapObject> target_object,
                                        RelaxedStoreTag) {
  MapField::Relaxed_Store_Map_Word(
      *this, MapWord::FromForwardingAddress(*this, target_object));
}

MapWord HeapObject::map_word(AcquireLoadTag tag) const {
  // This method is never used for objects located in code space
  // (InstructionStream and free space fillers) and thus it is fine to use
  // auto-computed cage base value.
  DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL,
                 !TrustedHeapLayout::InCodeSpace(*this));
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return HeapObject::map_word(cage_base, tag);
}
MapWord HeapObject::map_word(PtrComprCageBase cage_base,
                             AcquireLoadTag tag) const {
  return MapField::Acquire_Load_No_Unpack(cage_base, *this);
}

void HeapObject::set_map_word(Tagged<Map> map, ReleaseStoreTag) {
  MapField::Release_Store_Map_Word(*this, MapWord::FromMap(map));
}

void HeapObjectLayout::set_map_word_forwarded(Tagged<HeapObject> target_object,
                                              ReleaseStoreTag tag) {
  // TODO(leszeks): Support MapWord members and access via that instead.
  Tagged<HeapObject>(this)->set_map_word_forwarded(target_object, tag);
}

void HeapObjectLayout::set_map_word_forwarded(Tagged<HeapObject> target_object,
                                              RelaxedStoreTag tag) {
  // TODO(leszeks): Support MapWord members and access via that instead.
  Tagged<HeapObject>(this)->set_map_word_forwarded(target_object, tag);
}

void HeapObject::set_map_word_forwarded(Tagged<HeapObject> target_object,
                                        ReleaseStoreTag) {
  MapField::Release_Store_Map_Word(
      *this, MapWord::FromForwardingAddress(*this, target_object));
}

bool HeapObject::relaxed_compare_and_swap_map_word_forwarded(
    MapWord old_map_word, Tagged<HeapObject> new_target_object) {
  Tagged_t result = MapField::Relaxed_CompareAndSwap(
      *this, old_map_word,
      MapWord::FromForwardingAddress(*this, new_target_object));
  return result == static_cast<Tagged_t>(old_map_word.ptr());
}

int HeapObjectLayout::Size() const { return Tagged<HeapObject>(this)->Size(); }

SafeHeapObjectSize HeapObjectLayout::SafeSize() const {
  return Tagged<HeapObject>(this)->SafeSize();
}

// TODO(v8:11880): consider dropping parameterless version.
int HeapObject::Size() const {
  DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL,
                 !TrustedHeapLayout::InCodeSpace(*this));
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return HeapObject::Size(cage_base);
}
int HeapObject::Size(PtrComprCageBase cage_base) const {
  return SizeFromMap(map(cage_base));
}

SafeHeapObjectSize HeapObject::SafeSize() const {
  DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL,
                 !TrustedHeapLayout::InCodeSpace(*this));
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return HeapObject::SafeSize(cage_base);
}
SafeHeapObjectSize HeapObject::SafeSize(PtrComprCageBase cage_base) const {
  return SafeSizeFromMap(map(cage_base));
}

inline bool IsSpecialReceiverInstanceType(InstanceType instance_type) {
  return instance_type <= LAST_SPECIAL_RECEIVER_TYPE;
}

// This should be in objects/map-inl.h, but can't, because of a cyclic
// dependency.
bool IsSpecialReceiverMap(Tagged<Map> map) {
  bool result = IsSpecialReceiverInstanceType(map->instance_type());
  DCHECK_IMPLIES(
      !result, !map->has_named_interceptor() && !map->is_access_check_needed());
  return result;
}

inline bool IsCustomElementsReceiverInstanceType(InstanceType instance_type) {
  return instance_type <= LAST_CUSTOM_ELEMENTS_RECEIVER;
}

// This should be in objects/map-inl.h, but can't, because of a cyclic
// dependency.
bool IsCustomElementsReceiverMap(Tagged<Map> map) {
  return IsCustomElementsReceiverInstanceType(map->instance_type());
}

// static
bool Object::ToArrayLength(Tagged<Object> obj, uint32_t* index) {
  return Object::ToUint32(obj, index);
}

// static
bool Object::ToArrayIndex(Tagged<Object> obj, uint32_t* index) {
  return Object::ToUint32(obj, index) && *index != kMaxUInt32;
}

// static
bool Object::ToIntegerIndex(Tagged<Object> obj, size_t* index) {
  if (IsSmi(obj)) {
    int num = Smi::ToInt(obj);
    if (num < 0) return false;
    *index = static_cast<size_t>(num);
    return true;
  }
  if (IsHeapNumber(obj)) {
    double num = Cast<HeapNumber>(obj)->value();
    if (!(num >= 0)) return false;  // Negation to catch NaNs.
    constexpr double max =
        std::min(kMaxSafeInteger,
                 // The maximum size_t is reserved as "invalid" sentinel.
                 static_cast<double>(std::numeric_limits<size_t>::max() - 1));
    if (num > max) return false;
    size_t result = static_cast<size_t>(num);
    if (num != result) return false;  // Conversion lost fractional precision.
    *index = result;
    return true;
  }
  return false;
}

WriteBarrierModeScope HeapObjectLayout::GetWriteBarrierMode(
    const DisallowGarbageCollection& promise) {
  return WriteBarrier::GetWriteBarrierModeForObject(Tagged(this), promise);
}

WriteBarrierModeScope HeapObject::GetWriteBarrierMode(
    const DisallowGarbageCollection& promise) {
  return WriteBarrier::GetWriteBarrierModeForObject(*this, promise);
}

// static
AllocationAlignment HeapObject::RequiredAlignment(AllocationSpace space,
                                                  Tagged<Map> map) {
  return RequiredAlignment(
      IsAnyWritableSharedSpace(space) ? kInSharedSpace : kNotInSharedSpace,
      map);
}

// static
AllocationAlignment HeapObject::RequiredAlignment(InSharedSpace in_shared_space,
                                                  Tagged<Map> map) {
  // TODO(v8:4153): We should think about requiring double alignment
  // in general for ByteArray, since they are used as backing store for typed
  // arrays now.
  // TODO(ishell, v8:8875): Consider using aligned allocations for BigInt.
  if (USE_ALLOCATION_ALIGNMENT_HEAP_NUMBER_BOOL) {
    int instance_type = map->instance_type();

    static_assert(!USE_ALLOCATION_ALIGNMENT_HEAP_NUMBER_BOOL ||
                  (sizeof(FixedDoubleArray::Header) & kDoubleAlignmentMask) ==
                      kTaggedSize);
    if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) return kDoubleAligned;

    static_assert(!USE_ALLOCATION_ALIGNMENT_HEAP_NUMBER_BOOL ||
                  (offsetof(HeapNumber, value_) & kDoubleAlignmentMask) ==
                      kTaggedSize);
    if (instance_type == HEAP_NUMBER_TYPE) return kDoubleUnaligned;
  }
#if V8_ENABLE_WEBASSEMBLY
  if (in_shared_space && v8_flags.experimental_wasm_shared) [[unlikely]] {
    int instance_type = map->instance_type();
    if (instance_type == WASM_STRUCT_TYPE) {
      // The map of a shared wasm struct needs to be in the shared space.
      DCHECK(HeapLayout::InWritableSharedSpace(map));
      return kDoubleAligned;
    } else if (instance_type == WASM_ARRAY_TYPE) {
      // The map of a shared wasm array needs to be in the shared space.
      DCHECK(HeapLayout::InWritableSharedSpace(map));
      return kDoubleUnaligned;
    }
  }
#endif
  return kTaggedAligned;
}

bool HeapObject::CheckRequiredAlignment(PtrComprCageBase cage_base) const {
  const InSharedSpace in_shared_space = HeapLayout::InWritableSharedSpace(*this)
                                            ? kInSharedSpace
                                            : kNotInSharedSpace;
  AllocationAlignment alignment =
      HeapObject::RequiredAlignment(in_shared_space, map(cage_base));
  CHECK_EQ(0, Heap::GetFillToAlign(address(), alignment));
  return true;
}

Address HeapObject::GetFieldAddress(int field_offset) const {
  return field_address(field_offset);
}

// static
Maybe<bool> Object::GreaterThan(Isolate* isolate, DirectHandle<Object> x,
                                DirectHandle<Object> y) {
  Maybe<ComparisonResult> result = Compare(isolate, x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kGreaterThan:
        return Just(true);
      case ComparisonResult::kLessThan:
      case ComparisonResult::kEqual:
      case ComparisonResult::kUndefined:
        return Just(false);
    }
  }
  return Nothing<bool>();
}

// static
Maybe<bool> Object::GreaterThanOrEqual(Isolate* isolate, DirectHandle<Object> x,
                                       DirectHandle<Object> y) {
  Maybe<ComparisonResult> result = Compare(isolate, x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kEqual:
      case ComparisonResult::kGreaterThan:
        return Just(true);
      case ComparisonResult::kLessThan:
      case ComparisonResult::kUndefined:
        return Just(false);
    }
  }
  return Nothing<bool>();
}

// static
Maybe<bool> Object::LessThan(Isolate* isolate, DirectHandle<Object> x,
                             DirectHandle<Object> y) {
  Maybe<ComparisonResult> result = Compare(isolate, x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kLessThan:
        return Just(true);
      case ComparisonResult::kEqual:
      case ComparisonResult::kGreaterThan:
      case ComparisonResult::kUndefined:
        return Just(false);
    }
  }
  return Nothing<bool>();
}

// static
Maybe<bool> Object::LessThanOrEqual(Isolate* isolate, DirectHandle<Object> x,
                                    DirectHandle<Object> y) {
  Maybe<ComparisonResult> result = Compare(isolate, x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kEqual:
      case ComparisonResult::kLessThan:
        return Just(true);
      case ComparisonResult::kGreaterThan:
      case ComparisonResult::kUndefined:
        return Just(false);
    }
  }
  return Nothing<bool>();
}

MaybeHandle<Object> Object::GetPropertyOrElement(Isolate* isolate,
                                                 DirectHandle<JSAny> object,
                                                 DirectHandle<Name> name) {
  return GetPropertyOrElement(isolate, object, PropertyKey(isolate, name));
}

MaybeHandle<Object> Object::GetPropertyOrElement(Isolate* isolate,
                                                 DirectHandle<JSAny> object,
                                                 PropertyKey key) {
  LookupIterator it(isolate, object, key);
  return GetProperty(&it);
}

MaybeDirectHandle<Object> Object::SetPropertyOrElement(
    Isolate* isolate, DirectHandle<JSAny> object, DirectHandle<Name> name,
    DirectHandle<Object> value, Maybe<ShouldThrow> should_throw,
    StoreOrigin store_origin) {
  return SetPropertyOrElement(isolate, object, PropertyKey(isolate, name),
                              value, should_throw, store_origin);
}

MaybeDirectHandle<Object> Object::SetPropertyOrElement(
    Isolate* isolate, DirectHandle<JSAny> object, PropertyKey key,
    DirectHandle<Object> value, Maybe<ShouldThrow> should_throw,
    StoreOrigin store_origin) {
  LookupIterator it(isolate, object, key);
  MAYBE_RETURN_NULL(SetProperty(&it, value, store_origin, should_throw));
  return value;
}

// static
Tagged<Object> Object::GetSimpleHash(Tagged<Object> object) {
  DisallowGarbageCollection no_gc;
  if (IsSmi(object)) {
    uint32_t hash = ComputeUnseededHash(Smi::ToInt(object));
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  auto instance_type = Cast<HeapObject>(object)->map()->instance_type();
  if (InstanceTypeChecker::IsHeapNumber(instance_type)) {
    double num = Cast<HeapNumber>(object)->value();
    if (std::isnan(num)) return Smi::FromInt(Smi::kMaxValue);
    // Use ComputeUnseededHash for all values in Signed32 range, including -0,
    // which is considered equal to 0 because collections use SameValueZero.
    uint32_t hash;
    // Check range before conversion to avoid undefined behavior.
    if (num >= kMinInt && num <= kMaxInt && FastI2D(FastD2I(num)) == num) {
      hash = ComputeUnseededHash(FastD2I(num));
    } else {
      hash = ComputeLongHash(base::double_to_uint64(num));
    }
    return Smi::FromInt(hash & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsName(instance_type)) {
    uint32_t hash = Cast<Name>(object)->EnsureHash();
    return Smi::FromInt(hash);
  } else if (InstanceTypeChecker::IsOddball(instance_type)) {
    uint32_t hash = Cast<Oddball>(object)->to_string()->EnsureHash();
    return Smi::FromInt(hash);
  } else if (InstanceTypeChecker::IsBigInt(instance_type)) {
    uint32_t hash = Cast<BigInt>(object)->Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsSharedFunctionInfo(instance_type)) {
    uint32_t hash = Cast<SharedFunctionInfo>(object)->Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsScopeInfo(instance_type)) {
    uint32_t hash = Cast<ScopeInfo>(object)->Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsScript(instance_type)) {
    int id = Cast<Script>(object)->id();
    return Smi::FromInt(ComputeUnseededHash(id) & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsTemplateInfo(instance_type)) {
    uint32_t hash = Cast<TemplateInfo>(object)->GetHash();
    DCHECK_EQ(hash, hash & Smi::kMaxValue);
    return Smi::FromInt(hash);
  }

  DCHECK_NE(instance_type, HOLE_TYPE);
  DCHECK(IsJSReceiver(object));
  return object;
}

// static
Tagged<Object> Object::GetHash(Tagged<Object> obj) {
  DisallowGarbageCollection no_gc;
  Tagged<Object> hash = GetSimpleHash(obj);
  if (IsSmi(hash)) return hash;

  // Make sure that we never cast internal objects to JSReceivers.
  CHECK(IsJSReceiver(obj));
  Tagged<JSReceiver> receiver = Cast<JSReceiver>(obj);
  return receiver->GetIdentityHash();
}

bool IsShared(Tagged<Object> obj) {
  // This logic should be kept in sync with fast paths in
  // CodeStubAssembler::SharedValueBarrier.

  // Smis are trivially shared.
  if (IsSmi(obj)) return true;

  Tagged<HeapObject> object = Cast<HeapObject>(obj);

  // RO objects are shared when the RO space is shared.
  if (HeapLayout::InReadOnlySpace(object)) {
    return true;
  }

  // Check if this object is already shared.
  InstanceType instance_type = object->map()->instance_type();
  if (InstanceTypeChecker::IsAlwaysSharedSpaceJSObject(instance_type)) {
    DCHECK(HeapLayout::InAnySharedSpace(object));
    return true;
  }
  switch (instance_type) {
    case SHARED_SEQ_TWO_BYTE_STRING_TYPE:
    case SHARED_SEQ_ONE_BYTE_STRING_TYPE:
    case SHARED_EXTERNAL_TWO_BYTE_STRING_TYPE:
    case SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE:
    case SHARED_UNCACHED_EXTERNAL_TWO_BYTE_STRING_TYPE:
    case SHARED_UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE:
      DCHECK(HeapLayout::InAnySharedSpace(object));
      return true;
#if V8_ENABLE_WEBASSEMBLY
    case WASM_STRUCT_TYPE:
    case WASM_ARRAY_TYPE:
      return HeapLayout::InAnySharedSpace(object);
#endif
    case INTERNALIZED_TWO_BYTE_STRING_TYPE:
    case INTERNALIZED_ONE_BYTE_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_TWO_BYTE_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_ONE_BYTE_STRING_TYPE:
    case UNCACHED_EXTERNAL_INTERNALIZED_TWO_BYTE_STRING_TYPE:
    case UNCACHED_EXTERNAL_INTERNALIZED_ONE_BYTE_STRING_TYPE:
      if (v8_flags.shared_string_table) {
        DCHECK(HeapLayout::InAnySharedSpace(object));
        return true;
      }
      return false;
    case HEAP_NUMBER_TYPE:
      return HeapLayout::InWritableSharedSpace(object);
    default:
      return false;
  }
}

// static
template <typename T, template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<T>, DirectHandle<T>>)
typename HandleType<Object>::MaybeType Object::Share(
    Isolate* isolate, HandleType<T> value,
    ShouldThrow throw_if_cannot_be_shared) {
  // Sharing values requires the RO space be shared.
  if (IsShared(*value)) return value;
  return ShareSlow(isolate, Cast<HeapObject>(value), throw_if_cannot_be_shared);
}

// https://tc39.es/ecma262/#sec-canbeheldweakly
// static
bool Object::CanBeHeldWeakly(Tagged<Object> obj) {
  if (IsJSReceiver(obj)) {
    // TODO(v8:12547) Shared structs and arrays should only be able to point
    // to shared values in weak collections. For now, disallow them as weak
    // collection keys.
#if V8_ENABLE_WEBASSEMBLY
    if (v8_flags.experimental_wasm_shared &&
        (IsWasmStruct(obj) || IsWasmArray(obj)) &&
        HeapLayout::InAnySharedSpace(Cast<HeapObject>(obj))) {
      return false;
    }
#endif
    return (!v8_flags.harmony_struct ||
            (!IsJSSharedStruct(obj) && !IsJSSharedArray(obj)));
  }
  return IsSymbol(obj) && !Cast<Symbol>(obj)->is_in_public_symbol_table();
}

DirectHandle<Object> ObjectHashTableShapeBase::AsHandle(
    DirectHandle<Object> key) {
  return key;
}

Relocatable::Relocatable(Isolate* isolate) {
  isolate_ = isolate;
  prev_ = isolate->relocatable_top();
  isolate->set_relocatable_top(this);
}

Relocatable::~Relocatable() {
  DCHECK_EQ(isolate_->relocatable_top(), this);
  isolate_->set_relocatable_top(prev_);
}

// Predictably converts HeapObject or Address to uint32 by calculating
// offset of the address in respective MemoryChunk.
inline uint32_t ObjectAddressForHashing(Address object) {
  return MemoryChunk::AddressToOffset(object);
}

inline DirectHandle<Object> MakeEntryPair(Isolate* isolate, size_t index,
                                          DirectHandle<Object> value) {
  DirectHandle<Object> key = isolate->factory()->SizeToString(index);
  DirectHandle<FixedArray> entry_storage = isolate->factory()->NewFixedArray(2);
  {
    entry_storage->set(0, *key, SKIP_WRITE_BARRIER);
    entry_storage->set(1, *value, SKIP_WRITE_BARRIER);
  }
  return isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                    PACKED_ELEMENTS, 2);
}

inline DirectHandle<Object> MakeEntryPair(Isolate* isolate,
                                          DirectHandle<Object> key,
                                          DirectHandle<Object> value) {
  DirectHandle<FixedArray> entry_storage = isolate->factory()->NewFixedArray(2);
  {
    entry_storage->set(0, *key, SKIP_WRITE_BARRIER);
    entry_storage->set(1, *value, SKIP_WRITE_BARRIER);
  }
  return isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                    PACKED_ELEMENTS, 2);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_OBJECTS_INL_H_
