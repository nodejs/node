// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <sstream>
#include <utility>

#include "src/init/v8.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/ordered-hash-table.h"
#include "src/third_party/siphash/halfsiphash.h"
#include "src/utils/utils.h"

#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

int AddToSetAndGetHash(Isolate* isolate, Handle<JSObject> obj,
                       bool has_fast_properties) {
  CHECK_EQ(has_fast_properties, obj->HasFastProperties());
  CHECK_EQ(ReadOnlyRoots(isolate).undefined_value(), obj->GetHash());
  Handle<OrderedHashSet> set = isolate->factory()->NewOrderedHashSet();
  OrderedHashSet::Add(isolate, set, obj);
  CHECK_EQ(has_fast_properties, obj->HasFastProperties());
  return Smi::ToInt(obj->GetHash());
}

int GetPropertyDictionaryHash(Handle<JSObject> obj) {
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    return obj->property_dictionary_swiss().Hash();
  } else {
    return obj->property_dictionary().Hash();
  }
}

int GetPropertyDictionaryLength(Handle<JSObject> obj) {
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    return obj->property_dictionary_swiss().Capacity();
  } else {
    return obj->property_dictionary().length();
  }
}

void CheckIsDictionaryModeObject(Handle<JSObject> obj) {
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    CHECK(obj->raw_properties_or_hash().IsSwissNameDictionary());
  } else {
    CHECK(obj->raw_properties_or_hash().IsNameDictionary());
  }
}

void CheckFastObject(Handle<JSObject> obj, int hash) {
  CHECK(obj->HasFastProperties());
  CHECK(obj->raw_properties_or_hash().IsPropertyArray());
  CHECK_EQ(Smi::FromInt(hash), obj->GetHash());
  CHECK_EQ(hash, obj->property_array().Hash());
}

void CheckDictionaryObject(Handle<JSObject> obj, int hash) {
  CHECK(!obj->HasFastProperties());
  CheckIsDictionaryModeObject(obj);
  CHECK_EQ(Smi::FromInt(hash), obj->GetHash());
  CHECK_EQ(hash, GetPropertyDictionaryHash(obj));
}

TEST(AddHashCodeToFastObjectWithoutProperties) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<JSObject> obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  CHECK(obj->HasFastProperties());

  int hash = AddToSetAndGetHash(isolate, obj, true);
  CHECK_EQ(Smi::FromInt(hash), obj->raw_properties_or_hash());
}

TEST(AddHashCodeToFastObjectWithInObjectProperties) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const char* source = " var x = { a: 1};";
  CompileRun(source);

  Handle<JSObject> obj = GetGlobal<JSObject>("x");
  CHECK_EQ(ReadOnlyRoots(isolate).empty_fixed_array(),
           obj->raw_properties_or_hash());

  int hash = AddToSetAndGetHash(isolate, obj, true);
  CHECK_EQ(Smi::FromInt(hash), obj->raw_properties_or_hash());
}

TEST(AddHashCodeToFastObjectWithPropertiesArray) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const char* source =
      " var x = {}; "
      " x.a = 1; x.b = 2; x.c = 3; x.d = 4; x.e = 5; ";
  CompileRun(source);

  Handle<JSObject> obj = GetGlobal<JSObject>("x");
  CHECK(obj->HasFastProperties());

  int hash = AddToSetAndGetHash(isolate, obj, true);
  CheckFastObject(obj, hash);
}

TEST(AddHashCodeToSlowObject) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<JSObject> obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  CHECK(obj->HasFastProperties());
  JSObject::NormalizeProperties(isolate, obj, CLEAR_INOBJECT_PROPERTIES, 0,
                                "cctest/test-hashcode");

  CheckIsDictionaryModeObject(obj);

  int hash = AddToSetAndGetHash(isolate, obj, false);
  CheckDictionaryObject(obj, hash);
}

TEST(TransitionFastWithInObjectToFastWithPropertyArray) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const char* source =
      " var x = { };"
      " x.a = 1; x.b = 2; x.c = 3; x.d = 4;";
  CompileRun(source);

  Handle<JSObject> obj = GetGlobal<JSObject>("x");
  CHECK(obj->HasFastProperties());

  int hash = AddToSetAndGetHash(isolate, obj, true);
  CHECK_EQ(Smi::FromInt(hash), obj->raw_properties_or_hash());

  int length = obj->property_array().length();
  CompileRun("x.e = 5;");
  CHECK(obj->property_array().length() > length);
  CheckFastObject(obj, hash);
}

TEST(TransitionFastWithPropertyArray) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const char* source =
      " var x = { };"
      " x.a = 1; x.b = 2; x.c = 3; x.d = 4; x.e = 5; ";
  CompileRun(source);

  Handle<JSObject> obj = GetGlobal<JSObject>("x");
  CHECK(obj->raw_properties_or_hash().IsPropertyArray());

  int hash = AddToSetAndGetHash(isolate, obj, true);
  CHECK_EQ(hash, obj->property_array().Hash());

  int length = obj->property_array().length();
  CompileRun("x.f = 2; x.g = 5; x.h = 2");
  CHECK(obj->property_array().length() > length);
  CheckFastObject(obj, hash);
}

TEST(TransitionFastWithPropertyArrayToSlow) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const char* source =
      " var x = { };"
      " x.a = 1; x.b = 2; x.c = 3; x.d = 4; x.e = 5; ";
  CompileRun(source);

  Handle<JSObject> obj = GetGlobal<JSObject>("x");
  CHECK(obj->raw_properties_or_hash().IsPropertyArray());

  int hash = AddToSetAndGetHash(isolate, obj, true);
  CHECK(obj->raw_properties_or_hash().IsPropertyArray());
  CHECK_EQ(hash, obj->property_array().Hash());

  JSObject::NormalizeProperties(isolate, obj, KEEP_INOBJECT_PROPERTIES, 0,
                                "cctest/test-hashcode");
  CheckDictionaryObject(obj, hash);
}

