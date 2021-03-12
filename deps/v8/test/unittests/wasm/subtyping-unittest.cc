// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-subtyping.h"
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

TEST_F(WasmSubtypingTest, Subtyping) {
  v8::internal::AccountingAllocator allocator;
  WasmModule module1_(std::make_unique<Zone>(&allocator, ZONE_NAME));
  WasmModule module2_(std::make_unique<Zone>(&allocator, ZONE_NAME));

  WasmModule* module1 = &module1_;
  WasmModule* module2 = &module2_;

  // Set up two identical modules.
  for (WasmModule* module : {module1, module2}) {
    /* 0 */ DefineStruct(module, {mut(ref(2)), immut(optRef(2))});
    /* 1 */ DefineStruct(module, {mut(ref(2)), immut(ref(2))});
    /* 2 */ DefineArray(module, immut(ref(0)));
    /* 3 */ DefineArray(module, immut(ref(1)));
    /* 4 */ DefineStruct(module, {mut(ref(2)), immut(ref(3)), immut(kWasmF64)});
    /* 5 */ DefineStruct(module, {mut(optRef(2)), immut(ref(2))});
    /* 6 */ DefineArray(module, mut(kWasmI32));
    /* 7 */ DefineArray(module, immut(kWasmI32));
    /* 8 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(8))});
    /* 9 */ DefineStruct(module, {mut(kWasmI32), immut(optRef(8))});
  }

  ValueType numeric_types[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                               kWasmS128};
  ValueType ref_types[] = {
      kWasmExternRef, kWasmFuncRef, kWasmEqRef, kWasmI31Ref, kWasmDataRef,
      kWasmAnyRef,    optRef(0),    ref(0),     optRef(2),   ref(2)};

  // Type judgements across modules should work the same as within one module.
  for (WasmModule* module : {module1, module2}) {
    // Value types are unrelated, except if they are equal.
    for (ValueType subtype : numeric_types) {
      for (ValueType supertype : numeric_types) {
        EXPECT_EQ(IsSubtypeOf(subtype, supertype, module1, module),
                  subtype == supertype);
      }
    }

    // Value types are unrelated with reference types.
    for (ValueType value_type : numeric_types) {
      for (ValueType ref_type : ref_types) {
        EXPECT_TRUE(!IsSubtypeOf(value_type, ref_type, module1, module));
        EXPECT_TRUE(!IsSubtypeOf(ref_type, value_type, module1, module));
      }
    }

    for (ValueType ref_type : ref_types) {
      // Concrete reference types, i31ref and dataref are subtypes of eqref,
      // externref/funcref/anyref are not.
      EXPECT_EQ(IsSubtypeOf(ref_type, kWasmEqRef, module1, module),
                ref_type != kWasmFuncRef && ref_type != kWasmExternRef &&
                    ref_type != kWasmAnyRef);
      // Non-nullable struct/array types are subtypes of dataref.
      EXPECT_EQ(IsSubtypeOf(ref_type, kWasmDataRef, module1, module),
                ref_type == kWasmDataRef ||
                    (ref_type.kind() == kRef && ref_type.has_index()));
      // Each reference type is a subtype of itself.
      EXPECT_TRUE(IsSubtypeOf(ref_type, ref_type, module1, module));
      // Each reference type is a subtype of anyref.
      EXPECT_TRUE(IsSubtypeOf(ref_type, kWasmAnyRef, module1, module));
      // Only anyref is a subtype of anyref.
      EXPECT_EQ(IsSubtypeOf(kWasmAnyRef, ref_type, module1, module),
                ref_type == kWasmAnyRef);
    }

    // The rest of ref. types are unrelated.
    for (ValueType type_1 : {kWasmExternRef, kWasmFuncRef, kWasmI31Ref}) {
      for (ValueType type_2 : {kWasmExternRef, kWasmFuncRef, kWasmI31Ref}) {
        EXPECT_EQ(IsSubtypeOf(type_1, type_2, module1, module),
                  type_1 == type_2);
      }
    }

    // Unrelated refs are unrelated.
    EXPECT_TRUE(!IsSubtypeOf(ref(0), ref(2), module1, module));
    EXPECT_TRUE(!IsSubtypeOf(optRef(3), optRef(1), module1, module));
    // ref is a subtype of optref for the same struct/array.
    EXPECT_TRUE(IsSubtypeOf(ref(0), optRef(0), module1, module));
    EXPECT_TRUE(IsSubtypeOf(ref(2), optRef(2), module1, module));
    // optref is not a subtype of ref for the same struct/array.
    EXPECT_TRUE(!IsSubtypeOf(optRef(0), ref(0), module1, module));
    EXPECT_TRUE(!IsSubtypeOf(optRef(2), ref(2), module1, module));
    // ref is a subtype of optref if the same is true for the underlying
    // structs/arrays.
    EXPECT_TRUE(IsSubtypeOf(ref(3), optRef(2), module1, module));
    // Prefix subtyping for structs.
    EXPECT_TRUE(IsSubtypeOf(optRef(4), optRef(0), module1, module));
    // Mutable fields are invariant.
    EXPECT_TRUE(!IsSubtypeOf(ref(0), ref(5), module1, module));
    // Immutable fields are covariant.
    EXPECT_TRUE(IsSubtypeOf(ref(1), ref(0), module1, module));
    // Prefix subtyping + immutable field covariance for structs.
    EXPECT_TRUE(IsSubtypeOf(optRef(4), optRef(1), module1, module));
    // No subtyping between mutable/immutable fields.
    EXPECT_TRUE(!IsSubtypeOf(ref(7), ref(6), module1, module));
    EXPECT_TRUE(!IsSubtypeOf(ref(6), ref(7), module1, module));
    // Recursive types.
    EXPECT_TRUE(IsSubtypeOf(ref(9), ref(8), module1, module));

    // Identical rtts are subtypes of each other.
    EXPECT_TRUE(IsSubtypeOf(ValueType::Rtt(5, 3), ValueType::Rtt(5, 3), module1,
                            module2));
    EXPECT_TRUE(
        IsSubtypeOf(ValueType::Rtt(5), ValueType::Rtt(5), module1, module2));
    // Rtts of unrelated types are unrelated.
    EXPECT_TRUE(!IsSubtypeOf(ValueType::Rtt(1, 1), ValueType::Rtt(2, 1),
                             module1, module2));
    EXPECT_TRUE(
        !IsSubtypeOf(ValueType::Rtt(1), ValueType::Rtt(2), module1, module2));
    EXPECT_TRUE(!IsSubtypeOf(ValueType::Rtt(1, 0), ValueType::Rtt(2), module1,
                             module2));
    // Rtts of different depth are unrelated.
    EXPECT_TRUE(!IsSubtypeOf(ValueType::Rtt(5, 1), ValueType::Rtt(5, 3),
                             module1, module2));
    EXPECT_TRUE(!IsSubtypeOf(ValueType::Rtt(5, 8), ValueType::Rtt(5, 3),
                             module1, module2));
    // Rtts of identical types are subtype-related.
    EXPECT_TRUE(IsSubtypeOf(ValueType::Rtt(8, 1), ValueType::Rtt(9, 1), module1,
                            module));
    EXPECT_TRUE(
        IsSubtypeOf(ValueType::Rtt(8), ValueType::Rtt(9), module1, module));
    // Rtts of subtypes are not related.
    EXPECT_TRUE(!IsSubtypeOf(ValueType::Rtt(1, 1), ValueType::Rtt(0, 1),
                             module1, module));
    EXPECT_TRUE(
        !IsSubtypeOf(ValueType::Rtt(1), ValueType::Rtt(0), module1, module));
    // rtt(t, d) <: rtt(t)
    for (uint8_t depth : {0, 1, 5}) {
      EXPECT_TRUE(IsSubtypeOf(ValueType::Rtt(1, depth), ValueType::Rtt(1),
                              module1, module));
    }
  }
}

}  // namespace subtyping_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
