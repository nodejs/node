// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_VALUE_TYPE_H_
#define V8_WASM_VALUE_TYPE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <optional>

#include "src/base/bit-field.h"
#include "src/base/template-utils.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/signature.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-limits.h"

namespace v8 {
namespace internal {

// Type for holding simd values, defined in simd128.h.
class Simd128;
class Zone;

namespace wasm {

// Format: kind, log2Size, code, machineType, shortName, typeName
#define FOREACH_NUMERIC_VALUE_TYPE(V)    \
  V(I32, 2, I32, Int32, 'i', "i32")      \
  V(I64, 3, I64, Int64, 'l', "i64")      \
  V(F32, 2, F32, Float32, 'f', "f32")    \
  V(F64, 3, F64, Float64, 'd', "f64")    \
  V(S128, 4, S128, Simd128, 's', "v128") \
  V(I8, 0, I8, Int8, 'b', "i8")          \
  V(I16, 1, I16, Int16, 'h', "i16")      \
  V(F16, 1, F16, Float16, 'p', "f16")

#define FOREACH_VALUE_TYPE(V)                                      \
  V(Void, -1, Void, None, 'v', "<void>")                           \
  FOREACH_NUMERIC_VALUE_TYPE(V)                                    \
  V(Ref, kTaggedSizeLog2, Ref, AnyTagged, 'r', "ref")              \
  V(RefNull, kTaggedSizeLog2, RefNull, AnyTagged, 'n', "ref null") \
  V(Top, -1, Void, None, '\\', "<top>")                            \
  V(Bottom, -1, Void, None, '*', "<bot>")

constexpr int kMaxValueTypeSize = 16;  // bytes

struct TypeIndex {
  uint32_t index;

  // We intentionally don't define comparison operators here, because
  // different subclasses must not be compared to each other.

  static constexpr uint32_t kInvalid = ~0u;
  constexpr bool valid() const { return index != kInvalid; }

  size_t hash_value() const { return index; }
};

struct ModuleTypeIndex : public TypeIndex {
  inline static constexpr ModuleTypeIndex Invalid();
  // Can't use "=default" because the base class doesn't have operator<=>.
  bool operator==(ModuleTypeIndex other) const { return index == other.index; }
  auto operator<=>(ModuleTypeIndex other) const {
    return index <=> other.index;
  }
};
ASSERT_TRIVIALLY_COPYABLE(ModuleTypeIndex);

constexpr ModuleTypeIndex ModuleTypeIndex::Invalid() {
  return ModuleTypeIndex{ModuleTypeIndex::kInvalid};
}

struct CanonicalTypeIndex : public TypeIndex {
  inline static constexpr CanonicalTypeIndex Invalid();

  bool operator==(CanonicalTypeIndex other) const {
    return index == other.index;
  }
  auto operator<=>(CanonicalTypeIndex other) const {
    return index <=> other.index;
  }
};
ASSERT_TRIVIALLY_COPYABLE(CanonicalTypeIndex);

constexpr CanonicalTypeIndex CanonicalTypeIndex::Invalid() {
  return CanonicalTypeIndex{CanonicalTypeIndex::kInvalid};
}

inline std::ostream& operator<<(std::ostream& oss, TypeIndex index) {
  return oss << index.index;
}

enum Nullability : bool { kNonNullable = false, kNullable = true };

enum Exactness : bool { kAnySubtype, kExact };

static constexpr bool kNotShared = false;

enum ValueKind : uint8_t {
#define DEF_ENUM(kind, ...) k##kind,
  FOREACH_VALUE_TYPE(DEF_ENUM)
#undef DEF_ENUM
};

////////////////////////////////////////////////////////////////////////////////
// ValueType implementation.
//
// We represent ValueTypes as bit fields. We store a mix of spec-level
// information and additional caches that are useful for efficient algorithms
// (mostly of subtyping). The key design consideration is that we need to
// express many different variations of attributes, checks, switches; so we
// want a ValueType representation that efficiently supports many different
// kinds of queries and classifications.
// The usage of the bit field is as follows:
// bits 0-1: TypeKind: numeric, indexed ref, generic ref, or sentinel. The bits
//           are chosen such that they can also be interpreted as:
// bit 0: is it a reference of some sort?
// bit 1: if it's a reference: does it have an index?
//        if it isn't a reference: is it an internal sentinel?
//
// bit 2: if it's a reference, is it nullable? Otherwise: unused.
// bit 3: if it's a reference, is it an exact reference? Otherwise: unused.
//
// For indexed (aka user defined) types, the next two fields cache information
// that could also be looked up in the module's definition of the type:
// bit 4: is it a shared type? Note that we consider numeric types as shared,
//        i.e. "shared" means "safe to use in a shared function".
// bits 5-7: RefTypeKind: if it's a reference (indexed or generic), what
//           category of type (struct/array/function/continuation) is it?
//           Non-reference types store the value "kOther".
//
// bits 8-27: For indexed types, the index. For other types, the StandardType.
//            The "ValueType" subclass stores module-specific indices, the
//            "CanonicalValueType" stores canonicalized indices.
//            For non-indexed types, we also define their respective
//            "NumericKind" or "GenericKind", which in addition to the right
//            StandardType include the other relevant lower bits.
//
// With these assignments, it turns out that "HeapType" is effectively a
// subset of "ValueType", where bits 0-1 are known to *not* be "numeric",
// and bit 2 (which is ValueType-specific) is ignored.

namespace value_type_impl {

using IsRefField = base::BitField<bool, 0, 1>;
using HasIndexOrSentinelField = IsRefField::Next<bool, 1>;

}  // namespace value_type_impl

// Shorthands for the "extra_bits" definitions below. All of these must be
// subsets of kGenericKindMask below!
#define FUNC value_type_impl::RefTypeKindField::encode(RefTypeKind::kFunction)
#define STRUCT value_type_impl::RefTypeKindField::encode(RefTypeKind::kStruct)
#define ARRAY value_type_impl::RefTypeKindField::encode(RefTypeKind::kArray)
#define CONT value_type_impl::RefTypeKindField::encode(RefTypeKind::kCont)
#define REF value_type_impl::IsRefField::encode(true)
#define SENTINEL value_type_impl::HasIndexOrSentinelField::encode(true)
#define EXACT value_type_impl::IsExactField::encode(Exactness::kExact)

// Abstract ref types that can occur in wire bytes.
// We mark them as "exact" to require less special-casing in the subtyping
// implementation.
// Format: kName, ValueTypeCode, extra_bits, printable_name
#define FOREACH_NONE_TYPE(V) /*                               force 80 cols */ \
  V(NoCont, NoCont, REF | CONT | EXACT, "nocont")                              \
  V(NoExn, NoExn, REF | EXACT, "noexn")                                        \
  V(NoExtern, NoExtern, REF | EXACT, "noextern")                               \
  V(NoFunc, NoFunc, REF | FUNC | EXACT, "nofunc")                              \
  V(None, None, REF | EXACT, "none")

#define FOREACH_ABSTRACT_TYPE(V) /*                           force 80 cols */ \
  FOREACH_NONE_TYPE(V)                                                         \
  /* Established (non-proposal) types. */                                      \
  V(Func, FuncRef, REF | FUNC, "func")                                         \
  V(Any, AnyRef, REF, "any")                                                   \
  V(Eq, EqRef, REF, "eq")                                                      \
  V(I31, I31Ref, REF, "i31")                                                   \
  V(Struct, StructRef, REF | STRUCT, "struct")                                 \
  V(Array, ArrayRef, REF | ARRAY, "array")                                     \
  V(Extern, ExternRef, REF, "extern")                                          \
  V(Exn, ExnRef, REF, "exn")                                                   \
  /* WasmFX aka Core Stack Switching. */                                       \
  V(Cont, ContRef, REF | CONT, "cont")                                         \
  /* Stringref proposal. */                                                    \
  V(String, StringRef, REF, "string")                                          \
  V(StringViewWtf8, StringViewWtf8, REF, "stringview_wtf8")                    \
  V(StringViewWtf16, StringViewWtf16, REF, "stringview_wtf16")                 \
  V(StringViewIter, StringViewIter, REF, "stringview_iter")

// Types that don't fit into any of the other groups.
#define FOREACH_INTERNAL_TYPE(V) /*                           force 80 cols */ \
  V(Void, Void, SENTINEL, "<void>")                                            \
  V(Top, Void, SENTINEL, "<top>")                                              \
  V(Bottom, Void, SENTINEL | EXACT, "<bot>")                                   \
  V(ExternString, Void, REF, "<extern_string>")

#define FOREACH_GENERIC_TYPE(V) /*                            force 80 cols */ \
  FOREACH_INTERNAL_TYPE(V)                                                     \
  FOREACH_ABSTRACT_TYPE(V)

// 2 bits to coarsely classify ValueTypes.
enum class TypeKind : uint8_t {
  kNumeric = 0b00,       // i32, etc. FOREACH_NUMERIC_VALUE_TYPE above.
  kSentinel = SENTINEL,  // top, bottom, void. Have "SENTINEL" above.
  kAbstractRef = REF,    // anyref, eqref, funcref, etc. Have "REF" above.
  kIndexedRef = REF | SENTINEL,
};
enum class RefTypeKind : uint8_t {
  kOther = 0,
  kFunction = 1,
  kStruct = 2,
  kArray = 3,
  kCont = 4,

