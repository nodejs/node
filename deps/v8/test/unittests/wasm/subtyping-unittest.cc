// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-subtyping.h"
#include "test/common/wasm/flag-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace subtyping_unittest {

class WasmSubtypingTest : public ::testing::Test {};
using FieldInit = std::pair<ValueType, bool>;

ValueType ref(uint32_t index) { return ValueType::Ref(index, kNonNullable); }
ValueType optRef(uint32_t index) { return ValueType::Ref(index, kNullable); }

FieldInit mut(ValueType type) { return FieldInit(type, true); }
FieldInit immut(ValueType type) { return FieldInit(type, false); }

void DefineStruct(WasmModule* module, std::initializer_list<FieldInit> fields) {
  StructType::Builder builder(module->signature_zone.get(),
                              static_cast<uint32_t>(fields.size()));
  for (FieldInit field : fields) {
    builder.AddField(field.first, field.second);
  }
  return module->add_struct_type(builder.Build());
}

void DefineArray(WasmModule* module, FieldInit element_type) {
  module->add_array_type(module->signature_zone->New<ArrayType>(
      element_type.first, element_type.second));
}

void DefineSignature(WasmModule* module,
                     std::initializer_list<ValueType> params,
                     std::initializer_list<ValueType> returns) {
  module->add_signature(
      FunctionSig::Build(module->signature_zone.get(), returns, params));
}

