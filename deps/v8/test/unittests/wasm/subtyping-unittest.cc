// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/canonical-types.h"
#include "src/wasm/wasm-subtyping.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/flag-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace subtyping_unittest {

class WasmSubtypingTest : public TestWithPlatform {};
using FieldInit = std::pair<ValueType, bool>;

constexpr ValueType ref(uint32_t index) {
  return ValueType::Ref(index, kNonNullable);
}
constexpr ValueType optRef(uint32_t index) {
  return ValueType::Ref(index, kNullable);
}

FieldInit mut(ValueType type) { return FieldInit(type, true); }
FieldInit immut(ValueType type) { return FieldInit(type, false); }

void DefineStruct(WasmModule* module, std::initializer_list<FieldInit> fields,
                  uint32_t supertype = kNoSuperType,
                  bool in_singleton_rec_group = true) {
  StructType::Builder builder(module->signature_zone.get(),
                              static_cast<uint32_t>(fields.size()));
  for (FieldInit field : fields) {
    builder.AddField(field.first, field.second);
  }
  module->add_struct_type(builder.Build(), supertype);
  if (in_singleton_rec_group) {
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 1);
  }
}

void DefineArray(WasmModule* module, FieldInit element_type,
                 uint32_t supertype = kNoSuperType,
                 bool in_singleton_rec_group = true) {
  module->add_array_type(module->signature_zone->New<ArrayType>(
                             element_type.first, element_type.second),
                         supertype);
  if (in_singleton_rec_group) {
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 1);
  }
}

void DefineSignature(WasmModule* module,
                     std::initializer_list<ValueType> params,
                     std::initializer_list<ValueType> returns,
                     uint32_t supertype = kNoSuperType,
                     bool in_singleton_rec_group = true) {
  module->add_signature(
      FunctionSig::Build(module->signature_zone.get(), returns, params),
      supertype);
  if (in_singleton_rec_group) {
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 1);
  }
}