  kLastValue = kCont,
};

namespace value_type_impl {

using TypeKindField = base::BitField<TypeKind, 0, 2>;
static_assert(TypeKindField::kMask ==
              (IsRefField::kMask | HasIndexOrSentinelField::kMask));
// For reference types, we classify the type of reference.
// Non-reference types don't use these bits.
using IsNullableField = TypeKindField::Next<Nullability, 1>;
using IsExactField = IsNullableField::Next<Exactness, 1>;
// For reference types, we cache some information about the referenced type.
// Non-reference types don't use these bits.
using IsSharedField = IsExactField::Next<bool, 1>;
using RefTypeKindField = IsSharedField::Next<RefTypeKind, 3>;
static_assert(RefTypeKindField::is_valid(RefTypeKind::kLastValue));

// Stores the index if {has_index()}, or the {StandardType} otherwise.
using PayloadField = RefTypeKindField::Next<uint32_t, 20>;

// Reserved for future use.
using ReservedField = PayloadField::Next<uint32_t, 4>;
static_assert(ReservedField::kShift + ReservedField::kSize == 32);

// Useful for HeapTypes, whose "shared" bit is orthogonal to their kind.
static constexpr uint32_t kGenericKindMask =
    PayloadField::kMask | RefTypeKindField::kMask | TypeKindField::kMask |
    IsExactField::kMask;
// Useful for numeric types which are always considered "shared".
static constexpr uint32_t kNumericKindMask =
    kGenericKindMask | IsSharedField::kMask;
static constexpr uint32_t kNumericExtraBits = IsSharedField::encode(true);

#define COUNT(...) +1
static constexpr uint32_t kNumberOfGenericKinds = 0 FOREACH_GENERIC_TYPE(COUNT);
static constexpr uint32_t kNumberOfNumericKinds =
    0 FOREACH_NUMERIC_VALUE_TYPE(COUNT);
#undef COUNT
static constexpr uint32_t kNumberOfStandardTypes =
    kNumberOfGenericKinds + kNumberOfNumericKinds;

}  // namespace value_type_impl

// We define one enum with all non-indexed types, in a compact encoding
// space. These enum values will equal the contents of the {PayloadField} for
// the respective ValueTypes.
enum class StandardType : uint8_t {
#define DEF_ENUM(kind, ...) k##kind,
  FOREACH_GENERIC_TYPE(DEF_ENUM) FOREACH_NUMERIC_VALUE_TYPE(DEF_ENUM)
#undef DEF_ENUM
};

// We additionally define separate enums for generic and numeric types, so that
// we can write exhaustive switch statements. These enums' values include
// additional relevant bits in the {bit_field_}s of the respective HeapTypes/
// ValueTypes, so that certain checks can be expressed as bit field comparisons.
enum class GenericKind : uint32_t {
#define DEF_ENUM(kind, code, extra_bits, ...)                  \
  k##kind = value_type_impl::PayloadField::encode(             \
                static_cast<uint8_t>(StandardType::k##kind)) + \
            (extra_bits),
  FOREACH_GENERIC_TYPE(DEF_ENUM)
#undef DEF_ENUM
};

enum class NumericKind : uint32_t {
#define DEF_ENUM(kind, ...)                                    \
  k##kind = value_type_impl::PayloadField::encode(             \
                static_cast<uint8_t>(StandardType::k##kind)) + \
            value_type_impl::kNumericExtraBits,
  FOREACH_NUMERIC_VALUE_TYPE(DEF_ENUM)
#undef DEF_ENUM
};

#undef FUNC
#undef STRUCT
#undef ARRAY
#undef CONT
#undef REF
#undef SENTINEL
#undef EXACT

namespace value_type_impl {

// Useful for dense lookup tables.
constexpr uint32_t ToZeroBasedIndex(NumericKind kind) {
  // Inside {StandardType}, generic types come first.
  static_assert(static_cast<uint32_t>(StandardType::kI32) >=
                kNumberOfGenericKinds);
  uint32_t raw = PayloadField::decode(static_cast<uint32_t>(kind));
  DCHECK_GE(raw, kNumberOfGenericKinds);
  // As an additional safety net, as long as we happen to have exactly 8
  // numeric types, we can conveniently apply a mask here. If we need to
  // accommodate a different number of numeric kinds in the future, we should
  // consider adding bounds checks at use sites.
  static_assert(kNumberOfNumericKinds == 8);
  return (raw - kNumberOfGenericKinds) & 0x7;
}

}  // namespace value_type_impl

constexpr bool IsNullKind(GenericKind kind) {
  switch (kind) {
#define NULLTYPE(name, ...)  \
  case GenericKind::k##name: \
    return true;
    FOREACH_NONE_TYPE(NULLTYPE)
#undef NULLTYPE
    default:
      break;
  }
  return false;
}

// {ValueTypeBase} shouldn't be used directly; code should be using one of
// the subclasses. To enforce this, the public interface is limited to
// type index agnostic getters.
class ValueTypeBase {
 public:
  static const int kNumIndexBits = value_type_impl::PayloadField::kSize;
  static const int kLastUsedBit = value_type_impl::PayloadField::kLastUsedBit;
  static const uint32_t kIsRefBit = value_type_impl::IsRefField::kMask;
  static const uint32_t kIsNullableBit =
      value_type_impl::IsNullableField::kMask;
  static const uint32_t kIsSharedBit = value_type_impl::IsSharedField::kMask;
  static const uint32_t kHasIndexBit =
      value_type_impl::HasIndexOrSentinelField::kMask;
  static const uint32_t kRefKindBits = value_type_impl::RefTypeKindField::kMask;
  static const uint32_t kRefKindShift =
      value_type_impl::RefTypeKindField::kShift;
  static const uint32_t kIndexBits = value_type_impl::PayloadField::kMask;
  static const uint32_t kIndexShift = value_type_impl::PayloadField::kShift;