TEST_F(WasmSubtypingTest, Subtyping) {
  v8::internal::AccountingAllocator allocator;
  WasmModule module1_(std::make_unique<Zone>(&allocator, ZONE_NAME));
  WasmModule module2_(std::make_unique<Zone>(&allocator, ZONE_NAME));

  WasmModule* module1 = &module1_;
  WasmModule* module2 = &module2_;

  // Set up two identical modules.
  for (WasmModule* module : {module1, module2}) {
    /*  0 */ DefineStruct(module, {mut(ref(2)), immut(optRef(2))});
    /*  1 */ DefineStruct(module, {mut(ref(2)), immut(ref(2))});
    /*  2 */ DefineArray(module, immut(ref(0)));
    /*  3 */ DefineArray(module, immut(ref(1)));
    /*  4 */ DefineStruct(module,
                          {mut(ref(2)), immut(ref(3)), immut(kWasmF64)});
    /*  5 */ DefineStruct(module, {mut(optRef(2)), immut(ref(2))});
    /*  6 */ DefineArray(module, mut(kWasmI32));
    /*  7 */ DefineArray(module, immut(kWasmI32));
    /*  8 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(8))});
    /*  9 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(8))});
    /* 10 */ DefineSignature(module, {}, {});
    /* 11 */ DefineSignature(module, {kWasmI32}, {kWasmI32});
    /* 12 */ DefineSignature(module, {kWasmI32, kWasmI32}, {kWasmI32});
    /* 13 */ DefineSignature(module, {ref(1)}, {kWasmI32});
    /* 14 */ DefineSignature(module, {ref(0)}, {kWasmI32});
    /* 15 */ DefineSignature(module, {ref(0)}, {ref(4)});
    /* 16 */ DefineSignature(module, {ref(0)}, {ref(0)});
  }

  ValueType numeric_types[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                               kWasmS128};
  ValueType ref_types[] = {kWasmExternRef, kWasmFuncRef, kWasmEqRef,
                           kWasmI31Ref,    kWasmDataRef, kWasmAnyRef,
                           optRef(0),      ref(0),       optRef(2),
                           ref(2),         optRef(11),   ref(11)};

#define SUBTYPE(type1, type2) \
  EXPECT_TRUE(IsSubtypeOf(type1, type2, module1, module))
#define NOT_SUBTYPE(type1, type2) \
  EXPECT_FALSE(IsSubtypeOf(type1, type2, module1, module))
#define SUBTYPE_IFF(type1, type2, condition) \
  EXPECT_EQ(IsSubtypeOf(type1, type2, module1, module), condition)

  // Type judgements across modules should work the same as within one module.
  for (WasmModule* module : {module1, module2}) {
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
                  ref_type != kWasmFuncRef && ref_type != kWasmExternRef &&
                      ref_type != kWasmAnyRef && ref_type != optRef(11) &&
                      ref_type != ref(11));
      // Non-nullable struct/array types are subtypes of dataref.
      SUBTYPE_IFF(
          ref_type, kWasmDataRef,
          ref_type == kWasmDataRef || ref_type == ref(0) || ref_type == ref(2));
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
    for (ValueType type_1 : {kWasmExternRef, kWasmFuncRef, kWasmI31Ref}) {
      for (ValueType type_2 : {kWasmExternRef, kWasmFuncRef, kWasmI31Ref}) {
        SUBTYPE_IFF(type_1, type_2, type_1 == type_2);
      }
    }

    // Unrelated refs are unrelated.
    NOT_SUBTYPE(ref(0), ref(2));
    NOT_SUBTYPE(optRef(3), optRef(1));
    // ref is a subtype of optref for the same struct/array.
    SUBTYPE(ref(0), optRef(0));
    SUBTYPE(ref(2), optRef(2));
    // optref is not a subtype of ref for the same struct/array.
    NOT_SUBTYPE(optRef(0), ref(0));
    NOT_SUBTYPE(optRef(2), ref(2));
    // ref is a subtype of optref if the same is true for the underlying
    // structs/arrays.
    SUBTYPE(ref(3), optRef(2));
    // Prefix subtyping for structs.
    SUBTYPE(optRef(4), optRef(0));
    // Mutable fields are invariant.
    NOT_SUBTYPE(ref(0), ref(5));
    // Immutable fields are covariant.
    SUBTYPE(ref(1), ref(0));
    // Prefix subtyping + immutable field covariance for structs.
    SUBTYPE(optRef(4), optRef(1));
    // No subtyping between mutable/immutable fields.
    NOT_SUBTYPE(ref(7), ref(6));
    NOT_SUBTYPE(ref(6), ref(7));
    // Recursive types.
    SUBTYPE(ref(9), ref(8));

    // Identical rtts are subtypes of each other.
    SUBTYPE(ValueType::Rtt(5, 3), ValueType::Rtt(5, 3));
    SUBTYPE(ValueType::Rtt(5), ValueType::Rtt(5));
    // Rtts of unrelated types are unrelated.
    NOT_SUBTYPE(ValueType::Rtt(1, 1), ValueType::Rtt(2, 1));
    NOT_SUBTYPE(ValueType::Rtt(1), ValueType::Rtt(2));
    NOT_SUBTYPE(ValueType::Rtt(1, 0), ValueType::Rtt(2));
    // Rtts of different depth are unrelated.
    NOT_SUBTYPE(ValueType::Rtt(5, 1), ValueType::Rtt(5, 3));
    NOT_SUBTYPE(ValueType::Rtt(5, 8), ValueType::Rtt(5, 3));
    // Rtts of identical types are subtype-related.
    SUBTYPE(ValueType::Rtt(8, 1), ValueType::Rtt(9, 1));
    SUBTYPE(ValueType::Rtt(8), ValueType::Rtt(9));
    // Rtts of subtypes are not related.
    NOT_SUBTYPE(ValueType::Rtt(1, 1), ValueType::Rtt(0, 1));
    NOT_SUBTYPE(ValueType::Rtt(1), ValueType::Rtt(0));
    // rtt(t, d) <: rtt(t)
    for (uint8_t depth : {0, 1, 5}) {
      SUBTYPE(ValueType::Rtt(1, depth), ValueType::Rtt(1));
    }

    // Function subtyping depends on the selected wasm features.
    // Without wasm-gc:

    // Unrelated function types are unrelated.
    NOT_SUBTYPE(ref(10), ref(11));
    // Function type with different parameter counts are unrelated.
    NOT_SUBTYPE(ref(12), ref(11));
    // Parameter contravariance does not hold.
    NOT_SUBTYPE(ref(14), ref(13));
    // Return type covariance does not hold.
    NOT_SUBTYPE(ref(15), ref(16));
    // Only identical types are subtype-related.
    SUBTYPE(ref(10), ref(10));
    SUBTYPE(ref(11), ref(11));

    {
      // With wasm-gc:
      EXPERIMENTAL_FLAG_SCOPE(gc);
      // Unrelated function types are unrelated.
      NOT_SUBTYPE(ref(10), ref(11));
      // Function type with different parameter counts are unrelated.
      NOT_SUBTYPE(ref(12), ref(11));
      // Parameter contravariance holds.
      SUBTYPE(ref(14), ref(13));
      // Return type covariance holds.
      SUBTYPE(ref(15), ref(16));
      // Identical types are subtype-related.
      SUBTYPE(ref(10), ref(10));
      SUBTYPE(ref(11), ref(11));
    }
  }
#undef SUBTYPE
#undef NOT_SUBTYPE
#undef SUBTYPE_IFF
}

}  // namespace subtyping_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
