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

constexpr ValueType ref(uint32_t index) { return ValueType::Ref(index); }
constexpr ValueType refNull(uint32_t index) {
  return ValueType::RefNull(index);
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
    /*  0 */ DefineStruct(module, {mut(ref(2)), immut(refNull(2))});
    /*  1 */ DefineStruct(module, {mut(ref(2)), immut(ref(2))}, 0);
    /*  2 */ DefineArray(module, immut(ref(0)));
    /*  3 */ DefineArray(module, immut(ref(1)), 2);
    /*  4 */ DefineStruct(module, {mut(ref(2)), immut(ref(3)), immut(kWasmF64)},
                          1);
    /*  5 */ DefineStruct(module, {mut(refNull(2)), immut(ref(2))});
    /*  6 */ DefineArray(module, mut(kWasmI32));
    /*  7 */ DefineArray(module, immut(kWasmI32));
    /*  8 */ DefineStruct(module, {mut(kWasmI32), immut(refNull(8))});
    /*  9 */ DefineStruct(module, {mut(kWasmI32), immut(refNull(8))}, 8);
    /* 10 */ DefineSignature(module, {}, {});
    /* 11 */ DefineSignature(module, {kWasmI32}, {kWasmI32});
    /* 12 */ DefineSignature(module, {kWasmI32, kWasmI32}, {kWasmI32});
    /* 13 */ DefineSignature(module, {ref(1)}, {kWasmI32});
    /* 14 */ DefineSignature(module, {ref(0)}, {kWasmI32}, 13);
    /* 15 */ DefineSignature(module, {ref(0)}, {ref(4)}, 16);
    /* 16 */ DefineSignature(module, {ref(0)}, {ref(0)});
    /* 17 */ DefineStruct(module, {mut(kWasmI32), immut(refNull(17))});

    // Rec. group.
    /* 18 */ DefineStruct(module, {mut(kWasmI32), immut(refNull(17))}, 17,
                          false);
    /* 19 */ DefineArray(module, {mut(refNull(21))}, kNoSuperType, false);
    /* 20 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false);
    /* 21 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, 20, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    // Identical rec. group.
    /* 22 */ DefineStruct(module, {mut(kWasmI32), immut(refNull(17))}, 17,
                          false);
    /* 23 */ DefineArray(module, {mut(refNull(25))}, kNoSuperType, false);
    /* 24 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false);
    /* 25 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, 24, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    // Nonidentical rec. group: the last function extends a type outside the
    // recursive group.
    /* 26 */ DefineStruct(module, {mut(kWasmI32), immut(refNull(17))}, 17,
                          false);
    /* 27 */ DefineArray(module, {mut(refNull(29))}, kNoSuperType, false);
    /* 28 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false);
    /* 29 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, 20, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    /* 30 */ DefineStruct(module, {mut(kWasmI32), immut(refNull(18))}, 18);
    /* 31 */ DefineStruct(
        module, {mut(ref(2)), immut(refNull(2)), immut(kWasmS128)}, 1);
  }

  constexpr ValueType numeric_types[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                                         kWasmS128};
  constexpr ValueType ref_types[] = {
      kWasmFuncRef,   kWasmEqRef,           // --
      kWasmDataRef,   kWasmArrayRef,        // --
      kWasmI31Ref,    kWasmAnyRef,          // --
      kWasmExternRef, kWasmNullExternRef,   // --
      kWasmNullRef,   kWasmNullFuncRef,     // --
      kWasmStringRef, kWasmStringViewIter,  // --
      refNull(0),     ref(0),               // struct
      refNull(2),     ref(2),               // array
      refNull(11),    ref(11)               // signature
  };

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
#define IDENTICAL(index1, index2)                         \
  EXPECT_TRUE(EquivalentTypes(ValueType::RefNull(index1), \
                              ValueType::RefNull(index2), module1, module));
#define DISTINCT(index1, index2)                           \
  EXPECT_FALSE(EquivalentTypes(ValueType::RefNull(index1), \
                               ValueType::RefNull(index2), module1, module));
// Union always expresses the result in terms of module1.
#define UNION(type1, type2, type_result)          \
  EXPECT_EQ(Union(type1, type2, module1, module), \
            TypeInModule(type_result, module1))