  constexpr ValueTypeBase()
      : ValueTypeBase(GenericKind::kVoid, kNonNullable, false) {}

  // This is specifically for the needs of the decoder: sometimes we need to
  // create the ValueType instance when we still only know the type index.
  // Once we know more about the referenced type, this function updates those
  // bits to their correct values.
  void Populate(bool shared, RefTypeKind kind) {
    uint32_t bits = value_type_impl::IsSharedField::update(bit_field_, shared);
    bit_field_ = value_type_impl::RefTypeKindField::update(bits, kind);
  }

  constexpr TypeKind type_kind() const {
    return value_type_impl::TypeKindField::decode(bit_field_);
  }
  constexpr bool has_index() const {
    return type_kind() == TypeKind::kIndexedRef;
  }
  constexpr bool is_sentinel() const {
    return type_kind() == TypeKind::kSentinel;
  }
  constexpr bool is_numeric() const {
    return type_kind() == TypeKind::kNumeric;
  }
  constexpr bool is_abstract_ref() const {
    return type_kind() == TypeKind::kAbstractRef;
  }
  // Any kind of reference: abstract or indexed.
  constexpr bool is_ref() const {
    return value_type_impl::IsRefField::decode(bit_field_);
  }
  // Any GenericKind: abstract reference or internal sentinel.
  constexpr bool is_generic() const {
    TypeKind kind = type_kind();
    return kind == TypeKind::kAbstractRef || kind == TypeKind::kSentinel;
  }

  constexpr Nullability nullability() const {
    DCHECK(is_ref());
    return value_type_impl::IsNullableField::decode(bit_field_);
  }
  constexpr bool is_nullable() const {
    return is_ref() && static_cast<bool>(nullability());
  }
  constexpr bool is_non_nullable() const {
    return is_ref() && !static_cast<bool>(nullability());
  }
  constexpr Exactness exactness() const {
    return value_type_impl::IsExactField::decode(bit_field_);
  }
  constexpr bool is_exact() const { return exactness() == Exactness::kExact; }
  constexpr bool is_shared() const {
    return value_type_impl::IsSharedField::decode(bit_field_);
  }
  constexpr RefTypeKind ref_type_kind() const {
    return value_type_impl::RefTypeKindField::decode(bit_field_);
  }

  constexpr StandardType standard_type() const {
    DCHECK(!has_index());
    return static_cast<StandardType>(
        value_type_impl::PayloadField::decode(bit_field_));
  }
  constexpr NumericKind numeric_kind() const {
    DCHECK(is_numeric());
    return static_cast<NumericKind>(bit_field_ &
                                    value_type_impl::kNumericKindMask);
  }
  constexpr GenericKind generic_kind() const {
    DCHECK(is_generic());
    return static_cast<GenericKind>(bit_field_ &
                                    value_type_impl::kGenericKindMask);
  }

  constexpr bool is_bottom() const {
    uint32_t bits = bit_field_ & value_type_impl::kGenericKindMask;
    return bits == static_cast<uint32_t>(GenericKind::kBottom);
  }
  constexpr bool is_top() const {
    uint32_t bits = bit_field_ & value_type_impl::kGenericKindMask;
    return bits == static_cast<uint32_t>(GenericKind::kTop);
  }
  constexpr bool is_void() const {
    uint32_t bits = bit_field_ & value_type_impl::kGenericKindMask;
    return bits == static_cast<uint32_t>(GenericKind::kVoid);
  }
  constexpr bool is_string_view() const {
    uint32_t bits = bit_field_ & value_type_impl::kGenericKindMask;
    return bits == static_cast<uint32_t>(GenericKind::kStringViewWtf8) ||
           bits == static_cast<uint32_t>(GenericKind::kStringViewWtf16) ||
           bits == static_cast<uint32_t>(GenericKind::kStringViewIter);
  }
  constexpr bool is_packed() const {
    return bit_field_ == static_cast<uint32_t>(NumericKind::kI8) ||
           bit_field_ == static_cast<uint32_t>(NumericKind::kI16) ||
           bit_field_ == static_cast<uint32_t>(NumericKind::kF16);
  }
  constexpr bool is_reference_to(GenericKind type) const {
    return is_abstract_ref() && generic_kind() == type;
  }
  constexpr bool is_defaultable() const {
    return is_numeric() || is_nullable();
  }
  constexpr bool is_uninhabited() const {
    if (is_bottom()) return true;
    if (is_defaultable()) return false;  // Includes all nullable refs.
    if (has_index()) return false;
    // Non-nullable references to null types are uninhabitable.
    return IsNullKind(generic_kind());
  }
  constexpr bool use_wasm_null() const {
    DCHECK(is_ref());
    // TODO(jkummerow): Consider calling {wasm::IsSubtypeOf}.
    if (has_index()) return true;
    GenericKind ref = generic_kind();
    if (ref == GenericKind::kExtern) return false;
    if (ref == GenericKind::kExternString) return false;
    if (ref == GenericKind::kNoExtern) return false;
    return true;
  }

  constexpr int value_kind_size_log2() const {
    DCHECK(!is_sentinel());  // Caller's responsibility.
    if (is_ref()) return kTaggedSizeLog2;
    constexpr uint8_t kValueKindSizeLog2[] = {
#define VALUE_KIND_SIZE_LOG2(kind, log2Size, ...) log2Size,
        FOREACH_NUMERIC_VALUE_TYPE(VALUE_KIND_SIZE_LOG2)
#undef VALUE_KIND_SIZE_LOG2
    };
    uint32_t index = value_type_impl::ToZeroBasedIndex(numeric_kind());
    return kValueKindSizeLog2[index];
  }

  constexpr int value_kind_size() const {
    DCHECK(!is_sentinel());  // Caller's responsibility.
    if (is_ref()) return kTaggedSize;
    constexpr uint8_t kValueKindSize[] = {
#define ELEM_SIZE_LOG2(kind, log2Size, ...) (1 << log2Size),
        FOREACH_NUMERIC_VALUE_TYPE(ELEM_SIZE_LOG2)
#undef ELEM_SIZE_LOG2
    };
    uint32_t index = value_type_impl::ToZeroBasedIndex(numeric_kind());
    return kValueKindSize[index];
  }

  constexpr int value_kind_full_size() const {
    if (is_ref()) {
      // Uncompressed pointer size.
      return kSystemPointerSize;
    }
    return value_kind_size();
  }

  /*************************** Machine-type related ***************************/
  constexpr MachineType machine_type() const {
    // TODO(jkummerow): Do any callers ever pass a sentinel here?
    if (is_sentinel()) return MachineType::None();
    if (is_ref()) return MachineType::AnyTagged();
    constexpr MachineType kMachineType[] = {
#define MACH_TYPE(kind, log2Size, code, machineType, ...) \
  MachineType::machineType(),
        FOREACH_NUMERIC_VALUE_TYPE(MACH_TYPE)
#undef MACH_TYPE
    };
    uint32_t index = value_type_impl::ToZeroBasedIndex(numeric_kind());
    return kMachineType[index];
  }

