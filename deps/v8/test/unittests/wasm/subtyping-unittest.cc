// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/canonical-types.h"
#include "src/wasm/wasm-subtyping.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/flag-utils.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm::subtyping_unittest {

class WasmSubtypingTest : public TestWithPlatform {};
using FieldInit = std::pair<ValueType, bool>;
using Idx = ModuleTypeIndex;
constexpr bool kShared = true;

constexpr ValueType refS(uint32_t index, bool shared = kNotShared) {
  return ValueType::Ref(Idx{index}, shared, RefTypeKind::kStruct);
}
constexpr ValueType refA(uint32_t index, bool shared = kNotShared) {
  return ValueType::Ref(Idx{index}, shared, RefTypeKind::kArray);
}
constexpr ValueType refF(uint32_t index, bool shared = kNotShared) {
  return ValueType::Ref(Idx{index}, shared, RefTypeKind::kFunction);
}
constexpr ValueType refC(uint32_t index, bool shared = kNotShared) {
  return ValueType::Ref(Idx{index}, shared, RefTypeKind::kCont);
}
constexpr ValueType refNullS(uint32_t index, bool shared = kNotShared) {
  return ValueType::RefNull(Idx{index}, shared, RefTypeKind::kStruct);
}
constexpr ValueType refNullA(uint32_t index, bool shared = kNotShared) {
  return ValueType::RefNull(Idx{index}, shared, RefTypeKind::kArray);
}
constexpr ValueType refNullF(uint32_t index, bool shared = kNotShared) {
  return ValueType::RefNull(Idx{index}, shared, RefTypeKind::kFunction);
}
constexpr ValueType refNullC(uint32_t index, bool shared = kNotShared) {
  return ValueType::RefNull(Idx{index}, shared, RefTypeKind::kCont);
}

FieldInit mut(ValueType type) { return FieldInit(type, true); }
FieldInit immut(ValueType type) { return FieldInit(type, false); }

void DefineStruct(WasmModule* module, std::initializer_list<FieldInit> fields,
                  ModuleTypeIndex supertype = kNoSuperType,
                  bool is_final = false, bool is_shared = false,
                  bool in_singleton_rec_group = true) {
  StructType::Builder builder(&module->signature_zone,
                              static_cast<uint32_t>(fields.size()), false,
                              false);
  for (FieldInit field : fields) {
    builder.AddField(field.first, field.second);
  }
  module->AddStructTypeForTesting(builder.Build(), supertype, is_final,
                                  is_shared);
  if (in_singleton_rec_group) {
    GetTypeCanonicalizer()->AddRecursiveSingletonGroup(module);
  }
}

void DefineArray(WasmModule* module, FieldInit element_type,
                 ModuleTypeIndex supertype = kNoSuperType,
                 bool is_final = false, bool is_shared = false,
                 bool in_singleton_rec_group = true) {
  module->AddArrayTypeForTesting(module->signature_zone.New<ArrayType>(
                                     element_type.first, element_type.second),
                                 supertype, is_final, is_shared);
  if (in_singleton_rec_group) {
    GetTypeCanonicalizer()->AddRecursiveSingletonGroup(module);
  }
}

void DefineSignature(WasmModule* module,
                     std::initializer_list<ValueType> params,
                     std::initializer_list<ValueType> returns,
                     ModuleTypeIndex supertype = kNoSuperType,
                     bool is_final = false, bool is_shared = false,
                     bool in_singleton_rec_group = true) {
  module->AddSignatureForTesting(
      FunctionSig::Build(&module->signature_zone, returns, params), supertype,
      is_final, is_shared);
  if (in_singleton_rec_group) {
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 1);
  }
}

void DefineCont(WasmModule* module, ModuleTypeIndex cont,
                ModuleTypeIndex supertype = kNoSuperType, bool is_final = false,
                bool is_shared = false) {
  module->AddContTypeForTesting(module->signature_zone.New<ContType>(cont),
                                supertype, is_final, is_shared);
}