TEST_F(WasmSubtypingTest, Subtyping) {
  FLAG_SCOPE(experimental_wasm_gc);
  v8::internal::AccountingAllocator allocator;
  WasmModule module1_(std::make_unique<Zone>(&allocator, ZONE_NAME));
  WasmModule module2_(std::make_unique<Zone>(&allocator, ZONE_NAME));

  WasmModule* module1 = &module1_;
  WasmModule* module2 = &module2_;

  // Set up two identical modules.
  for (WasmModule* module : {module1, module2}) {
    /*  0 */ DefineStruct(module, {mut(ref(2)), immut(optRef(2))});
    /*  1 */ DefineStruct(module, {mut(ref(2)), immut(ref(2))}, 0);
    /*  2 */ DefineArray(module, immut(ref(0)));
    /*  3 */ DefineArray(module, immut(ref(1)), 2);
    /*  4 */ DefineStruct(module, {mut(ref(2)), immut(ref(3)), immut(kWasmF64)},
                          1);
    /*  5 */ DefineStruct(module, {mut(optRef(2)), immut(ref(2))});
    /*  6 */ DefineArray(module, mut(kWasmI32));
    /*  7 */ DefineArray(module, immut(kWasmI32));
    /*  8 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(8))});
    /*  9 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(8))}, 8);
    /* 10 */ DefineSignature(module, {}, {});
    /* 11 */ DefineSignature(module, {kWasmI32}, {kWasmI32});
    /* 12 */ DefineSignature(module, {kWasmI32, kWasmI32}, {kWasmI32});
    /* 13 */ DefineSignature(module, {ref(1)}, {kWasmI32});
    /* 14 */ DefineSignature(module, {ref(0)}, {kWasmI32}, 13);
    /* 15 */ DefineSignature(module, {ref(0)}, {ref(4)}, 16);
    /* 16 */ DefineSignature(module, {ref(0)}, {ref(0)});
    /* 17 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(17))});

    // Rec. group.
    /* 18 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(17))}, 17,
                          false);
    /* 19 */ DefineArray(module, {mut(optRef(21))}, kNoSuperType, false);
    /* 20 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false);
    /* 21 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, 20, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    // Identical rec. group.
    /* 22 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(17))}, 17,
                          false);
    /* 23 */ DefineArray(module, {mut(optRef(25))}, kNoSuperType, false);
    /* 24 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false);
    /* 25 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, 24, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    // Nonidentical rec. group: the last function extends a type outside the
    // recursive group.
    /* 26 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(17))}, 17,
                          false);
    /* 27 */ DefineArray(module, {mut(optRef(29))}, kNoSuperType, false);
    /* 28 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false);
    /* 29 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, 20, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    /* 30 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(18))}, 18);
  }

  constexpr ValueType numeric_types[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                                         kWasmS128};
  constexpr ValueType ref_types[] = {kWasmFuncRef, kWasmEqRef,    kWasmI31Ref,
                                     kWasmDataRef, kWasmArrayRef, kWasmAnyRef,
                                     optRef(0),    ref(0),        optRef(2),
                                     ref(2),       optRef(11),    ref(11)};

// Some macros to help managing types and modules.
#define SUBTYPE(type1, type2) \
  EXPECT_TRUE(IsSubtypeOf(type1, type2, module1, module))
#define SUBTYPE_IFF(type1, type2, condition) \
  EXPECT_EQ(IsSubtypeOf(type1, type2, module1, module), condition)
#define NOT_SUBTYPE(type1, type2) \
  EXPECT_FALSE(IsSubtypeOf(type1, type2, module1, module))
// Use only with indexed types.
#define VALID_SUBTYPE(type1, type2)                                        \
  EXPECT_TRUE(ValidSubtypeDefinition(type1.ref_index(), type2.ref_index(), \
                                     module1, module));                    \
  EXPECT_TRUE(IsSubtypeOf(type1, type2, module1, module));
#define NOT_VALID_SUBTYPE(type1, type2)                                     \
  EXPECT_FALSE(ValidSubtypeDefinition(type1.ref_index(), type2.ref_index(), \
                                      module1, module));
#define IDENTICAL(index1, index2)                                         \
  EXPECT_TRUE(EquivalentTypes(ValueType::Ref(index1, kNullable),          \
                              ValueType::Ref(index2, kNullable), module1, \
                              module));
#define DISTINCT(index1, index2)                                           \
  EXPECT_FALSE(EquivalentTypes(ValueType::Ref(index1, kNullable),          \
                               ValueType::Ref(index2, kNullable), module1, \
                               module));

  for (WasmModule* module : {module1, module2}) {
    // For cross module subtyping, we need to enable type canonicalization.
    // Type judgements across modules should work the same as within one module.
    FLAG_VALUE_SCOPE(wasm_type_canonicalization, module == module2);

    // Value types are unrelated, except if they are equal.
    for (ValueType subtype : numeric_types) {
      for (ValueType supertype : numeric_types) {
        SUBTYPE_IFF(subtype, supertype, subtype == supertype);
      }
    }

    // Value types are unrelated with reference types.
    for (ValueType value_type : numeric_types) {
      for (ValueType ref_type : ref_types) {
        NOT_SUBTYPE(value_type, ref_type);
        NOT_SUBTYPE(ref_type, value_type);
      }
    }

    for (ValueType ref_type : ref_types) {
      // Concrete reference types, i31ref and dataref are subtypes of eqref,
      // externref/funcref/anyref/functions are not.
      SUBTYPE_IFF(ref_type, kWasmEqRef,
                  ref_type != kWasmFuncRef && ref_type != kWasmAnyRef &&
                      ref_type != optRef(11) && ref_type != ref(11));
      // Non-nullable struct/array types are subtypes of dataref.
      SUBTYPE_IFF(ref_type, kWasmDataRef,
                  ref_type == kWasmDataRef || ref_type == kWasmArrayRef ||
                      ref_type == ref(0) || ref_type == ref(2));
      // Non-nullable array types are subtypes of arrayref.
      SUBTYPE_IFF(ref_type, kWasmArrayRef,
                  ref_type == kWasmArrayRef || ref_type == ref(2));
      // Functions are subtypes of funcref.
      SUBTYPE_IFF(ref_type, kWasmFuncRef,
                  ref_type == kWasmFuncRef || ref_type == optRef(11) ||
                      ref_type == ref(11));
      // Each reference type is a subtype of itself.
      SUBTYPE(ref_type, ref_type);
      // Each reference type is a subtype of anyref.
      SUBTYPE(ref_type, kWasmAnyRef);
      // Only anyref is a subtype of anyref.
      SUBTYPE_IFF(kWasmAnyRef, ref_type, ref_type == kWasmAnyRef);
    }

    // The rest of ref. types are unrelated.
    for (ValueType type_1 : {kWasmFuncRef, kWasmI31Ref, kWasmArrayRef}) {
      for (ValueType type_2 : {kWasmFuncRef, kWasmI31Ref, kWasmArrayRef}) {
        SUBTYPE_IFF(type_1, type_2, type_1 == type_2);
      }
    }

    // Unrelated refs are unrelated.
    NOT_VALID_SUBTYPE(ref(0), ref(2));
    NOT_VALID_SUBTYPE(optRef(3), optRef(1));
    // ref is a subtype of optref for the same struct/array.
    VALID_SUBTYPE(ref(0), optRef(0));
    VALID_SUBTYPE(ref(2), optRef(2));
    // optref is not a subtype of ref for the same struct/array.
    NOT_SUBTYPE(optRef(0), ref(0));
    NOT_SUBTYPE(optRef(2), ref(2));
    // ref is a subtype of optref if the same is true for the underlying
    // structs/arrays.
    VALID_SUBTYPE(ref(3), optRef(2));
    // Prefix subtyping for structs.
    VALID_SUBTYPE(optRef(4), optRef(0));
    // Mutable fields are invariant.
    NOT_VALID_SUBTYPE(ref(0), ref(5));
    // Immutable fields are covariant.
    VALID_SUBTYPE(ref(1), ref(0));
    // Prefix subtyping + immutable field covariance for structs.
    VALID_SUBTYPE(optRef(4), optRef(1));
    // No subtyping between mutable/immutable fields.
    NOT_VALID_SUBTYPE(ref(7), ref(6));
    NOT_VALID_SUBTYPE(ref(6), ref(7));
    // Recursive types.
    VALID_SUBTYPE(ref(9), ref(8));

    // Identical rtts are subtypes of each other.
    SUBTYPE(ValueType::Rtt(5), ValueType::Rtt(5));
    // Rtts of unrelated types are unrelated.
    NOT_SUBTYPE(ValueType::Rtt(1), ValueType::Rtt(2));
    // Rtts of subtypes are not related.
    NOT_SUBTYPE(ValueType::Rtt(1), ValueType::Rtt(0));

    // Function subtyping;
    // Unrelated function types are unrelated.
    NOT_VALID_SUBTYPE(ref(10), ref(11));
    // Function type with different parameter counts are unrelated.
    NOT_VALID_SUBTYPE(ref(12), ref(11));
    // Parameter contravariance holds.
    VALID_SUBTYPE(ref(14), ref(13));
    // Return type covariance holds.
    VALID_SUBTYPE(ref(15), ref(16));
    // Identical types are subtype-related.
    VALID_SUBTYPE(ref(10), ref(10));
    VALID_SUBTYPE(ref(11), ref(11));

    {
      // Canonicalization tests.
      FLAG_SCOPE(wasm_type_canonicalization);

      // Groups should only be canonicalized to identical groups.
      IDENTICAL(18, 22);
      IDENTICAL(19, 23);
      IDENTICAL(20, 24);
      IDENTICAL(21, 25);

      DISTINCT(18, 26);
      DISTINCT(19, 27);
      DISTINCT(20, 28);
      DISTINCT(21, 29);

      // A type should not be canonicalized to an identical one with a different
      // group structure.
      DISTINCT(18, 17);

      // A subtype should also be subtype of an equivalent type.
      VALID_SUBTYPE(ref(30), ref(18));
      VALID_SUBTYPE(ref(30), ref(22));
      NOT_SUBTYPE(ref(30), ref(26));

      // Rtts of identical types are subtype-related.
      SUBTYPE(ValueType::Rtt(8), ValueType::Rtt(17));
    }
  }
#undef SUBTYPE
#undef NOT_SUBTYPE
#undef SUBTYPE_IFF
#undef VALID_SUBTYPE
#undef NOT_VALID_SUBTYPE
#undef IDENTICAL
#undef DISTINCT
}

}  // namespace subtyping_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