TEST(TransitionSlowToSlow) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const char* source = " var x = {}; ";
  CompileRun(source);

  Handle<JSObject> obj = GetGlobal<JSObject>("x");
  JSObject::NormalizeProperties(isolate, obj, CLEAR_INOBJECT_PROPERTIES, 0,
                                "cctest/test-hashcode");
  CheckIsDictionaryModeObject(obj);

  int hash = AddToSetAndGetHash(isolate, obj, false);
  CHECK_EQ(hash, GetPropertyDictionaryHash(obj));

  int length = GetPropertyDictionaryLength(obj);
  CompileRun("for(var i = 0; i < 10; i++) { x['f'+i] = i };");
  CHECK(GetPropertyDictionaryLength(obj) > length);
  CheckDictionaryObject(obj, hash);
}

TEST(TransitionSlowToFastWithoutProperties) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<JSObject> obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  JSObject::NormalizeProperties(isolate, obj, CLEAR_INOBJECT_PROPERTIES, 0,
                                "cctest/test-hashcode");
  CheckIsDictionaryModeObject(obj);

  int hash = AddToSetAndGetHash(isolate, obj, false);
  CHECK_EQ(hash, GetPropertyDictionaryHash(obj));

  JSObject::MigrateSlowToFast(obj, 0, "cctest/test-hashcode");
  CHECK_EQ(Smi::FromInt(hash), obj->GetHash());
}

TEST(TransitionSlowToFastWithPropertyArray) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const char* source =
      " var x = Object.create(null); "
      " for(var i = 0; i < 10; i++) { x['f'+i] = i }; ";
  CompileRun(source);

  Handle<JSObject> obj = GetGlobal<JSObject>("x");
  CheckIsDictionaryModeObject(obj);

  int hash = AddToSetAndGetHash(isolate, obj, false);
  CHECK_EQ(hash, GetPropertyDictionaryHash(obj));

  JSObject::MigrateSlowToFast(obj, 0, "cctest/test-hashcode");
  CheckFastObject(obj, hash);
}

namespace {

using HashFunction = uint32_t (*)(uint32_t key, uint64_t seed);

void TestIntegerHashQuality(const int samples_log2, int num_buckets_log2,
                            uint64_t seed, double max_var,
                            HashFunction hash_function) {
  int samples = 1 << samples_log2;
  int num_buckets = 1 << num_buckets_log2;
  int mean = samples / num_buckets;
  int* buckets = new int[num_buckets];

  for (int i = 0; i < num_buckets; i++) buckets[i] = 0;

  for (int i = 0; i < samples; i++) {
    uint32_t hash = hash_function(i, seed);
    buckets[hash % num_buckets]++;
  }

  int sum_deviation = 0;
  for (int i = 0; i < num_buckets; i++) {
    int deviation = abs(buckets[i] - mean);
    sum_deviation += deviation * deviation;
  }
  delete[] buckets;

  double variation_coefficient = sqrt(sum_deviation * 1.0 / num_buckets) / mean;

  printf("samples: 1 << %2d, buckets: 1 << %2d, var_coeff: %0.3f\n",
         samples_log2, num_buckets_log2, variation_coefficient);
  CHECK_LT(variation_coefficient, max_var);
}
uint32_t HalfSipHash(uint32_t key, uint64_t seed) {
  return halfsiphash(key, seed);
}

uint32_t JenkinsHash(uint32_t key, uint64_t seed) {
  return ComputeLongHash(static_cast<uint64_t>(key) ^ seed);
}

uint32_t DefaultHash(uint32_t key, uint64_t seed) {
  return ComputeSeededHash(key, seed);
}
}  // anonymous namespace

void TestIntegerHashQuality(HashFunction hash_function) {
  TestIntegerHashQuality(17, 13, 0x123456789ABCDEFU, 0.4, hash_function);
  TestIntegerHashQuality(16, 12, 0x123456789ABCDEFU, 0.4, hash_function);
  TestIntegerHashQuality(15, 11, 0xFEDCBA987654321U, 0.4, hash_function);
  TestIntegerHashQuality(14, 10, 0xFEDCBA987654321U, 0.4, hash_function);
  TestIntegerHashQuality(13, 9, 1, 0.4, hash_function);
  TestIntegerHashQuality(12, 8, 1, 0.4, hash_function);

  TestIntegerHashQuality(17, 10, 0x123456789ABCDEFU, 0.2, hash_function);
  TestIntegerHashQuality(16, 9, 0x123456789ABCDEFU, 0.2, hash_function);
  TestIntegerHashQuality(15, 8, 0xFEDCBA987654321U, 0.2, hash_function);
  TestIntegerHashQuality(14, 7, 0xFEDCBA987654321U, 0.2, hash_function);
  TestIntegerHashQuality(13, 6, 1, 0.2, hash_function);
  TestIntegerHashQuality(12, 5, 1, 0.2, hash_function);
}

TEST(HalfSipHashQuality) { TestIntegerHashQuality(HalfSipHash); }

TEST(JenkinsHashQuality) { TestIntegerHashQuality(JenkinsHash); }

TEST(DefaultHashQuality) { TestIntegerHashQuality(DefaultHash); }

}  // namespace internal
}  // namespace v8