// Intersection might return either module, so we have a version which checks
// the module and one which deos not.
#define INTERSECTION(type1, type2, type_result) \
  EXPECT_EQ(Intersection(type1, type2, module1, module).type, type_result)
#define INTERSECTION_M(type1, type2, type_result, module_result) \
  EXPECT_EQ(Intersection(type1, type2, module1, module),         \
            TypeInModule(type_result, module_result))

  for (WasmModule* module : {module1, module2}) {
    // Type judgements across modules should work the same as within one module.

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
      const bool is_extern =
          ref_type == kWasmExternRef || ref_type == kWasmNullExternRef;
      const bool is_any_func = ref_type == kWasmFuncRef ||
                               ref_type == kWasmNullFuncRef ||
                               ref_type == refNull(11) || ref_type == ref(11);
      const bool is_string_view = ref_type == kWasmStringViewIter ||
                                  ref_type == kWasmStringViewWtf8 ||
                                  ref_type == kWasmStringViewWtf16;
      SCOPED_TRACE("ref_type: " + ref_type.name());
      // Concrete reference types, i31ref and dataref are subtypes of eqref,
      // externref/funcref/anyref/functions are not.
      SUBTYPE_IFF(ref_type, kWasmEqRef,
                  ref_type != kWasmAnyRef && !is_any_func && !is_extern &&
                      !is_string_view && ref_type != kWasmStringRef);
      // Struct/array types are subtypes of dataref.
      SUBTYPE_IFF(ref_type, kWasmDataRef,
                  ref_type == kWasmDataRef || ref_type == kWasmArrayRef ||
                      ref_type == kWasmNullRef || ref_type == ref(0) ||
                      ref_type == ref(2) || ref_type == refNull(0) ||
                      ref_type == refNull(2));
      // Array types are subtypes of arrayref.
      SUBTYPE_IFF(ref_type, kWasmArrayRef,
                  ref_type == kWasmArrayRef || ref_type == ref(2) ||
                      ref_type == kWasmNullRef || ref_type == refNull(2));
      // Functions are subtypes of funcref.
      SUBTYPE_IFF(ref_type, kWasmFuncRef, is_any_func);
      // Each reference type is a subtype of itself.
      SUBTYPE(ref_type, ref_type);
      // Each non-func, non-extern, non-string-view, non-string-iter reference
      // type is a subtype of anyref.
      SUBTYPE_IFF(ref_type, kWasmAnyRef,
                  !is_any_func && !is_extern && !is_string_view);
      // Only anyref is a subtype of anyref.
      SUBTYPE_IFF(kWasmAnyRef, ref_type, ref_type == kWasmAnyRef);
      // Only externref and nullexternref are subtypes of externref.
      SUBTYPE_IFF(ref_type, kWasmExternRef, is_extern);
      // Only nullexternref is a subtype of nullexternref.
      SUBTYPE_IFF(ref_type, kWasmNullExternRef, ref_type == kWasmNullExternRef);
      // Each nullable non-func, non-extern reference type is a supertype of
      // nullref.
      SUBTYPE_IFF(kWasmNullRef, ref_type,
                  ref_type.is_nullable() && !is_any_func && !is_extern);
      // Only nullref is a subtype of nullref.
      SUBTYPE_IFF(ref_type, kWasmNullRef, ref_type == kWasmNullRef);
      // Only nullable funcs are supertypes of nofunc.
      SUBTYPE_IFF(kWasmNullFuncRef, ref_type,
                  ref_type.is_nullable() && is_any_func);
      // Only nullfuncref is a subtype of nullfuncref.
      SUBTYPE_IFF(ref_type, kWasmNullFuncRef, ref_type == kWasmNullFuncRef);

      // Make sure symmetric relations are symmetric.
      for (ValueType ref_type2 : ref_types) {
        if (ref_type == ref_type2) {
          EXPECT_TRUE(EquivalentTypes(ref_type, ref_type2, module, module1));
          EXPECT_TRUE(EquivalentTypes(ref_type2, ref_type, module1, module));
        } else {
          EXPECT_FALSE(EquivalentTypes(ref_type, ref_type2, module, module1));
          EXPECT_FALSE(EquivalentTypes(ref_type2, ref_type, module1, module));
        }
      }
    }

    // The rest of ref. types are unrelated.
    for (ValueType type_1 : {kWasmFuncRef, kWasmI31Ref, kWasmArrayRef}) {
      for (ValueType type_2 : {kWasmFuncRef, kWasmI31Ref, kWasmArrayRef}) {
        SUBTYPE_IFF(type_1, type_2, type_1 == type_2);
      }
    }

    // Unrelated refs are unrelated.
    NOT_VALID_SUBTYPE(ref(0), ref(2));
    NOT_VALID_SUBTYPE(refNull(3), refNull(1));
    // ref is a subtype of ref null for the same struct/array.
    VALID_SUBTYPE(ref(0), refNull(0));
    VALID_SUBTYPE(ref(2), refNull(2));
    // ref null is not a subtype of ref for the same struct/array.
    NOT_SUBTYPE(refNull(0), ref(0));
    NOT_SUBTYPE(refNull(2), ref(2));
    // ref is a subtype of ref null if the same is true for the underlying
    // structs/arrays.
    VALID_SUBTYPE(ref(3), refNull(2));
    // Prefix subtyping for structs.
    VALID_SUBTYPE(refNull(4), refNull(0));
    // Mutable fields are invariant.
    NOT_VALID_SUBTYPE(ref(0), ref(5));
    // Immutable fields are covariant.
    VALID_SUBTYPE(ref(1), ref(0));
    // Prefix subtyping + immutable field covariance for structs.
    VALID_SUBTYPE(refNull(4), refNull(1));
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

    // Unions and intersections.

    // Distinct numeric types are unrelated.
    for (ValueType type1 : numeric_types) {
      for (ValueType type2 : numeric_types) {
        UNION(type1, type2, (type1 == type2 ? type1 : kWasmBottom));
        INTERSECTION(type1, type2, (type1 == type2 ? type1 : kWasmBottom));
      }
    }
    // Numeric and reference types are unrelated.
    for (ValueType type1 : numeric_types) {
      for (ValueType type2 : ref_types) {
        UNION(type1, type2, kWasmBottom);
        INTERSECTION(type1, type2, kWasmBottom);
      }
    }

    // Reference type vs. itself and anyref.
    for (ValueType type : ref_types) {
      SCOPED_TRACE(type.name());
      UNION(type, type, type);
      INTERSECTION(type, type, type);
      if (type == kWasmStringViewIter || type == kWasmStringViewWtf8 ||
          type == kWasmStringViewWtf16) {
        // String view and string iter aren't subtypes of any but have the same
        // null sentinel nullref (ref null none).
        INTERSECTION(type, kWasmAnyRef, kWasmNullRef);
        continue;
      }
      if (type == kWasmFuncRef || type == kWasmNullFuncRef || type == ref(11) ||
          type == refNull(11) || type == kWasmExternRef ||
          type == kWasmNullExternRef) {
        // func and extern types don't share the same type hierarchy as anyref.
        INTERSECTION(type, kWasmAnyRef, kWasmBottom);
        continue;
      }
      UNION(kWasmAnyRef, type, kWasmAnyRef);
      INTERSECTION(kWasmAnyRef, type, type);
      UNION(kWasmAnyRef.AsNonNull(), type,
            type.is_nullable() ? kWasmAnyRef : kWasmAnyRef.AsNonNull());
      INTERSECTION(kWasmAnyRef.AsNonNull(), type,
                   type != kWasmNullRef ? type.AsNonNull() : kWasmBottom);
    }

    // Abstract types vs abstract types.
    UNION(kWasmEqRef, kWasmDataRef, kWasmEqRef);
    UNION(kWasmEqRef, kWasmI31Ref, kWasmEqRef);
    UNION(kWasmEqRef, kWasmArrayRef, kWasmEqRef);
    UNION(kWasmEqRef, kWasmNullRef, kWasmEqRef);
    UNION(kWasmDataRef, kWasmI31Ref, kWasmEqRef);
    UNION(kWasmDataRef, kWasmArrayRef, kWasmDataRef);
    UNION(kWasmDataRef, kWasmNullRef, kWasmDataRef.AsNullable());
    UNION(kWasmI31Ref.AsNonNull(), kWasmArrayRef.AsNonNull(),
          kWasmEqRef.AsNonNull());
    UNION(kWasmI31Ref, kWasmNullRef, kWasmI31Ref.AsNullable());
    UNION(kWasmArrayRef, kWasmNullRef, kWasmArrayRef.AsNullable());
    UNION(kWasmDataRef.AsNonNull(), kWasmI31Ref.AsNonNull(),
          kWasmEqRef.AsNonNull());
    UNION(kWasmDataRef, kWasmArrayRef, kWasmDataRef);
    UNION(kWasmI31Ref.AsNonNull(), kWasmArrayRef, kWasmEqRef);
    UNION(kWasmAnyRef, kWasmNullRef, kWasmAnyRef);
    UNION(kWasmExternRef, kWasmNullExternRef, kWasmExternRef);
    UNION(kWasmFuncRef, kWasmNullFuncRef, kWasmFuncRef);

    INTERSECTION(kWasmExternRef, kWasmEqRef, kWasmBottom);
    INTERSECTION(kWasmExternRef, kWasmDataRef, kWasmBottom);
    INTERSECTION(kWasmExternRef, kWasmI31Ref.AsNonNull(), kWasmBottom);
    INTERSECTION(kWasmExternRef, kWasmArrayRef, kWasmBottom);
    INTERSECTION(kWasmExternRef, kWasmNullRef, kWasmBottom);
    INTERSECTION(kWasmExternRef, kWasmFuncRef, kWasmBottom);
    INTERSECTION(kWasmNullExternRef, kWasmEqRef, kWasmBottom);
    INTERSECTION(kWasmNullExternRef, kWasmDataRef, kWasmBottom);
    INTERSECTION(kWasmNullExternRef, kWasmI31Ref, kWasmBottom);
    INTERSECTION(kWasmNullExternRef, kWasmArrayRef, kWasmBottom);
    INTERSECTION(kWasmNullExternRef, kWasmNullRef, kWasmBottom);
    INTERSECTION(kWasmNullExternRef, kWasmExternRef, kWasmNullExternRef);
    INTERSECTION(kWasmNullExternRef, kWasmExternRef.AsNonNull(), kWasmBottom);

    INTERSECTION(kWasmFuncRef, kWasmEqRef, kWasmBottom);
    INTERSECTION(kWasmFuncRef, kWasmDataRef, kWasmBottom);
    INTERSECTION(kWasmFuncRef, kWasmI31Ref.AsNonNull(), kWasmBottom);
    INTERSECTION(kWasmFuncRef, kWasmArrayRef, kWasmBottom);
    INTERSECTION(kWasmFuncRef, kWasmNullRef, kWasmBottom);
    INTERSECTION(kWasmFuncRef, kWasmNullExternRef, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, kWasmEqRef, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, kWasmDataRef, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, kWasmI31Ref, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, kWasmArrayRef, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, kWasmNullRef, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, kWasmFuncRef, kWasmNullFuncRef);
    INTERSECTION(kWasmNullFuncRef, kWasmFuncRef.AsNonNull(), kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, kWasmNullExternRef, kWasmBottom);

    INTERSECTION(kWasmEqRef, kWasmDataRef, kWasmDataRef);
    INTERSECTION(kWasmEqRef, kWasmI31Ref, kWasmI31Ref);
    INTERSECTION(kWasmEqRef, kWasmArrayRef, kWasmArrayRef);
    INTERSECTION(kWasmEqRef, kWasmNullRef, kWasmNullRef);
    INTERSECTION(kWasmEqRef, kWasmFuncRef, kWasmBottom);
    INTERSECTION(kWasmDataRef, kWasmI31Ref, kWasmNullRef);
    INTERSECTION(kWasmDataRef, kWasmArrayRef, kWasmArrayRef);
    INTERSECTION(kWasmDataRef, kWasmNullRef, kWasmNullRef);
    INTERSECTION(kWasmI31Ref, kWasmArrayRef, kWasmNullRef);
    INTERSECTION(kWasmI31Ref.AsNonNull(), kWasmNullRef, kWasmBottom);
    INTERSECTION(kWasmArrayRef.AsNonNull(), kWasmNullRef, kWasmBottom);

    ValueType struct_type = ref(0);
    ValueType array_type = ref(2);
    ValueType function_type = ref(11);

    // Abstract vs indexed types.
    UNION(kWasmFuncRef, function_type, kWasmFuncRef);
    INTERSECTION(kWasmFuncRef, struct_type, kWasmBottom);
    INTERSECTION(kWasmFuncRef, array_type, kWasmBottom);
    INTERSECTION(kWasmFuncRef, function_type, function_type);

    UNION(kWasmNullFuncRef, function_type, function_type.AsNullable());
    INTERSECTION(kWasmNullFuncRef, struct_type, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, struct_type.AsNullable(), kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, array_type, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, array_type.AsNullable(), kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, function_type, kWasmBottom);
    INTERSECTION(kWasmNullFuncRef, function_type.AsNullable(),
                 kWasmNullFuncRef);

    UNION(kWasmEqRef, struct_type, kWasmEqRef);
    UNION(kWasmEqRef, array_type, kWasmEqRef);
    INTERSECTION(kWasmEqRef, struct_type, struct_type);
    INTERSECTION(kWasmEqRef, array_type, array_type);
    INTERSECTION(kWasmEqRef, function_type, kWasmBottom);

    UNION(kWasmDataRef, struct_type, kWasmDataRef);
    UNION(kWasmDataRef, array_type, kWasmDataRef);
    INTERSECTION(kWasmDataRef, struct_type, struct_type);
    INTERSECTION(kWasmDataRef, array_type, array_type);
    INTERSECTION(kWasmDataRef, function_type, kWasmBottom);

    UNION(kWasmI31Ref, struct_type, kWasmEqRef);
    UNION(kWasmI31Ref, array_type, kWasmEqRef);
    INTERSECTION(kWasmI31Ref, struct_type, kWasmBottom);
    INTERSECTION(kWasmI31Ref, array_type, kWasmBottom);
    INTERSECTION(kWasmI31Ref, function_type, kWasmBottom);

    UNION(kWasmArrayRef, struct_type, kWasmDataRef);
    UNION(kWasmArrayRef, array_type, kWasmArrayRef);
    INTERSECTION(kWasmArrayRef, struct_type, kWasmBottom);
    INTERSECTION(kWasmArrayRef, array_type, array_type);
    INTERSECTION(kWasmArrayRef, function_type, kWasmBottom);

    UNION(kWasmNullRef, struct_type, struct_type.AsNullable());
    UNION(kWasmNullRef, array_type, array_type.AsNullable());
    UNION(kWasmNullRef, function_type, function_type.AsNullable());
    INTERSECTION(kWasmNullRef, struct_type, kWasmBottom);
    INTERSECTION(kWasmNullRef, array_type, kWasmBottom);
    INTERSECTION(kWasmNullRef, function_type, kWasmBottom);

    // Indexed types of different kinds.
    UNION(struct_type, array_type, kWasmDataRef.AsNonNull());
    INTERSECTION(struct_type, array_type, kWasmBottom);
    INTERSECTION(struct_type, function_type, kWasmBottom);
    INTERSECTION(array_type, function_type, kWasmBottom);

    // Nullable vs. non-nullable.
    UNION(struct_type, struct_type.AsNullable(), struct_type.AsNullable());
    INTERSECTION(struct_type, struct_type.AsNullable(), struct_type);
    UNION(kWasmDataRef, kWasmDataRef.AsNullable(), kWasmDataRef.AsNullable());
    INTERSECTION(kWasmDataRef, kWasmDataRef.AsNullable(), kWasmDataRef);

    // Concrete types of the same kind.
    // Subtyping relation.
    UNION(refNull(4), ref(1), refNull(1));
    INTERSECTION_M(refNull(4), ref(1), ref(4), module1);
    INTERSECTION_M(refNull(1), refNull(4), refNull(4), module);
    // Common ancestor.
    UNION(ref(4), ref(31), ref(1));
    INTERSECTION(ref(4), ref(31), kWasmBottom);
    // No common ancestor.
    UNION(ref(6), refNull(2), kWasmArrayRef.AsNullable());
    INTERSECTION(ref(6), refNull(2), kWasmBottom);
    UNION(ref(0), ref(17), kWasmDataRef.AsNonNull());
    INTERSECTION(ref(0), ref(17), kWasmBottom);
    UNION(ref(10), refNull(11), kWasmFuncRef);
    INTERSECTION(ref(10), refNull(11), kWasmBottom);
  }
#undef SUBTYPE
#undef NOT_SUBTYPE
#undef SUBTYPE_IFF
#undef VALID_SUBTYPE
#undef NOT_VALID_SUBTYPE
#undef IDENTICAL
#undef DISTINCT
#undef UNION
#undef INTERSECTION
#undef INTERSECTION_M
}

}  // namespace subtyping_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