TEST_F(WasmSubtypingTest, Subtyping) {
  v8::internal::AccountingAllocator allocator;
  WasmModule module1_;
  WasmModule module2_;

  WasmModule* module1 = &module1_;
  WasmModule* module2 = &module2_;

  // Set up two identical modules.
  for (WasmModule* module : {module1, module2}) {
    // Three mutually recursive types.
    /*  0 */ DefineStruct(module, {mut(refA(2)), immut(refNullA(2))},
                          kNoSuperType, false, false, false);
    /*  1 */ DefineStruct(module, {mut(refA(2)), immut(refA(2))}, Idx{0}, false,
                          false, false);
    /*  2 */ DefineArray(module, immut(refS(0)), kNoSuperType, false, false,
                         false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 3);

    /*  3 */ DefineArray(module, immut(refS(1)), Idx{2});
    /*  4 */ DefineStruct(
        module, {mut(refA(2)), immut(refA(3)), immut(kWasmF64)}, Idx{1});
    /*  5 */ DefineStruct(module, {mut(refNullA(2)), immut(refA(2))});
    /*  6 */ DefineArray(module, mut(kWasmI32));
    /*  7 */ DefineArray(module, immut(kWasmI32));
    /*  8 */ DefineStruct(module, {mut(kWasmI32), immut(refNullS(8))});
    /*  9 */ DefineStruct(module, {mut(kWasmI32), immut(refNullS(8))}, Idx{8});
    /* 10 */ DefineSignature(module, {}, {});
    /* 11 */ DefineSignature(module, {kWasmI32}, {kWasmI32});
    /* 12 */ DefineSignature(module, {kWasmI32, kWasmI32}, {kWasmI32});
    /* 13 */ DefineSignature(module, {refS(1)}, {kWasmI32});
    /* 14 */ DefineSignature(module, {refS(0)}, {kWasmI32}, Idx{13});
    /* 15 */ DefineSignature(module, {refS(0)}, {refS(0)});
    /* 16 */ DefineSignature(module, {refS(0)}, {refS(4)}, Idx{15});
    /* 17 */ DefineStruct(module, {mut(kWasmI32), immut(refNullS(17))});

    // Rec. group.
    /* 18 */ DefineStruct(module, {mut(kWasmI32), immut(refNullS(17))}, Idx{17},
                          false, false, false);
    /* 19 */ DefineArray(module, {mut(refNullF(21))}, kNoSuperType, false,
                         false, false);
    /* 20 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false, false, false);
    /* 21 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, Idx{20}, false,
                             false, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    // Identical rec. group.
    /* 22 */ DefineStruct(module, {mut(kWasmI32), immut(refNullS(17))}, Idx{17},
                          false, false, false);
    /* 23 */ DefineArray(module, {mut(refNullF(25))}, kNoSuperType, false,
                         false, false);
    /* 24 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false, false, false);
    /* 25 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, Idx{24}, false,
                             false, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    // Nonidentical rec. group: the last function extends a type outside the
    // recursive group.
    /* 26 */ DefineStruct(module, {mut(kWasmI32), immut(refNullS(17))}, Idx{17},
                          false, false, false);
    /* 27 */ DefineArray(module, {mut(refNullF(29))}, kNoSuperType, false,
                         false, false);
    /* 28 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false, false, false);
    /* 29 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, Idx{20}, false,
                             false, false);
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);

    /* 30 */ DefineStruct(module, {mut(kWasmI32), immut(refNullS(18))},
                          Idx{18});
    /* 31 */ DefineStruct(
        module, {mut(refA(2)), immut(refNullA(2)), immut(kWasmS128)}, Idx{1});

    // Final types
    /* 32 */ DefineStruct(module, {mut(kWasmI32)}, kNoSuperType, true);
    /* 33 */ DefineStruct(module, {mut(kWasmI32), mut(kWasmI64)}, Idx{32},
                          true);
    /* 34 */ DefineStruct(module, {mut(kWasmI32)}, kNoSuperType, true);
    /* 35 */ DefineStruct(module, {mut(kWasmI32)}, kNoSuperType, false);

    // Shared types.
    /* 36 */ DefineStruct(module, {mut(kWasmI32)}, kNoSuperType);
    /* 37 */ DefineStruct(module, {mut(kWasmI32), mut(kWasmI64)}, Idx{36});
    /* 38 */ DefineStruct(module, {mut(kWasmI32)}, kNoSuperType, false, true);
    /* 39 */ DefineStruct(module, {mut(kWasmI32), mut(kWasmI64)}, Idx{38},
                          false, true);
    /* 40 */ DefineStruct(module, {mut(kWasmI32)}, kNoSuperType, false, true);
    /* 41 */ DefineSignature(module, {kWasmI32}, {kWasmI32}, kNoSuperType,
                             false, true, true);

    // Continuation types (switching group)
    /* 42 */ DefineSignature(module, {kWasmI32}, {refNullC(45)}, kNoSuperType,
                             false, false, false);
    /* 43 */ DefineSignature(module, {refNullC(44)}, {kWasmI32}, kNoSuperType,
                             false, false, false);
    /* 44 */ DefineCont(module, ModuleTypeIndex{42});
    /* 45 */ DefineCont(module, ModuleTypeIndex{43});
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 4);
    // Continuation types, functions outside the group
    /* 46 */ DefineCont(module, ModuleTypeIndex{42});
    /* 47 */ DefineCont(module, ModuleTypeIndex{43}, Idx{45});
    GetTypeCanonicalizer()->AddRecursiveGroup(module, 2);
  }

  constexpr ValueType numeric_types[] = {kWasmI32, kWasmI64, kWasmF32, kWasmF64,
                                         kWasmS128};
  constexpr ValueType ref_types[] = {
      kWasmFuncRef,
      kWasmEqRef,
      kWasmStructRef,
      kWasmArrayRef,
      kWasmI31Ref,
      kWasmAnyRef,
      kWasmExternRef,
      kWasmNullExternRef,
      kWasmNullRef,
      kWasmNullFuncRef,
      kWasmStringRef,
      kWasmStringViewIter,
      kWasmExnRef,
      kWasmNullExnRef,
      kWasmRefNullExternString,
      kWasmContRef,
      kWasmNullContRef,
      refNullS(0),             // struct
      refS(0),                 // struct
      refNullS(0).AsExact(),   // exact struct
      refS(0).AsExact(),       // exact struct
      refNullA(2),             // array
      refA(2),                 // array
      refNullA(2).AsExact(),   // exact array
      refA(2).AsExact(),       // exact array
      refNullF(11),            // function
      refF(11),                // function
      refNullF(11).AsExact(),  // exact function
      refF(11).AsExact(),      // exact function
      refNullC(44),            // continuation
      refC(44),                // continuation
      refNullC(44).AsExact(),  // exact continuation
      refC(44).AsExact(),      // exact continuation
  };

// Some macros to help managing types and modules.
#define SUBTYPE(type1, type2)                      \
  EXPECT_TRUE(IsSubtypeOf(type1, type2, module1)); \
  EXPECT_TRUE(IsSubtypeOf(type1, type2, module1, module2))
#define SUBTYPE_IFF(type1, type2, condition)                \
  EXPECT_EQ(IsSubtypeOf(type1, type2, module1), condition); \
  EXPECT_EQ(IsSubtypeOf(type1, type2, module1, module2), condition)
#define NOT_SUBTYPE(type1, type2)                   \
  EXPECT_FALSE(IsSubtypeOf(type1, type2, module1)); \
  EXPECT_FALSE(IsSubtypeOf(type1, type2, module1, module2))
// Use only with indexed types.
#define VALID_SUBTYPE(type1, type2)                                           \
  EXPECT_TRUE(                                                                \
      ValidSubtypeDefinition(type1.ref_index(), type2.ref_index(), module1)); \
  EXPECT_TRUE(IsSubtypeOf(type1, type2, module1));                            \
  EXPECT_TRUE(IsSubtypeOf(type1, type2, module1, module2))
#define NOT_VALID_SUBTYPE(type1, type2) \
  EXPECT_FALSE(                         \
      ValidSubtypeDefinition(type1.ref_index(), type2.ref_index(), module1))
