// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/casting.h"

#include "src/heap/factory.h"
#include "src/heap/heap-layout-inl.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"

namespace v8::internal {
namespace {

using CastingTest = TestWithNativeContext;

struct Expected {
  bool smi;
  bool fixed_array;
  bool byte_array;
  bool weak_fixed_array;
  bool weak_byte_array;
  bool cleared;
  bool read_only = false;
};

template <typename To, typename From>
void CheckCast(Tagged<From> value, bool expected) {
  SCOPED_TRACE(V8_PRETTY_FUNCTION_VALUE_OR("CheckCast"));
  EXPECT_EQ(expected, Is<To>(value));
  EXPECT_EQ(expected, CastTraitsImpl<To>::AllowFrom(value));
  Tagged<To> result = UncheckedCast<To>(Smi::zero());
  ASSERT_EQ(expected, TryCast<To>(value, &result));
  if (expected) {
    EXPECT_EQ(value.ptr(), result.ptr());
    EXPECT_EQ(value.ptr(), Cast<To>(value).ptr());
    EXPECT_EQ(value.ptr(), CheckedCast<To>(value).ptr());
  } else {
    EXPECT_EQ(Smi::zero().ptr(), result.ptr());
  }
}

template <typename From>
void CheckTargets(Tagged<From> value, Expected e) {
  CheckCast<Smi>(value, e.smi);
  CheckCast<Number>(value, e.smi);
  CheckCast<FixedArray>(value, e.fixed_array);
  CheckCast<ByteArray>(value, e.byte_array);
  CheckCast<HeapObject>(value, e.fixed_array || e.byte_array);
  CheckCast<Object>(value, e.smi || e.fixed_array || e.byte_array);
  CheckCast<ReadOnly<FixedArray>>(value, e.read_only && e.fixed_array);
  CheckCast<ReadOnly<ByteArray>>(value, e.read_only && e.byte_array);
  CheckCast<ReadOnly<HeapObject>>(
      value, e.read_only && (e.fixed_array || e.byte_array));
  CheckCast<UnionOf<Smi, ReadOnly<FixedArray>>>(
      value, e.smi || (e.read_only && e.fixed_array));
  CheckCast<UnionOf<ReadOnly<FixedArray>, ByteArray>>(
      value, (e.read_only && e.fixed_array) || e.byte_array);
  CheckCast<UnionOf<ReadOnly<FixedArray>, Weak<ByteArray>>>(
      value, (e.read_only && e.fixed_array) || e.weak_byte_array || e.cleared);
  CheckCast<Weak<FixedArray>>(value, e.weak_fixed_array || e.cleared);
  CheckCast<Weak<ByteArray>>(value, e.weak_byte_array || e.cleared);
  CheckCast<Weak<HeapObject>>(
      value, e.weak_fixed_array || e.weak_byte_array || e.cleared);
  CheckCast<ClearedWeakValue>(value, e.cleared);
  CheckCast<MaybeObject>(value, true);
  CheckCast<MaybeWeak<FixedArray>>(
      value, e.fixed_array || e.weak_fixed_array || e.cleared);
  CheckCast<UnionOf<Smi, FixedArray>>(value, e.smi || e.fixed_array);
  CheckCast<UnionOf<FixedArray, ByteArray>>(value,
                                            e.fixed_array || e.byte_array);
  CheckCast<UnionOf<FixedArray, Weak<ByteArray>>>(
      value, e.fixed_array || e.weak_byte_array || e.cleared);
  CheckCast<UnionOf<Weak<FixedArray>, ByteArray>>(
      value, e.weak_fixed_array || e.byte_array || e.cleared);
  CheckCast<UnionOf<Weak<FixedArray>, Weak<ByteArray>>>(
      value, e.weak_fixed_array || e.weak_byte_array || e.cleared);
  CheckCast<UnionOf<Smi, Weak<FixedArray>>>(
      value, e.smi || e.weak_fixed_array || e.cleared);
  CheckCast<JSPrimitive>(value, e.smi);
  CheckCast<JSAny>(value, e.smi);
}

template <typename From>
void CheckSourceTypes(Tagged<From> value, Expected expected) {
  CheckTargets(value, expected);
  auto check = [&]<typename U>() {
    if constexpr (is_subtype_v<From, U>) {
      CheckTargets(Tagged<U>(value), expected);
    }
  };
  check.template operator()<Object>();
  check.template operator()<HeapObject>();
  check.template operator()<ReadOnly<FixedArray>>();
  check.template operator()<ReadOnly<ByteArray>>();
  check.template operator()<ReadOnly<HeapObject>>();
  check.template operator()<Weak<HeapObject>>();
  check.template operator()<MaybeObject>();
  check.template operator()<MaybeWeak<HeapObject>>();
  check.template operator()<MaybeWeak<FixedArray>>();
  check.template operator()<UnionOf<Smi, FixedArray>>();
  check.template operator()<UnionOf<FixedArray, ByteArray>>();
  check.template operator()<UnionOf<FixedArray, Weak<ByteArray>>>();
  check.template operator()<UnionOf<Weak<FixedArray>, ByteArray>>();
  check.template operator()<UnionOf<Weak<FixedArray>, Weak<ByteArray>>>();
  check.template operator()<UnionOf<Smi, Weak<FixedArray>>>();
  check.template operator()<UnionOf<ReadOnly<FixedArray>, ByteArray>>();
  check.template operator()<UnionOf<ReadOnly<FixedArray>, Weak<ByteArray>>>();
}

TEST_F(CastingTest, StrongFixedArray) {
  auto value = *isolate()->factory()->empty_fixed_array();
  ASSERT_TRUE(HeapLayout::InReadOnlySpace(value));
  Expected expected{false, true, false, false, false, false, true};
  CheckSourceTypes(value, expected);
  CheckSourceTypes(Cast<ReadOnly<FixedArray>>(value), expected);
}

TEST_F(CastingTest, MutableFixedArray) {
  auto value = *isolate()->factory()->NewFixedArray(1);
  ASSERT_FALSE(HeapLayout::InReadOnlySpace(value));
  CheckSourceTypes(value, {false, true, false, false, false, false});
}

TEST_F(CastingTest, ReadOnlyByteArray) {
  auto value = ReadOnlyRoots(isolate()).empty_byte_array();
  ASSERT_TRUE(HeapLayout::InReadOnlySpace(value));
  Expected expected{false, false, true, false, false, false, true};
  CheckSourceTypes(value, expected);
  CheckSourceTypes(Cast<ReadOnly<ByteArray>>(value), expected);
}

TEST_F(CastingTest, WeakFixedArray) {
  CheckSourceTypes(MakeWeak(*isolate()->factory()->empty_fixed_array()),
                   {false, false, false, true, false, false});
}

TEST_F(CastingTest, StrongByteArray) {
  CheckSourceTypes(*isolate()->factory()->NewByteArray(1),
                   {false, false, true, false, false, false});
}

TEST_F(CastingTest, WeakByteArray) {
  CheckSourceTypes(MakeWeak(*isolate()->factory()->NewByteArray(1)),
                   {false, false, false, false, true, false});
}

TEST_F(CastingTest, Smi) {
  for (int n : {0, 1, -1}) {
    CheckSourceTypes(Smi::FromInt(n),
                     {true, false, false, false, false, false});
  }
}

TEST_F(CastingTest, Cleared) {
  CheckSourceTypes(kClearedWeakValue,
                   {false, false, false, false, false, true});
}

TEST_F(CastingTest, Number) {
  Tagged<HeapNumber> number = *isolate()->factory()->NewHeapNumber(1.5);
  CheckCast<Number>(number, true);
  CheckCast<Number>(Tagged<Object>(number), true);
  CheckCast<Number>(Tagged<MaybeObject>(number), true);
  CheckCast<Number>(MakeWeak(number), false);
  CheckCast<Number>(Tagged<MaybeObject>(MakeWeak(number)), false);
}

// FieldType's predicate depends on the Smi value, so it detects Smi payload
// corruption when stripping strength from maybe-weak sources (Any() is Smi(1),
// whose payload bit overlaps the weak tag bit under 31-bit Smi tagging).
TEST_F(CastingTest, FieldType) {
  Tagged<Map> map = ReadOnlyRoots(isolate()).fixed_array_map();
  auto check = [](Tagged<MaybeObject> value, bool expected) {
    CheckCast<FieldType>(value, expected);
    CheckCast<FieldType>(Cast<UnionOf<Smi, Map, Weak<Map>>>(value), expected);
  };
  check(FieldType::Any(), true);
  check(FieldType::None(), true);
  check(Smi::zero(), false);
  check(Smi::FromInt(3), false);
  check(map, true);
  check(MakeWeak(map), false);
}

TEST_F(CastingTest, JSUnionPredicates) {
  auto check = [](Tagged<Object> value) {
    if (IsHeapObject(value) && IsInaccessible(Cast<HeapObject>(value))) return;
    bool primitive = IsPrimitive(value);
    bool js_any = primitive || IsJSReceiver(value);
    CheckCast<JSPrimitive>(value, primitive);
    CheckCast<JSAny>(value, js_any);
    CheckCast<JSPrimitive>(Tagged<MaybeObject>(value), primitive);
    CheckCast<JSAny>(Tagged<MaybeObject>(value), js_any);
    if (IsHeapObject(value)) {
      auto weak = MakeWeak(Cast<HeapObject>(value));
      CheckCast<JSPrimitive>(weak, false);
      CheckCast<JSAny>(weak, false);
      CheckCast<JSPrimitive>(Tagged<MaybeObject>(weak), false);
      CheckCast<JSAny>(Tagged<MaybeObject>(weak), false);
    }
  };
  ReadOnlyRoots roots(isolate());
#define CHECK_ROOT(type, name, CamelName) \
  {                                       \
    SCOPED_TRACE(#name);                  \
    check(roots.name());                  \
  }
  READ_ONLY_ROOT_LIST(CHECK_ROOT)
#undef CHECK_ROOT
  check(*isolate()->factory()->NewJSObject(isolate()->object_function()));
}

}  // namespace
}  // namespace v8::internal
