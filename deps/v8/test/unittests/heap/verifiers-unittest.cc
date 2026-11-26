// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests check that Torque-generated verifier functions crash the process
// when encountering data that doesn't fit the Torque type definitions.

#include "src/api/api-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/map-inl.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::verifiers_unittest {

class VerifiersTest : public TestWithNativeContext {
 protected:
#ifdef VERIFY_HEAP
  void TestWrongTypeInNormalField(bool should_fail);
  void TestWrongStrongTypeInIndexedStructField(bool should_fail);
  void TestWrongWeakTypeInIndexedStructField(bool should_fail);
  void TestWrongOddball(bool should_fail);
  void TestWrongNumber(bool should_fail);
#endif  // VERIFY_HEAP
};

// Defines a pair of tests with similar code. The goal is to test that a
// specific action causes a failure, but that everything else in the test case
// succeeds. The general pattern should be:
//
// TEST_PAIR(Something) {
//   do_setup_steps_that_always_succeed();
//   if (should_fail) {
//     do_the_step_that_fails();
//   }
//   do_teardown_steps_that_always_succeed();
// }
//
// A corresponding entry in unittests.status specifies that all Fail* tests in
// this file must fail.
#define TEST_PAIR(Name)                              \
  TEST_F(VerifiersTest, Pass##Name) { Name(false); } \
  TEST_F(VerifiersTest, Fail##Name) { Name(true); }  \
  void VerifiersTest::Name(bool should_fail)

#ifdef VERIFY_HEAP

TEST_PAIR(TestWrongTypeInNormalField) {
  HandleScope scope(i_isolate());
  DirectHandle<JSObject> o = RunJS<JSObject>("({a: 3, b: 4})");
  DirectHandle<Object> original_elements(
      TaggedField<Object>::load(*o, offsetof(JSObject, elements_)),
      i_isolate());
  CHECK(IsFixedArrayBase(*original_elements));

  // There must be no GC (and therefore no verifiers running) until we can
  // restore the modified data.
  DisallowGarbageCollection no_gc;

  // Elements must be FixedArrayBase according to the Torque definition, so a
  // JSObject should cause a failure.
  TaggedField<Object>::store(*o, offsetof(JSObject, elements_), *o);
  if (should_fail) {
    o->JSObjectVerify(i_isolate());
  }

  // Put back the original value in case verifiers run on test shutdown.
  TaggedField<Object>::store(*o, offsetof(JSObject, elements_),
                             *original_elements);
}

TEST_PAIR(TestWrongStrongTypeInIndexedStructField) {
  HandleScope scope(i_isolate());
  DirectHandle<Object> o = RunJS("({a: 3, b: 4})");
  DirectHandle<Map> map(Cast<HeapObject>(o)->map(), i_isolate());
  DirectHandle<DescriptorArray> descriptors(map->instance_descriptors(),
                                            i_isolate());
  int offset = DescriptorArray::OffsetOfDescriptorAt(1) +
               DescriptorArray::kEntryKeyOffset;
  DirectHandle<Object> original_key(
      TaggedField<Object>::load(*descriptors, offset), i_isolate());
  CHECK(IsString(*original_key));

  // There must be no GC (and therefore no verifiers running) until we can
  // restore the modified data.
  DisallowGarbageCollection no_gc;

  // Key must be Name|Undefined according to the Torque definition, so a
  // JSObject should cause a failure.
  TaggedField<Object>::store(*descriptors, offset, *o);
  if (should_fail) {
    descriptors->DescriptorArrayEntryTypesVerify(i_isolate());
  }

  // Put back the original value in case verifiers run on test shutdown.
  TaggedField<Object>::store(*descriptors, offset, *original_key);
}

TEST_PAIR(TestWrongWeakTypeInIndexedStructField) {
  HandleScope scope(i_isolate());
  DirectHandle<Object> o = RunJS("({a: 3, b: 4})");
  DirectHandle<Map> map(Cast<HeapObject>(o)->map(), i_isolate());
  DirectHandle<DescriptorArray> descriptors(map->instance_descriptors(),
                                            i_isolate());
  int offset = DescriptorArray::OffsetOfDescriptorAt(0) +
               DescriptorArray::kEntryValueOffset;
  DirectHandle<Object> original_value(
      TaggedField<Object>::load(*descriptors, offset), i_isolate());

  // There must be no GC (and therefore no verifiers running) until we can
  // restore the modified data.
  DisallowGarbageCollection no_gc;

  // Value can be JSAny, which includes JSObject, and it can be Weak<Map>, but
  // it can't be Weak<JSObject>.
  TaggedField<Object>::store(*descriptors, offset, *o);
  descriptors->DescriptorArrayEntryTypesVerify(i_isolate());
  Tagged<MaybeObject> weak = MakeWeak(Cast<HeapObject>(*o));
  TaggedField<MaybeObject>::store(*descriptors, offset, weak);
  if (should_fail) {
    descriptors->DescriptorArrayEntryTypesVerify(i_isolate());
  }

  // Put back the original value in case verifiers run on test shutdown.
  TaggedField<Object>::store(*descriptors, offset, *original_value);
}

TEST_PAIR(TestWrongOddball) {
  HandleScope scope(i_isolate());
  DirectHandle<JSDate> date = RunJS<JSDate>("new Date()");
  DirectHandle<Object> original_hour(
      TaggedField<Object>::load(*date, offsetof(JSDate, hour_)), i_isolate());

  // There must be no GC (and therefore no verifiers running) until we can
  // restore the modified data.
  DisallowGarbageCollection no_gc;

  // Hour is Smi|NaN. Other oddballs like null should cause a failure.
  TaggedField<Object>::store(*date, offsetof(JSDate, hour_),
                             *i_isolate()->factory()->null_value());
  if (should_fail) {
    date->JSDateVerify(i_isolate());
  }

  // Put back the original value in case verifiers run on test shutdown.
  TaggedField<Object>::store(*date, offsetof(JSDate, hour_), *original_hour);
}

TEST_PAIR(TestWrongNumber) {
  HandleScope scope(i_isolate());
  DirectHandle<JSDate> date = RunJS<JSDate>("new Date()");
  DirectHandle<Object> original_hour(
      TaggedField<Object>::load(*date, offsetof(JSDate, hour_)), i_isolate());
  DirectHandle<Object> float_val = RunJS("1.1");

  // There must be no GC (and therefore no verifiers running) until we can
  // restore the modified data.
  DisallowGarbageCollection no_gc;

  // Hour is Smi|NaN. Other doubles like 1.1 should cause a failure.
  TaggedField<Object>::store(*date, offsetof(JSDate, hour_), *float_val);
  if (should_fail) {
    date->JSDateVerify(i_isolate());
  }

  // Put back the original value in case verifiers run on test shutdown.
  TaggedField<Object>::store(*date, offsetof(JSDate, hour_), *original_hour);
}

#endif  // VERIFY_HEAP

#undef TEST_PAIR

}  // namespace v8::internal::verifiers_unittest