  constexpr MachineRepresentation machine_representation() const {
    return machine_type().representation();
  }

  /********************************* Encoding *********************************/
  // Returns the first byte of this type's representation in the wasm binary
  // format. Prioritizes shorthand encodings, e.g. kFuncRefCode.
  ValueTypeCode value_type_code() const {
    if (is_numeric()) return value_type_code_numeric();
    if (is_ref() && encoding_needs_heap_type()) {
      return is_nullable() ? kRefNullCode : kRefCode;
    }
    DCHECK(!has_index());  // Handled by {encoding_needs_heap_type()}.
    return value_type_code_generic();
  }

  // Returns true iff the heap type is needed to encode this type in the wasm
  // binary format, taking into account available type shorthands.
  constexpr bool encoding_needs_heap_type() const {
    if (has_index()) return true;
    if (!is_abstract_ref()) return false;
    return !is_nullable() || is_shared();
  }

  constexpr bool encoding_needs_shared() const {
    return is_abstract_ref() && is_shared();
  }

  constexpr bool encoding_needs_exact() const {
    return has_index() && is_exact();
  }

  V8_EXPORT_PRIVATE ValueTypeCode value_type_code_numeric() const;
  V8_EXPORT_PRIVATE ValueTypeCode value_type_code_generic() const;

  /****************************** Pretty-printing *****************************/
  constexpr char short_name() const {
    DCHECK(!is_sentinel());  // Caller's responsibility.
    if (is_ref()) {
      return is_nullable() ? 'n' : 'r';
    }
    constexpr const char kNumericShortName[] = {
#define SHORT_NAME(kind, log2, code, mtype, shortName, ...) shortName,
        FOREACH_NUMERIC_VALUE_TYPE(SHORT_NAME)
#undef SHORT_NAME
    };
    uint32_t index = value_type_impl::ToZeroBasedIndex(numeric_kind());
    return kNumericShortName[index];
  }

  V8_EXPORT_PRIVATE std::string generic_heaptype_name() const;
  V8_EXPORT_PRIVATE std::string name() const;

  constexpr bool is_valid() const {
    static_assert(static_cast<int>(StandardType::kI32) >
                  static_cast<int>(StandardType::kAny));
    uint32_t payload = value_type_impl::PayloadField::decode(bit_field_);
    if (is_numeric()) {
      // For numeric types, everything outside the payload is fixed.
      if ((bit_field_ & ~value_type_impl::PayloadField::kMask) !=
          value_type_impl::kNumericExtraBits) {
        return false;
      }
      return payload >= value_type_impl::kNumberOfGenericKinds &&
             payload < value_type_impl::kNumberOfStandardTypes;
    }
    if (!has_index()) {
      // Generic types must be part of the predefined set.
      if (payload >= value_type_impl::kNumberOfGenericKinds) return false;
    }
    // Both generic and indexed ref types must leave the reserved bits free.
    return value_type_impl::ReservedField::decode(bit_field_) == 0;
  }

  constexpr uint32_t raw_bit_field() const { return bit_field_; }

  size_t hash_value() const { return bit_field_; }

  /************************* Incremental transition ***************************/
  // The following methods are deprecated. Their usage should be replaced.
  constexpr bool is_reference() const { return is_ref(); }
  constexpr bool is_object_reference() const { return is_ref(); }

  static constexpr ValueTypeBase Primitive(ValueKind kind) {
    switch (kind) {
      case kI32:
        return ValueTypeBase(NumericKind::kI32);
      case kI64:
        return ValueTypeBase(NumericKind::kI64);
      case kF32:
        return ValueTypeBase(NumericKind::kF32);
      case kF64:
        return ValueTypeBase(NumericKind::kF64);
      case kS128:
        return ValueTypeBase(NumericKind::kS128);
      case kI8:
        return ValueTypeBase(NumericKind::kI8);
      case kI16:
        return ValueTypeBase(NumericKind::kI16);
      case kF16:
        return ValueTypeBase(NumericKind::kF16);
      case kVoid:
        return ValueTypeBase(GenericKind::kVoid, kNonNullable, false);
      case kRef:
      case kRefNull:
      case kTop:
      case kBottom:
        UNREACHABLE();
    }
  }

  constexpr ValueKind kind() const {
    if (is_numeric()) {
      switch (numeric_kind()) {
        case NumericKind::kI32:
          return kI32;
        case NumericKind::kI64:
          return kI64;
        case NumericKind::kF32:
          return kF32;
        case NumericKind::kF64:
          return kF64;
        case NumericKind::kS128:
          return kS128;
        case NumericKind::kI8:
          return kI8;
        case NumericKind::kI16:
          return kI16;
        case NumericKind::kF16:
          return kF16;
      }
      UNREACHABLE();
    }
    if (is_top()) return kTop;
    if (is_bottom()) return kBottom;
    if (is_void()) return kVoid;
    return is_nullable() ? kRefNull : kRef;
  }

  constexpr uint32_t raw_heap_representation(bool distinguish_shared) const;

 protected:
  friend class CanonicalValueType;
  friend class HeapType;
  friend class IndependentValueType;
  friend class ValueType;

  explicit constexpr ValueTypeBase(uint32_t bit_field) : bit_field_(bit_field) {
    DCHECK(is_valid());
  }

  explicit constexpr ValueTypeBase(NumericKind kind)
      : bit_field_(static_cast<uint32_t>(kind)) {
    DCHECK(is_numeric());
  }

  explicit constexpr ValueTypeBase(GenericKind kind, Nullability nullable,
                                   bool is_shared)
      : bit_field_(static_cast<uint32_t>(kind) |
                   value_type_impl::IsNullableField::encode(nullable) |
                   value_type_impl::IsSharedField::encode(is_shared)) {
    DCHECK(is_generic());
  }

  explicit constexpr ValueTypeBase(TypeIndex index, Nullability nullable,
                                   Exactness exact, bool shared,
                                   RefTypeKind ref_type_kind)
      : bit_field_(
            value_type_impl::TypeKindField::encode(TypeKind::kIndexedRef) |
            value_type_impl::IsNullableField::encode(nullable) |
            value_type_impl::IsExactField::encode(exact) |
            value_type_impl::IsSharedField::encode(shared) |
            value_type_impl::RefTypeKindField::encode(ref_type_kind) |
            value_type_impl::PayloadField::encode(index.index)) {
    // We shouldn't need this, but experience has shown that having an extra
    // "safety net" here is valuable every now and then.
    CHECK(value_type_impl::PayloadField::is_valid(index.index));
  }

  constexpr TypeIndex raw_index() const {
    DCHECK(has_index());
    return TypeIndex(value_type_impl::PayloadField::decode(bit_field_));
  }