#define IDENTICAL(kind, index1, index2)                                        \
  EXPECT_TRUE(                                                                 \
      EquivalentTypes(refNull##kind(index1), refNull##kind(index2), module1)); \
  EXPECT_TRUE(EquivalentTypes(refNull##kind(index1), refNull##kind(index2),    \
                              module1, module2))
#define DISTINCT(kind, index1, index2)                                         \
  EXPECT_FALSE(                                                                \
      EquivalentTypes(refNull##kind(index1), refNull##kind(index2), module1)); \
  EXPECT_FALSE(EquivalentTypes(refNull##kind(index1), refNull##kind(index2),   \
                               module1, module2))
#define DISTINCT_SHARED(kind, index1, shared1, index2, shared2)           \
  EXPECT_FALSE(EquivalentTypes(refNull##kind(index1, shared1),            \
                               refNull##kind(index2, shared2), module1)); \
  EXPECT_FALSE(EquivalentTypes(refNull##kind(index1, shared1),            \
                               refNull##kind(index2, shared2), module1,   \
                               module2))
#define UNION(type1, type2, type_result) \
  EXPECT_EQ(Union(type1, type2, module1).type, type_result)
#define INTERSECTION(type1, type2, type_result) \
  EXPECT_EQ(Intersection(type1, type2, module1).type, type_result)

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
    ValueType inexact_ref_type = ref_type.AsExact(Exactness::kAnySubtype);

    const bool is_defined_func =
        inexact_ref_type == refNullF(11) || inexact_ref_type == refF(11);
    const bool is_defined_struct =
        inexact_ref_type == refNullS(0) || inexact_ref_type == refS(0);
    const bool is_defined_array =
        inexact_ref_type == refNullA(2) || inexact_ref_type == refA(2);
    const bool is_defined_cont =
        inexact_ref_type == refNullC(44) || inexact_ref_type == refC(44);

    const bool is_extern = ref_type == kWasmExternRef ||
                           ref_type == kWasmNullExternRef ||
                           ref_type == kWasmRefNullExternString;
    const bool is_any_func = ref_type == kWasmFuncRef ||
                             ref_type == kWasmNullFuncRef || is_defined_func;
    const bool is_string_view = ref_type == kWasmStringViewIter ||
                                ref_type == kWasmStringViewWtf8 ||
                                ref_type == kWasmStringViewWtf16;
    const bool is_any_cont = ref_type == kWasmContRef ||
                             ref_type == kWasmNullContRef || is_defined_cont;
    const bool is_exn = ref_type == kWasmExnRef || ref_type == kWasmNullExnRef;
    SCOPED_TRACE("ref_type: " + ref_type.name());
    // Concrete reference types, i31ref, structref and arrayref are subtypes
    // of eqref, externref/funcref/anyref/exnref/functions are not.
    SUBTYPE_IFF(ref_type, kWasmEqRef,
                ref_type != kWasmAnyRef && !is_any_func && !is_extern &&
                    !is_string_view && ref_type != kWasmStringRef && !is_exn &&
                    !is_any_cont);
    // Struct types are subtypes of structref.
    SUBTYPE_IFF(ref_type, kWasmStructRef,
                ref_type == kWasmStructRef || ref_type == kWasmNullRef ||
                    is_defined_struct);
    // Array types are subtypes of arrayref.
    SUBTYPE_IFF(ref_type, kWasmArrayRef,
                ref_type == kWasmArrayRef || ref_type == kWasmNullRef ||
                    is_defined_array);
    // Functions are subtypes of funcref.
    SUBTYPE_IFF(ref_type, kWasmFuncRef, is_any_func);
    // Each reference type is a subtype of itself.
    SUBTYPE(ref_type, ref_type);
    // Each non-func, non-extern, non-string-view, non-string-iter reference
    // type is a subtype of anyref.
    SUBTYPE_IFF(ref_type, kWasmAnyRef,
                !is_any_func && !is_extern && !is_string_view && !is_exn &&
                    !is_any_cont);
    // Only anyref is a supertype of anyref.
    SUBTYPE_IFF(kWasmAnyRef, ref_type, ref_type == kWasmAnyRef);
    // Only externref and nullexternref are subtypes of externref.
    SUBTYPE_IFF(ref_type, kWasmExternRef, is_extern);
    // Only nullexternref is a subtype of nullexternref.
    SUBTYPE_IFF(ref_type, kWasmNullExternRef, ref_type == kWasmNullExternRef);
    // Each nullable non-func, non-extern reference type is a supertype of
    // nullref.
    SUBTYPE_IFF(kWasmNullRef, ref_type,
                ref_type.is_nullable() && !is_any_func && !is_extern &&
                    !is_exn && !is_any_cont);
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
        EXPECT_TRUE(EquivalentTypes(ref_type, ref_type2, module2, module1));
        EXPECT_TRUE(EquivalentTypes(ref_type2, ref_type, module1, module2));
      } else {
        EXPECT_FALSE(EquivalentTypes(ref_type, ref_type2, module2, module1));
        EXPECT_FALSE(EquivalentTypes(ref_type2, ref_type, module1, module2));
      }
    }
  }

  // The rest of ref. types are unrelated.
  for (ValueType type_1 :
       {kWasmFuncRef, kWasmI31Ref, kWasmArrayRef, kWasmExnRef}) {
    for (ValueType type_2 :
         {kWasmFuncRef, kWasmI31Ref, kWasmArrayRef, kWasmExnRef}) {
      SUBTYPE_IFF(type_1, type_2, type_1 == type_2);
    }
  }

  // Unrelated refs are unrelated.
  NOT_VALID_SUBTYPE(refS(0), refA(2));
  NOT_VALID_SUBTYPE(refNullA(3), refNullS(1));
  // ref is a subtype of ref null for the same struct/array.
  VALID_SUBTYPE(refS(0), refNullS(0));
  VALID_SUBTYPE(refA(2), refNullA(2));
  // ref null is not a subtype of ref for the same struct/array.
  NOT_SUBTYPE(refNullS(0), refS(0));
  NOT_SUBTYPE(refNullA(2), refA(2));
  // ref is a subtype of ref null if the same is true for the underlying
  // structs/arrays.
  VALID_SUBTYPE(refA(3), refNullA(2));
  // Prefix subtyping for structs.
  VALID_SUBTYPE(refNullS(4), refNullS(0));
  // Mutable fields are invariant.
  NOT_VALID_SUBTYPE(refS(0), refS(5));
  // Immutable fields are covariant.
  VALID_SUBTYPE(refS(1), refS(0));
  // Prefix subtyping + immutable field covariance for structs.
  VALID_SUBTYPE(refNullS(4), refNullS(1));
  // No subtyping between mutable/immutable fields.
  NOT_VALID_SUBTYPE(refA(7), refA(6));
  NOT_VALID_SUBTYPE(refA(6), refA(7));
  // Recursive types.
  VALID_SUBTYPE(refS(9), refS(8));

  // Function subtyping;
  // Unrelated function types are unrelated.
  NOT_VALID_SUBTYPE(refF(10), refF(11));
  // Function type with different parameter counts are unrelated.
  NOT_VALID_SUBTYPE(refF(12), refF(11));
  // Parameter contravariance holds.
  VALID_SUBTYPE(refF(14), refF(13));
  // Return type covariance holds.
  VALID_SUBTYPE(refF(16), refF(15));
  // Identical types are subtype-related.
  VALID_SUBTYPE(refF(10), refF(10));
  VALID_SUBTYPE(refF(11), refF(11));

  // Continuation subtyping:
  VALID_SUBTYPE(refC(44), refC(44));
  NOT_VALID_SUBTYPE(refC(44), refC(45));
  VALID_SUBTYPE(refC(45), refC(45));
  NOT_VALID_SUBTYPE(refC(45), refC(44));

  INTERSECTION(refF(11), refC(44), kWasmBottom);

  INTERSECTION(refNullC(44), refNullC(45), kWasmNullContRef);
  INTERSECTION(refNullC(44), kWasmContRef, refNullC(44));
  INTERSECTION(refC(44), kWasmContRef, refC(44));
  INTERSECTION(refC(44), refC(45), kWasmBottom);
  INTERSECTION(refNullC(44), kWasmNullContRef, kWasmNullContRef);
  INTERSECTION(kWasmContRef, kWasmNullContRef, kWasmNullContRef);

  UNION(kWasmNullContRef, kWasmContRef, kWasmContRef);
  UNION(refNullC(44), kWasmContRef, kWasmContRef);
  UNION(refC(44), kWasmContRef, kWasmContRef);
  UNION(refNullC(44), refNullC(45), kWasmContRef);
  UNION(refC(44), refC(45), kWasmContRef.AsNonNull());
  UNION(refNullC(44), kWasmNullContRef, refNullC(44));
  UNION(kWasmContRef, kWasmNullContRef, kWasmContRef);

  // Canonicalization tests.

  // Groups should only be canonicalized to identical groups.
  IDENTICAL(S, 18, 22);
  IDENTICAL(A, 19, 23);
  IDENTICAL(F, 20, 24);
  IDENTICAL(F, 21, 25);

  DISTINCT(S, 18, 26);
  DISTINCT(A, 19, 27);
  DISTINCT(F, 20, 28);
  DISTINCT(F, 21, 29);

  // A type should not be canonicalized to an identical one with a different
  // group structure.
  DISTINCT(S, 18, 17);

  // A subtype should also be subtype of an equivalent type.
  VALID_SUBTYPE(refS(30), refS(18));
  VALID_SUBTYPE(refS(30), refS(22));
  NOT_SUBTYPE(refS(30), refS(26));

  // Final types

  // A type is not a valid subtype of a final type.
  NOT_VALID_SUBTYPE(refS(33), refS(32));
  IDENTICAL(S, 32, 34);
  // A final and a non-final type are distinct.
  DISTINCT(S, 32, 35);

  /* Shared types */
  // A shared type can be a subtype of a shared type.
  VALID_SUBTYPE(refS(39, kShared), refS(38, kShared));
  // A shared type is not a valid subtype of a non-shared type and vice versa.
  NOT_VALID_SUBTYPE(refS(39, kShared), refS(36));
  NOT_VALID_SUBTYPE(refS(37), refS(38, kShared));
  // Two shared types are identical. A shared and non-shared type are
  // distinct.
  IDENTICAL(S, 38, 40);
  DISTINCT_SHARED(S, 36, kNotShared, 38, kShared);

  // Abstract types.
  auto Gen = ValueType::Generic;
  using G = GenericKind;
  ValueType kRefAny = kWasmAnyRef.AsNonNull();
  ValueType kRefAnyShared = Gen(G::kAny, kNonNullable, kShared);
  ValueType kRefEq = kWasmEqRef.AsNonNull();
  ValueType kRefEqShared = Gen(G::kEq, kNonNullable, kShared);
  ValueType kRefI31Shared = Gen(G::kI31, kNonNullable, kShared);
  ValueType kRefStructShared = Gen(G::kStruct, kNonNullable, kShared);
  ValueType kRefArrayShared = Gen(G::kArray, kNonNullable, kShared);
  ValueType kRefNoneShared = Gen(G::kNone, kNonNullable, kShared);
  ValueType kRefFunc = kWasmFuncRef.AsNonNull();
  ValueType kRefFuncShared = Gen(G::kFunc, kNonNullable, kShared);
  ValueType kRefNoFuncShared = Gen(G::kNoFunc, kNonNullable, kShared);
  ValueType kRefNoExternShared = Gen(G::kNoExtern, kNonNullable, kShared);
  ValueType kRefNullFuncShared = Gen(G::kFunc, kNullable, kShared);
  ValueType kRefNullEqShared = Gen(G::kEq, kNullable, kShared);
  ValueType kRefNullExternShared = Gen(G::kExtern, kNullable, kShared);
  ValueType kRefNullNoneShared = Gen(G::kNone, kNullable, kShared);
  ValueType kRefNullNoFuncShared = Gen(G::kNoFunc, kNullable, kShared);
  ValueType kRefNullI31Shared = Gen(G::kI31, kNullable, kShared);

  SUBTYPE(kRefEqShared, kRefAnyShared);
  NOT_SUBTYPE(kRefEqShared, kRefAny);
  NOT_SUBTYPE(kRefEq, kRefAnyShared);
  NOT_SUBTYPE(kRefFuncShared, kRefAnyShared);
  SUBTYPE(kRefNullNoneShared, kRefNullI31Shared);
  SUBTYPE(kRefNullNoFuncShared, kRefNullFuncShared);
  SUBTYPE(refS(40, kShared), kRefNullEqShared);
  SUBTYPE(kRefNullNoneShared, refNullS(40, kShared));
  NOT_SUBTYPE(refS(40, kShared), kWasmEqRef);
  NOT_SUBTYPE(refS(40, kShared), kRefNullExternShared);
  SUBTYPE(refF(41, kShared), kRefNullFuncShared);
  SUBTYPE(kRefNullNoFuncShared, refNullF(41, kShared));
  NOT_SUBTYPE(kRefNullNoFuncShared, refF(41, kShared));
  NOT_SUBTYPE(refF(41, kShared), kWasmSharedAnyRef);
  NOT_SUBTYPE(refF(41, kShared), kWasmFuncRef);
  NOT_SUBTYPE(refS(0), kRefStructShared);
  NOT_SUBTYPE(refA(2), kRefArrayShared);
  NOT_SUBTYPE(refF(10), kRefFuncShared);

  // Unions and intersections.

  // Distinct numeric types are unrelated.
  for (ValueType type1 : numeric_types) {
    for (ValueType type2 : numeric_types) {
      UNION(type1, type2, (type1 == type2 ? type1 : kWasmTop));
      INTERSECTION(type1, type2, (type1 == type2 ? type1 : kWasmBottom));
    }
  }
  // Numeric and reference types are unrelated.
  for (ValueType type1 : numeric_types) {
    for (ValueType type2 : ref_types) {
      UNION(type1, type2, kWasmTop);
      INTERSECTION(type1, type2, kWasmBottom);
    }
  }

  // Reference type vs. itself and anyref.
  for (ValueType type : ref_types) {
    SCOPED_TRACE(type.name());
    ValueType inexact_type = type.AsExact(Exactness::kAnySubtype);
    if (type == kWasmStringViewIter || type == kWasmStringViewWtf8 ||
        type == kWasmStringViewWtf16) {
      // String views aren't subtypes of any nor supertypes of null.
      INTERSECTION(type, kWasmAnyRef, kWasmBottom);
      INTERSECTION(type, kWasmNullRef, kWasmBottom);

    } else if (type == kWasmFuncRef || type == kWasmNullFuncRef ||
               inexact_type == refF(11) || inexact_type == refNullF(11) ||
               type == kWasmExternRef || type == kWasmNullExternRef ||
               type == kWasmRefNullExternString || type == kWasmContRef ||
               type == kWasmNullContRef || inexact_type == refNullC(44) ||
               inexact_type == refC(44)) {
      // func, cont and extern types don't share the same type hierarchy as
      // anyref.
      INTERSECTION(type, kWasmAnyRef, kWasmBottom);
    } else {
      bool is_exn = type == kWasmExnRef || type == kWasmNullExnRef;
      UNION(kWasmAnyRef, type, is_exn ? kWasmTop : kWasmAnyRef);
      INTERSECTION(kWasmAnyRef, type, is_exn ? kWasmBottom : type);
      UNION(kWasmAnyRef.AsNonNull(), type,
            is_exn               ? kWasmTop
            : type.is_nullable() ? kWasmAnyRef
                                 : kWasmAnyRef.AsNonNull());
      INTERSECTION(kWasmAnyRef.AsNonNull(), type,
                   is_exn                 ? kWasmBottom
                   : type != kWasmNullRef ? type.AsNonNull()
                                          : kWasmBottom);
    }
  }

  // Exact and inexact versions of the same type
  UNION(refF(11), refF(11).AsExact(), refF(11));
  INTERSECTION(refF(11), refF(11).AsExact(), refF(11).AsExact());
  UNION(refNullS(0), refNullS(0).AsExact(), refNullS(0));
  INTERSECTION(refNullS(0), refNullS(0).AsExact(), refNullS(0).AsExact());
  UNION(kWasmNullFuncRef, refF(11).AsExact(), refNullF(11).AsExact());
  INTERSECTION(kWasmNullFuncRef, refF(11).AsExact(), kWasmBottom);
  UNION(refF(11).AsExact(), refNullF(11).AsExact(), refNullF(11).AsExact());
  INTERSECTION(refF(11).AsExact(), refNullF(11).AsExact(), refF(11).AsExact());

  // Abstract types vs abstract types.
  UNION(kWasmEqRef, kWasmStructRef, kWasmEqRef);
  UNION(kWasmEqRef, kWasmI31Ref, kWasmEqRef);
  UNION(kWasmEqRef, kWasmArrayRef, kWasmEqRef);
  UNION(kWasmEqRef, kWasmNullRef, kWasmEqRef);
  UNION(kWasmStructRef, kWasmI31Ref, kWasmEqRef);
  UNION(kWasmStructRef, kWasmArrayRef, kWasmEqRef);
  UNION(kWasmStructRef, kWasmNullRef, kWasmStructRef);
  UNION(kWasmI31Ref.AsNonNull(), kWasmArrayRef.AsNonNull(),
        kWasmEqRef.AsNonNull());
  UNION(kWasmI31Ref, kWasmNullRef, kWasmI31Ref);
  UNION(kWasmArrayRef, kWasmNullRef, kWasmArrayRef);
  UNION(kWasmStructRef.AsNonNull(), kWasmI31Ref.AsNonNull(),
        kWasmEqRef.AsNonNull());
  UNION(kWasmI31Ref.AsNonNull(), kWasmArrayRef, kWasmEqRef);
  UNION(kWasmAnyRef, kWasmNullRef, kWasmAnyRef);
  UNION(kWasmExternRef, kWasmNullExternRef, kWasmExternRef);
  UNION(kWasmRefNullExternString, kWasmNullExternRef, kWasmRefNullExternString);
  UNION(kWasmRefNullExternString.AsNonNull(), kWasmNullExternRef,
        kWasmRefNullExternString);
  UNION(kWasmRefNullExternString, kWasmExternRef, kWasmExternRef);
  UNION(kWasmRefNullExternString, kWasmAnyRef, kWasmTop);
  UNION(kWasmRefNullExternString, kWasmFuncRef, kWasmTop);
  // Imported strings and stringref represent the same values. Still, they are
  // in different type hierarchies and therefore incompatible (e.g. due to
  // different null representation).
  // (There is no interoperability between stringref and imported strings as
  // they are competing proposals.)
  UNION(kWasmRefNullExternString, kWasmStringRef, kWasmTop);
  UNION(kWasmRefNullExternString.AsNonNull(), kWasmStringRef.AsNonNull(),
        kWasmTop);
  UNION(kWasmFuncRef, kWasmNullFuncRef, kWasmFuncRef);
  UNION(kWasmFuncRef, kWasmStructRef, kWasmTop);
  UNION(kWasmFuncRef, kWasmArrayRef, kWasmTop);
  UNION(kWasmFuncRef, kWasmAnyRef, kWasmTop);
  UNION(kWasmFuncRef, kWasmEqRef, kWasmTop);
  UNION(kWasmStringRef, kWasmAnyRef, kWasmAnyRef);
  UNION(kWasmStringRef, kWasmStructRef, kWasmAnyRef);
  UNION(kWasmStringRef, kWasmArrayRef, kWasmAnyRef);
  UNION(kWasmStringRef, kWasmFuncRef, kWasmTop);
  UNION(kWasmStringViewIter, kWasmStringRef, kWasmTop);
  UNION(kWasmStringViewWtf8, kWasmStringRef, kWasmTop);
  UNION(kWasmStringViewWtf16, kWasmStringRef, kWasmTop);
  UNION(kWasmStringViewIter, kWasmAnyRef, kWasmTop);
  UNION(kWasmStringViewWtf8, kWasmAnyRef, kWasmTop);
  UNION(kWasmStringViewWtf16, kWasmAnyRef, kWasmTop);
  UNION(kWasmNullFuncRef, kWasmEqRef, kWasmTop);

  INTERSECTION(kWasmExternRef, kWasmEqRef, kWasmBottom);
  INTERSECTION(kWasmExternRef, kWasmStructRef, kWasmBottom);
  INTERSECTION(kWasmExternRef, kWasmI31Ref.AsNonNull(), kWasmBottom);
  INTERSECTION(kWasmExternRef, kWasmArrayRef, kWasmBottom);
  INTERSECTION(kWasmExternRef, kWasmNullRef, kWasmBottom);
  INTERSECTION(kWasmExternRef, kWasmFuncRef, kWasmBottom);
  INTERSECTION(kWasmNullExternRef, kWasmEqRef, kWasmBottom);
  INTERSECTION(kWasmNullExternRef, kWasmStructRef, kWasmBottom);
  INTERSECTION(kWasmNullExternRef, kWasmI31Ref, kWasmBottom);
  INTERSECTION(kWasmNullExternRef, kWasmArrayRef, kWasmBottom);
  INTERSECTION(kWasmNullExternRef, kWasmNullRef, kWasmBottom);
  INTERSECTION(kWasmNullExternRef, kWasmExternRef, kWasmNullExternRef);
  INTERSECTION(kWasmNullExternRef, kWasmExternRef.AsNonNull(), kWasmBottom);
  INTERSECTION(kWasmRefNullExternString, kWasmEqRef, kWasmBottom);
  INTERSECTION(kWasmRefNullExternString, kWasmAnyRef, kWasmBottom);
  INTERSECTION(kWasmRefNullExternString, kWasmFuncRef.AsNonNull(), kWasmBottom);
  INTERSECTION(kWasmRefNullExternString, kWasmNullRef, kWasmBottom);
  INTERSECTION(kWasmRefNullExternString, kWasmNullExternRef,
               kWasmNullExternRef);
  INTERSECTION(kWasmRefNullExternString.AsNonNull(), kWasmNullExternRef,
               kWasmBottom);
  INTERSECTION(kWasmRefNullExternString, kWasmExternRef,
               kWasmRefNullExternString);
  INTERSECTION(kWasmRefNullExternString, kWasmExternRef.AsNonNull(),
               kWasmRefNullExternString.AsNonNull());

  INTERSECTION(kWasmFuncRef, kWasmEqRef, kWasmBottom);
  INTERSECTION(kWasmFuncRef, kWasmStructRef, kWasmBottom);
  INTERSECTION(kWasmFuncRef, kWasmI31Ref.AsNonNull(), kWasmBottom);
  INTERSECTION(kWasmFuncRef, kWasmArrayRef, kWasmBottom);
  INTERSECTION(kWasmFuncRef, kWasmNullRef, kWasmBottom);
  INTERSECTION(kWasmFuncRef, kWasmNullExternRef, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, kWasmEqRef, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, kWasmStructRef, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, kWasmI31Ref, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, kWasmArrayRef, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, kWasmNullRef, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, kWasmFuncRef, kWasmNullFuncRef);
  INTERSECTION(kWasmNullFuncRef, kWasmFuncRef.AsNonNull(), kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, kWasmNullExternRef, kWasmBottom);

  INTERSECTION(kWasmEqRef, kWasmStructRef, kWasmStructRef);
  INTERSECTION(kWasmEqRef, kWasmI31Ref, kWasmI31Ref);
  INTERSECTION(kWasmEqRef, kWasmArrayRef, kWasmArrayRef);
  INTERSECTION(kWasmEqRef, kWasmNullRef, kWasmNullRef);
  INTERSECTION(kWasmEqRef, kWasmFuncRef, kWasmBottom);
  INTERSECTION(kWasmStructRef, kWasmI31Ref, kWasmNullRef);
  INTERSECTION(kWasmStructRef, kWasmArrayRef, kWasmNullRef);
  INTERSECTION(kWasmStructRef, kWasmNullRef, kWasmNullRef);
  INTERSECTION(kWasmI31Ref, kWasmArrayRef, kWasmNullRef);
  INTERSECTION(kWasmI31Ref.AsNonNull(), kWasmNullRef, kWasmBottom);
  INTERSECTION(kWasmArrayRef.AsNonNull(), kWasmNullRef, kWasmBottom);

  ValueType struct_type = refS(0);
  ValueType array_type = refA(2);
  ValueType function_type = refF(11);

  // Abstract vs indexed types.
  UNION(kWasmFuncRef, function_type, kWasmFuncRef);
  UNION(kWasmFuncRef, struct_type, kWasmTop);
  UNION(kWasmFuncRef, array_type, kWasmTop);
  INTERSECTION(kWasmFuncRef, struct_type, kWasmBottom);
  INTERSECTION(kWasmFuncRef, array_type, kWasmBottom);
  INTERSECTION(kWasmFuncRef, function_type, function_type);

  UNION(kWasmExnRef, struct_type, kWasmTop);
  UNION(kWasmExnRef, array_type, kWasmTop);
  UNION(kWasmExnRef, function_type, kWasmTop);
  INTERSECTION(kWasmExnRef, struct_type, kWasmBottom);
  INTERSECTION(kWasmExnRef, array_type, kWasmBottom);
  INTERSECTION(kWasmExnRef, function_type, kWasmBottom);

  UNION(kWasmNullFuncRef, function_type, function_type.AsNullable());
  UNION(kWasmNullFuncRef, struct_type, kWasmTop);
  UNION(kWasmNullFuncRef, array_type, kWasmTop);
  INTERSECTION(kWasmNullFuncRef, struct_type, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, struct_type.AsNullable(), kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, array_type, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, array_type.AsNullable(), kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, function_type, kWasmBottom);
  INTERSECTION(kWasmNullFuncRef, function_type.AsNullable(), kWasmNullFuncRef);

  UNION(kWasmEqRef, struct_type, kWasmEqRef);
  UNION(kWasmEqRef, array_type, kWasmEqRef);
  INTERSECTION(kWasmEqRef, struct_type, struct_type);
  INTERSECTION(kWasmEqRef, array_type, array_type);
  INTERSECTION(kWasmEqRef, function_type, kWasmBottom);

  UNION(kWasmStructRef, struct_type, kWasmStructRef);
  UNION(kWasmStructRef, array_type, kWasmEqRef);
  UNION(kWasmStructRef, function_type, kWasmTop);
  INTERSECTION(kWasmStructRef, struct_type, struct_type);
  INTERSECTION(kWasmStructRef, array_type, kWasmBottom);
  INTERSECTION(kWasmStructRef, function_type, kWasmBottom);

  UNION(kWasmI31Ref, struct_type, kWasmEqRef);
  UNION(kWasmI31Ref, array_type, kWasmEqRef);
  INTERSECTION(kWasmI31Ref, struct_type, kWasmBottom);
  INTERSECTION(kWasmI31Ref, array_type, kWasmBottom);
  INTERSECTION(kWasmI31Ref, function_type, kWasmBottom);

  UNION(kWasmArrayRef, struct_type, kWasmEqRef);
  UNION(kWasmArrayRef, array_type, kWasmArrayRef);
  UNION(kWasmArrayRef, function_type, kWasmTop);
  INTERSECTION(kWasmArrayRef, struct_type, kWasmBottom);
  INTERSECTION(kWasmArrayRef, array_type, array_type);
  INTERSECTION(kWasmArrayRef, function_type, kWasmBottom);

  UNION(kWasmNullRef, struct_type, struct_type.AsNullable());
  UNION(kWasmNullRef, array_type, array_type.AsNullable());
  UNION(kWasmNullRef, function_type, kWasmTop);
  INTERSECTION(kWasmNullRef, struct_type, kWasmBottom);
  INTERSECTION(kWasmNullRef, array_type, kWasmBottom);
  INTERSECTION(kWasmNullRef, function_type, kWasmBottom);
  INTERSECTION(kWasmNullRef, struct_type.AsNullable(), kWasmNullRef);
  INTERSECTION(kWasmNullRef, array_type.AsNullable(), kWasmNullRef);
  INTERSECTION(kWasmNullRef, function_type.AsNullable(), kWasmBottom);

  UNION(struct_type, kWasmStringRef, kWasmAnyRef);
  UNION(array_type, kWasmStringRef, kWasmAnyRef);
  UNION(function_type, kWasmStringRef, kWasmTop);

  UNION(struct_type, kWasmRefNullExternString, kWasmTop);
  UNION(array_type, kWasmRefNullExternString, kWasmTop);
  UNION(function_type, kWasmRefNullExternString, kWasmTop);

  // Indexed types of different kinds.
  UNION(struct_type, array_type, kRefEq);
  INTERSECTION(struct_type, array_type, kWasmBottom);
  INTERSECTION(struct_type, function_type, kWasmBottom);
  INTERSECTION(array_type, function_type, kWasmBottom);

  // Nullable vs. non-nullable.
  UNION(struct_type, struct_type.AsNullable(), struct_type.AsNullable());
  INTERSECTION(struct_type, struct_type.AsNullable(), struct_type);
  UNION(kWasmStructRef, kWasmStructRef, kWasmStructRef);
  INTERSECTION(kWasmStructRef, kWasmStructRef, kWasmStructRef);

  // Concrete types of the same kind.
  // Subtyping relation.
  UNION(refNullS(4), refS(1), refNullS(1));
  INTERSECTION(refNullS(4), refS(1), refS(4));
  INTERSECTION(refNullS(1), refNullS(4), refNullS(4));
  INTERSECTION(refS(0), refS(1), refS(1));
  INTERSECTION(refS(0).AsExact(), refS(1), kWasmBottom);
  INTERSECTION(refS(0), refS(1).AsExact(), refS(1).AsExact());
  INTERSECTION(refS(0).AsExact(), refS(1).AsExact(), kWasmBottom);
  // Common ancestor.
  UNION(refS(4), refS(31), refS(1));
  INTERSECTION(refS(4), refS(31), kWasmBottom);
  // No common ancestor.
  UNION(refA(6), refNullA(2), kWasmArrayRef);
  INTERSECTION(refA(6), refNullA(2), kWasmBottom);
  UNION(refS(0), refS(17), kWasmStructRef.AsNonNull());
  INTERSECTION(refS(0), refS(17), kWasmBottom);
  UNION(refF(10), refNullF(11), kWasmFuncRef);
  INTERSECTION(refF(10), refNullF(11), kWasmBottom);

  // Shared types
  ValueType struct_shared = refS(40, kShared);
  ValueType function_shared = refF(41, kShared);
  UNION(struct_shared, struct_shared.AsNullable(), struct_shared.AsNullable());
  UNION(struct_shared, struct_type, kWasmTop);
  UNION(struct_shared, function_shared, kWasmTop);
  UNION(struct_shared, kRefI31Shared, kRefEqShared);
  UNION(struct_shared, kRefAnyShared, kRefAnyShared);
  UNION(struct_shared, kRefNoneShared, struct_shared);
  UNION(struct_shared, kRefAny, kWasmTop);
  INTERSECTION(struct_shared, struct_shared.AsNullable(), struct_shared);
  INTERSECTION(struct_shared, struct_type, kWasmBottom);
  INTERSECTION(struct_shared, function_shared, kWasmBottom);
  INTERSECTION(struct_shared.AsNullable(), kRefNullI31Shared,
               kRefNullNoneShared);
  INTERSECTION(struct_shared, kRefAnyShared, struct_shared);
  INTERSECTION(struct_shared.AsNullable(), kRefNullNoneShared,
               kRefNullNoneShared);
  INTERSECTION(struct_shared, kRefAny, kWasmBottom);
  UNION(function_shared, kRefFuncShared, kRefFuncShared);
  UNION(function_shared, kRefFunc, kWasmTop);
  UNION(function_shared, kRefEqShared, kWasmTop);
  UNION(function_shared, kRefNoFuncShared, function_shared);
  UNION(function_shared, kRefNoExternShared, kWasmTop);
  INTERSECTION(function_shared, kRefFuncShared, function_shared);
  INTERSECTION(function_shared, kRefFunc, kWasmBottom);
  INTERSECTION(function_shared, kRefEqShared, kWasmBottom);
  INTERSECTION(function_shared.AsNullable(), kRefNullNoFuncShared,
               kRefNullNoFuncShared);
  INTERSECTION(function_shared, kRefNoExternShared, kWasmBottom);

  // Generic test covering all kinds of always applicable rules (like
  // commutativity).
  std::vector<ValueType> test_types;
  test_types.reserve(arraysize(numeric_types) + arraysize(ref_types));
  test_types.insert(test_types.end(), std::begin(numeric_types),
                    std::end(numeric_types));
  test_types.insert(test_types.end(), std::begin(ref_types),
                    std::end(ref_types));
  test_types.push_back(kWasmBottom);
  test_types.push_back(kWasmTop);
  for (const ValueType type_a : test_types) {
    SCOPED_TRACE("a = " + type_a.name());
    // Neutral elements: kWasmTop wrt. intersection, kWasmBottom wrt. union.
    INTERSECTION(type_a, kWasmTop, type_a);
    UNION(type_a, kWasmBottom, type_a);
    // Absorbing element: kWasmTop wrt. union, kWasmBottom wrt. intersection.
    UNION(type_a, kWasmTop, kWasmTop);
    INTERSECTION(type_a, kWasmBottom, kWasmBottom);

    UNION(type_a, type_a, type_a);         // idempotency
    INTERSECTION(type_a, type_a, type_a);  // idempotency

    for (const ValueType type_b : test_types) {
      SCOPED_TRACE("b = " + type_b.name());

      // There may not be any "cycles" in the type hierarchy.
      if (IsSubtypeOf(type_a, type_b, module1) && type_a != type_b) {
        EXPECT_FALSE(IsSubtypeOf(type_b, type_a, module1));
      }

      // The union of two types is always a super type of both types.
      ValueType union_ab = Union(type_a, type_b, module1).type;
      EXPECT_TRUE(IsSubtypeOf(type_a, union_ab, module1));
      EXPECT_TRUE(IsSubtypeOf(type_b, union_ab, module1));

      // Test commutativity.
      EXPECT_EQ(Union(type_a, type_b, module1).type,
                Union(type_b, type_a, module1).type);
      EXPECT_EQ(Intersection(type_a, type_b, module1).type,
                Intersection(type_b, type_a, module1).type);

      // If the union of a and b is b, then a is a subtype of b, so the
      // intersection has to be a.
      EXPECT_EQ(Union(type_a, type_b, module1).type == type_b,
                Intersection(type_a, type_b, module1).type == type_a);

      for (const ValueType type_c : test_types) {
        SCOPED_TRACE("c = " + type_c.name());
        // Test associativity.
        EXPECT_EQ(
            Union(type_a, Union(type_b, type_c, module1).type, module1).type,
            Union(Union(type_a, type_b, module1).type, type_c, module1).type);
        EXPECT_EQ(
            Intersection(type_a, Intersection(type_b, type_c, module1).type,
                         module1)
                .type,
            Intersection(Intersection(type_a, type_b, module1).type, type_c,
                         module1)
                .type);

        // Test transitivity.
        if (IsSubtypeOf(type_a, type_b, module1) &&
            IsSubtypeOf(type_b, type_c, module1)) {
          EXPECT_TRUE(IsSubtypeOf(type_a, type_c, module1));
        }

        // The Union(a, b) is the most specific supertype of a and b.
        // Therefore there may not be any type c that is a supertype of a and b
        // but not a supertype of their union.
        if (IsSubtypeOf(type_a, type_c, module1) &&
            IsSubtypeOf(type_b, type_c, module1)) {
          EXPECT_TRUE(IsSubtypeOf(union_ab, type_c, module1));
        }
      }
    }
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
}

}  // namespace v8::internal::wasm::subtyping_unittest