  uint32_t bit_field_;
};
ASSERT_TRIVIALLY_COPYABLE(ValueTypeBase);

// A HeapType is like a ValueType where {is_numeric()} is known to be false
// and the value of {is_nullable()} is ignored.
// Uses module-specific type indices.
class HeapType : public ValueTypeBase {
 public:
  static constexpr HeapType Generic(GenericKind kind, bool shared) {
    return HeapType{ValueTypeBase(kind, kNullable, shared)};
  }
  static constexpr HeapType Index(ModuleTypeIndex index, bool shared,
                                  RefTypeKind kind,
                                  Exactness exact = Exactness::kAnySubtype) {
    return HeapType{ValueTypeBase(index, kNullable, exact, shared, kind)};
  }
  static constexpr HeapType FromBits(uint32_t bits) {
    HeapType type{ValueTypeBase(bits)};
    DCHECK(!type.is_numeric());
    return type;
  }

  static constexpr HeapType from_code(uint8_t code, bool is_shared) {
    constexpr uint8_t kFirst = ValueTypeCode::kFirstHeapTypeCode;
    constexpr uint8_t kLast = ValueTypeCode::kLastHeapTypeCode;
    if (code < kFirst || code > kLast) {
      return Generic(GenericKind::kBottom, false);
    }
    constexpr size_t kNumCases = kLast - kFirst + 1;
    constexpr std::array<GenericKind, kNumCases> kLookupTable =
        base::make_array<kNumCases>([](size_t i) {
          switch (i) {
#define CASE(name, code, ...)                                         \
  case (k##code##Code - kFirst):                                      \
    static_assert(k##code##Code >= kFirst && k##code##Code <= kLast); \
    return GenericKind::k##name;
            FOREACH_ABSTRACT_TYPE(CASE)
#undef CASE
            default:
              return GenericKind::kBottom;
          }
        });
    return Generic(kLookupTable[code - kFirst], is_shared);
  }

  constexpr HeapType AsExact(Exactness exact = Exactness::kExact) const {
    return HeapType{ValueTypeBase(
        value_type_impl::IsExactField::update(raw_bit_field(), exact))};
  }

  int32_t code() const {
    if (has_index()) return static_cast<int32_t>(ref_index().index);
    // Value type codes represent the first byte of the LEB128 encoding. To get
    // the int32 represented by a code, we sign-extend it from 7 to 32 bits.
    return 0xFFFF'FF80 | value_type_code_generic();
  }

  constexpr ModuleTypeIndex ref_index() const {
    return ModuleTypeIndex{raw_index()};
  }

  constexpr bool operator==(HeapType other) const {
    constexpr uint32_t kMask = ~value_type_impl::IsRefField::kMask &
                               ~value_type_impl::IsNullableField::kMask;
    return (bit_field_ & kMask) == (other.bit_field_ & kMask);
  }

  /****************************** Pretty-printing *****************************/
  std::string name() const {
    if (has_index()) {
      if (is_exact()) return "exact " + std::to_string(raw_index().index);
      return std::to_string(raw_index().index);
    }
    return generic_heaptype_name();
  }

  /************************* Incremental transition ***************************/
  // The following methods are deprecated. Their usage should be replaced.
  enum Representation : uint32_t {
    kFunc = kV8MaxWasmTypes,  // shorthand: c
    kEq,                      // shorthand: q
    kI31,                     // shorthand: j
    kStruct,                  // shorthand: o
    kArray,                   // shorthand: g
    kAny,                     //
    kExtern,                  // shorthand: a.
    kExternString,            // Internal type for optimization purposes.
                              // Subtype of extern.
                              // Used by the js-builtin-strings proposal.
    kExn,                     //
    kString,                  // shorthand: w.
    kStringViewWtf8,          // shorthand: x.
    kStringViewWtf16,         // shorthand: y.
    kStringViewIter,          // shorthand: z.
    kNone,                    //
    kNoFunc,                  //
    kNoExtern,                //
    kNoExn,                   //
    kCont,                    // shorthand: k.
    kContShared,
    kNoCont,  // bottom continuation type
    kFuncShared,
    kEqShared,
    kI31Shared,
    kStructShared,
    kArrayShared,
    kAnyShared,
    kExternShared,
    kExternStringShared,
    kExnShared,
    kStringShared,
    kStringViewWtf8Shared,
    kStringViewWtf16Shared,
    kStringViewIterShared,
    kNoneShared,
    kNoFuncShared,
    kNoExternShared,
    kNoExnShared,
    kNoContShared,
    // This value is an internal type (not part of the Wasm spec) that
    // is the common supertype across all type hierarchies.  It should never
    // appear in validated Wasm programs, but is used to signify that we don't
    // have any information about a particular value and to prevent bugs in our
    // typed optimizations, see crbug.com/361652141. Note: kTop is the neutral
    // element wrt. to intersection (whereas kBottom is for union), and kBottom
    // is indicating unreachable code, which might be used for subsequent
    // optimizations, e.g., DCE.
    kTop,
    // This value is used to represent failures in the parsing of heap types and
    // does not correspond to a Wasm heap type. It has to be last in this list.
    kBottom
  };
  constexpr Representation representation() const {
    return static_cast<Representation>(raw_heap_representation(true));
  }
  constexpr bool is_index() const { return has_index(); }

 private:
  // Hide inherited methods that don't make sense for HeapTypes.
  constexpr bool is_nullable() const;
  constexpr bool is_non_nullable() const;
  constexpr Nullability nullability() const;
};

// Deprecated.
constexpr uint32_t ValueTypeBase::raw_heap_representation(
    bool distinguish_shared) const {
  DCHECK(!is_numeric());
  if (has_index()) return raw_index().index;
  switch (generic_kind()) {
    case GenericKind::kTop:
      return HeapType::kTop;
    case GenericKind::kBottom:
      return HeapType::kBottom;

#define CASE(name, ...)                                                  \
  case GenericKind::k##name:                                             \
    return distinguish_shared && is_shared() ? HeapType::k##name##Shared \
                                             : HeapType::k##name;
      FOREACH_ABSTRACT_TYPE(CASE)
#undef CASE

    case GenericKind::kExternString:
      return distinguish_shared && is_shared() ? HeapType::kExternStringShared
                                               : HeapType::kExternString;
    case GenericKind::kVoid:
      UNREACHABLE();
  }
}

class CanonicalValueType;

// Uses module-specific type indices.
class ValueType : public ValueTypeBase {
 public:
  static constexpr ValueType Primitive(NumericKind kind) {
    return ValueType{ValueTypeBase(kind)};
  }
  static constexpr ValueType Generic(GenericKind kind, Nullability nullable,
                                     bool shared) {
    return ValueType{ValueTypeBase(kind, nullable, shared)};
  }
  static constexpr ValueType Ref(ModuleTypeIndex index, bool shared,
                                 RefTypeKind kind) {
    return ValueType{ValueTypeBase(index, kNonNullable, Exactness::kAnySubtype,
                                   shared, kind)};
  }
  static constexpr ValueType Ref(HeapType type) {
    return ValueType{type}.AsNonNull();
  }
  static constexpr ValueType RefNull(ModuleTypeIndex index, bool shared,
                                     RefTypeKind kind) {
    return ValueType{
        ValueTypeBase(index, kNullable, Exactness::kAnySubtype, shared, kind)};
  }
  static constexpr ValueType RefNull(HeapType type) {
    return ValueType{type}.AsNullable(kNullable);
  }
  static constexpr ValueType RefMaybeNull(ModuleTypeIndex index,
                                          Nullability nullable, bool shared,
                                          RefTypeKind kind) {
    return ValueType{
        ValueTypeBase(index, nullable, Exactness::kAnySubtype, shared, kind)};
  }
  static constexpr ValueType RefMaybeNull(HeapType type, Nullability nullable) {
    return ValueType{type}.AsNullable(nullable);
  }

  static constexpr ValueType FromRawBitField(uint32_t bits) {
    return ValueType{ValueTypeBase(bits)};
  }

  constexpr ValueType AsNonNull() const { return AsNullable(kNonNullable); }
  constexpr ValueType AsNullable(Nullability nullable = kNullable) const {
    DCHECK(!is_numeric());
    return ValueType{ValueTypeBase(
        value_type_impl::IsNullableField::update(raw_bit_field(), nullable))};
  }

  constexpr ValueType AsExact(Exactness exact = Exactness::kExact) const {
    DCHECK(!is_numeric());
    return ValueType{ValueTypeBase(
        value_type_impl::IsExactField::update(raw_bit_field(), exact))};
  }

  // This will be replaced by direct calls to {AsExact} once the proposal
  // has shipped by default and the flag is removed.
  ValueType AsExactIfProposalEnabled(
      Exactness exact = Exactness::kExact) const {
    if V8_LIKELY (!v8_flags.experimental_wasm_custom_descriptors) return *this;
    return AsExact(exact);
  }

  constexpr ValueType AsNonShared() const {
    if (!is_ref()) return *this;
    return ValueType{ValueTypeBase(
        value_type_impl::IsSharedField::update(raw_bit_field(), false))};
  }

  constexpr ValueType Unpacked() const {
    if (bit_field_ == static_cast<uint32_t>(NumericKind::kI8) ||
        bit_field_ == static_cast<uint32_t>(NumericKind::kI16)) {
      return Primitive(NumericKind::kI32);
    }
    if (bit_field_ == static_cast<uint32_t>(NumericKind::kF16)) {
      return Primitive(NumericKind::kF32);
    }
    return *this;
  }

  constexpr CanonicalValueType Canonicalize(CanonicalTypeIndex index) const;

  // For incremental transition:
  static constexpr ValueType Primitive(ValueKind kind) {
    return ValueType{ValueTypeBase::Primitive(kind)};
  }

  // For incremental transition:
  constexpr HeapType::Representation heap_representation() const {
    return static_cast<HeapType::Representation>(raw_heap_representation(true));
  }

  // For incremental transition:
  constexpr bool is_reference_to(HeapType::Representation repr) const {
    return is_ref() && heap_representation() == repr;
  }
  // Un-hide the superclass method (which is here to stay):
  constexpr bool is_reference_to(GenericKind kind) const {
    return ValueTypeBase::is_reference_to(kind);
  }

  static ValueType For(MachineType type) {
    switch (type.representation()) {
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return Primitive(NumericKind::kI32);
      case MachineRepresentation::kWord64:
        return Primitive(NumericKind::kI64);
      case MachineRepresentation::kFloat32:
        return Primitive(NumericKind::kF32);
      case MachineRepresentation::kFloat64:
        return Primitive(NumericKind::kF64);
      case MachineRepresentation::kTaggedPointer:
        return Generic(GenericKind::kAny, kNullable, kNotShared);
      case MachineRepresentation::kSimd128:
        return Primitive(NumericKind::kS128);
      default:
        UNREACHABLE();
    }
  }

  constexpr bool operator==(ValueType other) const {
    return bit_field_ == other.bit_field_;
  }

  constexpr HeapType heap_type() const {
    DCHECK(!is_numeric());
    return HeapType{*this};
  }

  constexpr ModuleTypeIndex ref_index() const {
    return ModuleTypeIndex{raw_index()};
  }
};
ASSERT_TRIVIALLY_COPYABLE(ValueType);

// Uses canonicalized type indices.
class CanonicalValueType : public ValueTypeBase {
 public:
  static constexpr CanonicalValueType Primitive(NumericKind kind) {
    return CanonicalValueType{ValueTypeBase(kind)};
  }
  static constexpr CanonicalValueType Ref(CanonicalTypeIndex index, bool shared,
                                          RefTypeKind kind) {
    return CanonicalValueType{ValueTypeBase(
        index, kNonNullable, Exactness::kAnySubtype, shared, kind)};
  }
  static constexpr CanonicalValueType RefNull(CanonicalTypeIndex index,
                                              bool shared, RefTypeKind kind) {
    return CanonicalValueType{
        ValueTypeBase(index, kNullable, Exactness::kAnySubtype, shared, kind)};
  }
  // Technically this should take a "CanonicalHeapType", but we don't have
  // that class because there's not enough use case for it.
  static constexpr CanonicalValueType RefMaybeNull(CanonicalValueType type,
                                                   Nullability nullable) {
    return CanonicalValueType{
        ValueTypeBase(value_type_impl::IsNullableField::update(
            type.raw_bit_field(), nullable))};
  }

  static constexpr CanonicalValueType FromRawBitField(uint32_t bits) {
    return CanonicalValueType{ValueTypeBase(bits)};
  }

  constexpr CanonicalValueType AsExact(
      Exactness exact = Exactness::kExact) const {
    DCHECK(!is_numeric());
    return CanonicalValueType{ValueTypeBase(
        value_type_impl::IsExactField::update(raw_bit_field(), exact))};
  }

  // This will be replaced by direct calls to {AsExact} once the proposal
  // has shipped by default and the flag is removed.
  CanonicalValueType AsExactIfProposalEnabled(
      Exactness exact = Exactness::kExact) const {
    if V8_LIKELY (!v8_flags.experimental_wasm_custom_descriptors) return *this;
    return AsExact(exact);
  }

  constexpr CanonicalTypeIndex ref_index() const {
    return CanonicalTypeIndex{raw_index()};
  }

  constexpr bool operator==(CanonicalValueType other) const {
    return bit_field_ == other.bit_field_;
  }

  constexpr bool is_equal_except_index(CanonicalValueType other) const {
    return (bit_field_ & ~kIndexBits) == (other.bit_field_ & ~kIndexBits);
  }

  constexpr bool IsFunctionType() const {
    return ref_type_kind() == RefTypeKind::kFunction;
  }

  // For incremental transition.
  constexpr HeapType::Representation heap_representation() const {
    return static_cast<HeapType::Representation>(raw_heap_representation(true));
  }

  // For incremental transition.
  constexpr HeapType::Representation heap_representation_non_shared() const {
    return static_cast<HeapType::Representation>(
        raw_heap_representation(false));
  }

  // For incremental transition:
  constexpr bool is_reference_to(HeapType::Representation repr) const {
    return is_ref() && heap_representation() == repr;
  }
};
ASSERT_TRIVIALLY_COPYABLE(CanonicalValueType);

constexpr CanonicalValueType ValueType::Canonicalize(
    CanonicalTypeIndex index) const {
  DCHECK(has_index());
  return CanonicalValueType{ValueTypeBase(
      value_type_impl::PayloadField::update(raw_bit_field(), index.index))};
}

// Non-indexed types. We use them for global constants; they have the
// specific characteristic that they can be compared and implicitly converted
// to both HeapTypes/ValueTypes and CanonicalValueTypes.
class IndependentValueType : public ValueTypeBase {
 public:
  explicit constexpr IndependentValueType(NumericKind kind)
      : ValueTypeBase(kind) {}

  constexpr operator ValueType() const { return ValueType{*this}; }
  constexpr operator CanonicalValueType() const {
    return CanonicalValueType{*this};
  }

  // Implicit conversions aren't enough when the IndependentValueType is the
  // left-hand side.
  constexpr bool operator==(ValueType b) const {
    return raw_bit_field() == b.raw_bit_field();
  }
  constexpr bool operator==(CanonicalValueType b) const {
    return raw_bit_field() == b.raw_bit_field();
  }

 protected:
  explicit constexpr IndependentValueType(GenericKind kind,
                                          Nullability nullable, bool shared)
      : ValueTypeBase(kind, nullable, shared) {}
};
class IndependentHeapType : public IndependentValueType {
 public:
  explicit constexpr IndependentHeapType(GenericKind kind,
                                         Nullability nullable = kNullable,
                                         bool shared = false)
      : IndependentValueType(kind, nullable, shared) {}

  constexpr IndependentHeapType AsNonNull() const {
    return IndependentHeapType(generic_kind(), kNonNullable, is_shared());
  }

  constexpr HeapType heap_type() const { return HeapType{*this}; }
  constexpr operator HeapType() const { return HeapType{*this}; }
};

// TODO(jkummerow): See how many of these ValueKind-based helpers we can
// replace with ValueType methods.

constexpr bool is_reference(ValueKind kind) {
  return kind == kRef || kind == kRefNull;
}

constexpr bool is_object_reference(ValueKind kind) {
  return kind == kRef || kind == kRefNull;
}

constexpr int value_kind_size_log2(ValueKind kind) {
  constexpr int8_t kValueKindSizeLog2[] = {
#define VALUE_KIND_SIZE_LOG2(kind, log2Size, ...) log2Size,
      FOREACH_VALUE_TYPE(VALUE_KIND_SIZE_LOG2)
#undef VALUE_KIND_SIZE_LOG2
  };

  int size_log_2 = kValueKindSizeLog2[kind];
  DCHECK_LE(0, size_log_2);
  return size_log_2;
}

constexpr int value_kind_size(ValueKind kind) {
  constexpr int8_t kElementSize[] = {
#define ELEM_SIZE_LOG2(kind, log2Size, ...) \
  log2Size == -1 ? -1 : (1 << std::max(0, log2Size)),
      FOREACH_VALUE_TYPE(ELEM_SIZE_LOG2)
#undef ELEM_SIZE_LOG2
  };

  int size = kElementSize[kind];
  DCHECK_LT(0, size);
  return size;
}

constexpr int value_kind_full_size(ValueKind kind) {
  if (is_reference(kind)) {
    // Uncompressed pointer size.
    return kSystemPointerSize;
  }
  return value_kind_size(kind);
}

constexpr const char* name(ValueKind kind) {
  constexpr const char* kKindName[] = {
#define KIND_NAME(kind, log2Size, code, machineType, shortName, kindName, ...) \
  kindName,
      FOREACH_VALUE_TYPE(KIND_NAME)
#undef TYPE_NAME
  };

  return kKindName[kind];
}

// Output operator, useful for DCHECKS and others.
inline std::ostream& operator<<(std::ostream& oss, ValueKind kind) {
  return oss << name(kind);
}

constexpr MachineType machine_type(ValueKind kind) {
  DCHECK_NE(kBottom, kind);

  constexpr MachineType kMachineType[] = {
#define MACH_TYPE(kind, log2Size, code, machineType, ...) \
  MachineType::machineType(),
      FOREACH_VALUE_TYPE(MACH_TYPE)
#undef MACH_TYPE
  };

  return kMachineType[kind];
}

constexpr bool is_packed(ValueKind kind) {
  return kind == kI8 || kind == kI16 || kind == kF16;
}
constexpr ValueKind unpacked(ValueKind kind) {
  return is_packed(kind) ? (kind == kF16 ? kF32 : kI32) : kind;
}

static_assert(sizeof(ValueTypeBase) <= kUInt32Size,
              "ValueType is small and can be passed by value");
static_assert(ValueTypeBase::kLastUsedBit < kSmiValueSize,
              "ValueType has space to be encoded in a Smi");

// Output operator, useful for DCHECKS and others.
inline std::ostream& operator<<(std::ostream& oss, ValueType type) {
  return oss << type.name() << " (raw=0x" << std::hex << type.raw_bit_field()
             << ")" << std::dec;
}

// Precomputed primitive types.
constexpr IndependentValueType kWasmI32{NumericKind::kI32};
constexpr IndependentValueType kWasmI64{NumericKind::kI64};
constexpr IndependentValueType kWasmF32{NumericKind::kF32};
constexpr IndependentValueType kWasmF64{NumericKind::kF64};
constexpr IndependentValueType kWasmS128{NumericKind::kS128};
constexpr IndependentValueType kWasmI8{NumericKind::kI8};
constexpr IndependentValueType kWasmI16{NumericKind::kI16};
constexpr IndependentValueType kWasmF16{NumericKind::kF16};
constexpr IndependentHeapType kWasmVoid{GenericKind::kVoid, kNonNullable};
// The abstract top type (super type of all other types).
constexpr IndependentHeapType kWasmTop{GenericKind::kTop};
constexpr IndependentHeapType kWasmBottom{GenericKind::kBottom, kNonNullable};
// Established reference-type and wasm-gc proposal shorthands.
constexpr IndependentHeapType kWasmFuncRef{GenericKind::kFunc};
constexpr IndependentHeapType kWasmAnyRef{GenericKind::kAny};
constexpr IndependentHeapType kWasmExternRef{GenericKind::kExtern};
constexpr IndependentHeapType kWasmRefExtern{GenericKind::kExtern,
                                             kNonNullable};
constexpr IndependentHeapType kWasmExnRef{GenericKind::kExn};
constexpr IndependentHeapType kWasmEqRef{GenericKind::kEq};
constexpr IndependentHeapType kWasmI31Ref{GenericKind::kI31};
constexpr IndependentHeapType kWasmRefI31{GenericKind::kI31, kNonNullable};
constexpr IndependentHeapType kWasmStructRef{GenericKind::kStruct};
constexpr IndependentHeapType kWasmArrayRef{GenericKind::kArray};
constexpr IndependentHeapType kWasmStringRef{GenericKind::kString};
constexpr IndependentHeapType kWasmRefString{GenericKind::kString,
                                             kNonNullable};
constexpr IndependentHeapType kWasmRefNullExternString{
    GenericKind::kExternString};
constexpr IndependentHeapType kWasmRefExternString{GenericKind::kExternString,
                                                   kNonNullable};
constexpr IndependentHeapType kWasmStringViewWtf8{GenericKind::kStringViewWtf8,
                                                  kNonNullable};
constexpr IndependentHeapType kWasmStringViewWtf16{
    GenericKind::kStringViewWtf16, kNonNullable};
constexpr IndependentHeapType kWasmStringViewIter{GenericKind::kStringViewIter,
                                                  kNonNullable};
constexpr IndependentHeapType kWasmNullRef{GenericKind::kNone};
constexpr IndependentHeapType kWasmNullExternRef{GenericKind::kNoExtern};
constexpr IndependentHeapType kWasmNullExnRef{GenericKind::kNoExn};
constexpr IndependentHeapType kWasmNullFuncRef{GenericKind::kNoFunc};
constexpr IndependentHeapType kWasmContRef{GenericKind::kCont};
constexpr IndependentHeapType kWasmNullContRef{GenericKind::kNoCont};

#define FOREACH_WASMVALUE_CTYPES(V) \
  V(kI32, int32_t)                  \
  V(kI64, int64_t)                  \
  V(kF32, float)                    \
  V(kF64, double)                   \
  V(kS128, Simd128)

using FunctionSig = Signature<ValueType>;

class CanonicalSig : public Signature<CanonicalValueType> {
 public:
  CanonicalSig(size_t return_count, size_t parameter_count,
               const CanonicalValueType* reps)
      : Signature<CanonicalValueType>(return_count, parameter_count, reps) {}

  class Builder : public SignatureBuilder<CanonicalSig, CanonicalValueType> {
   public:
    Builder(Zone* zone, size_t return_count, size_t parameter_count)
        : SignatureBuilder<CanonicalSig, CanonicalValueType>(zone, return_count,
                                                             parameter_count) {}
    CanonicalSig* Get() const;
  };

  uint64_t signature_hash() const { return signature_hash_; }

 private:
  uint64_t signature_hash_;
};

// This is the special case where comparing module-specific to canonical
// signatures is safe: when they only contain numerical types.
V8_EXPORT_PRIVATE bool EquivalentNumericSig(const CanonicalSig* a,
                                            const FunctionSig* b);

#define FOREACH_LOAD_TYPE(V) \
  V(I32, , Int32)            \
  V(I32, 8S, Int8)           \
  V(I32, 8U, Uint8)          \
  V(I32, 16S, Int16)         \
  V(I32, 16U, Uint16)        \
  V(I64, , Int64)            \
  V(I64, 8S, Int8)           \
  V(I64, 8U, Uint8)          \
  V(I64, 16S, Int16)         \
  V(I64, 16U, Uint16)        \
  V(I64, 32S, Int32)         \
  V(I64, 32U, Uint32)        \
  V(F32, F16, Float16)       \
  V(F32, , Float32)          \
  V(F64, , Float64)          \
  V(S128, , Simd128)

class LoadType {
 public:
  enum LoadTypeValue : uint8_t {
#define DEF_ENUM(type, suffix, ...) k##type##Load##suffix,
    FOREACH_LOAD_TYPE(DEF_ENUM)
#undef DEF_ENUM
  };

  // Allow implicit conversion of the enum value to this wrapper.
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr LoadType(LoadTypeValue val) : val_(val) {}

  constexpr LoadTypeValue value() const { return val_; }
  constexpr uint8_t size_log_2() const { return kLoadSizeLog2[val_]; }
  constexpr uint8_t size() const { return kLoadSize[val_]; }
  constexpr ValueType value_type() const { return kValueType[val_]; }
  constexpr MachineType mem_type() const { return kMemType[val_]; }

  static LoadType ForValueKind(ValueKind kind, bool is_signed = false) {
    switch (kind) {
      case kI32:
        return kI32Load;
      case kI64:
        return kI64Load;
      case kF32:
        return kF32Load;
      case kF64:
        return kF64Load;
      case kS128:
        return kS128Load;
      case kI8:
        return is_signed ? kI32Load8S : kI32Load8U;
      case kI16:
        return is_signed ? kI32Load16S : kI32Load16U;
      case kF16:
        return kF32LoadF16;
      default:
        UNREACHABLE();
    }
  }

 private:
  LoadTypeValue val_;

  static constexpr uint8_t kLoadSize[] = {
  // MSVC wants a static_cast here.
#define LOAD_SIZE(_, __, memtype) \
  static_cast<uint8_t>(           \
      ElementSizeInBytes(MachineType::memtype().representation())),
      FOREACH_LOAD_TYPE(LOAD_SIZE)
#undef LOAD_SIZE
  };

  static constexpr uint8_t kLoadSizeLog2[] = {
  // MSVC wants a static_cast here.
#define LOAD_SIZE(_, __, memtype) \
  static_cast<uint8_t>(           \
      ElementSizeLog2Of(MachineType::memtype().representation())),
      FOREACH_LOAD_TYPE(LOAD_SIZE)
#undef LOAD_SIZE
  };

  static constexpr ValueType kValueType[] = {
#define VALUE_TYPE(type, ...) ValueType::Primitive(k##type),
      FOREACH_LOAD_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineType kMemType[] = {
#define MEMTYPE(_, __, memtype) MachineType::memtype(),
      FOREACH_LOAD_TYPE(MEMTYPE)
#undef MEMTYPE
  };
};

#define FOREACH_STORE_TYPE(V) \
  V(I32, , Word32)            \
  V(I32, 8, Word8)            \
  V(I32, 16, Word16)          \
  V(I64, , Word64)            \
  V(I64, 8, Word8)            \
  V(I64, 16, Word16)          \
  V(I64, 32, Word32)          \
  V(F32, F16, Float16)        \
  V(F32, , Float32)           \
  V(F64, , Float64)           \
  V(S128, , Simd128)

class StoreType {
 public:
  enum StoreTypeValue : uint8_t {
#define DEF_ENUM(type, suffix, ...) k##type##Store##suffix,
    FOREACH_STORE_TYPE(DEF_ENUM)
#undef DEF_ENUM
  };

  // Allow implicit conversion of the enum value to this wrapper.
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr StoreType(StoreTypeValue val) : val_(val) {}

  constexpr StoreTypeValue value() const { return val_; }
  constexpr unsigned size_log_2() const { return kStoreSizeLog2[val_]; }
  constexpr unsigned size() const { return 1 << size_log_2(); }
  constexpr ValueType value_type() const { return kValueType[val_]; }
  constexpr MachineRepresentation mem_rep() const { return kMemRep[val_]; }

  static StoreType ForValueKind(ValueKind kind) {
    switch (kind) {
      case kI32:
        return kI32Store;
      case kI64:
        return kI64Store;
      case kF32:
        return kF32Store;
      case kF64:
        return kF64Store;
      case kS128:
        return kS128Store;
      case kI8:
        return kI32Store8;
      case kI16:
        return kI32Store16;
      case kF16:
        return kF32StoreF16;
      default:
        UNREACHABLE();
    }
  }

 private:
  StoreTypeValue val_;

  static constexpr uint8_t kStoreSizeLog2[] = {
  // MSVC wants a static_cast here.
#define STORE_SIZE(_, __, memrep) \
  static_cast<uint8_t>(ElementSizeLog2Of(MachineRepresentation::k##memrep)),
      FOREACH_STORE_TYPE(STORE_SIZE)
#undef STORE_SIZE
  };

  static constexpr ValueType kValueType[] = {
#define VALUE_TYPE(type, ...) ValueType::Primitive(k##type),
      FOREACH_STORE_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineRepresentation kMemRep[] = {
#define MEMREP(_, __, memrep) MachineRepresentation::k##memrep,
      FOREACH_STORE_TYPE(MEMREP)
#undef MEMREP
  };
};

std::optional<wasm::ValueKind> WasmReturnTypeFromSignature(
    const CanonicalSig* wasm_signature);

// Lowers a signature for 32 bit platforms by replacing i64 parameters and
// returns with two i32s each.
V8_EXPORT_PRIVATE const wasm::FunctionSig* GetI32Sig(
    Zone* zone, const wasm::FunctionSig* sig);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_VALUE_TYPE_H_
