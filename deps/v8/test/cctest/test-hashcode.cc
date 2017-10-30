// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <sstream>
#include <utility>

#include "src/api.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/v8.h"

#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

int AddToSetAndGetHash(Isolate* isolate, Handle<JSObject> obj,
                       bool has_fast_properties) {
  CHECK_EQ(has_fast_properties, obj->HasFastProperties());
  CHECK_EQ(isolate->heap()->undefined_value(), obj->GetHash());
  Handle<OrderedHashSet> set = isolate->factory()->NewOrderedHashSet();
  OrderedHashSet::Add(set, obj);
  CHECK_EQ(has_fast_properties, obj->HasFastProperties());
  return Smi::ToInt(obj->GetHash());
}

void CheckFastObject(Isolate* isolate, Handle<JSObject> obj, int hash) {
  CHECK(obj->HasFastProperties());
  CHECK(obj->raw_properties_or_hash()->IsPropertyArray());
  CHECK_EQ(Smi::FromInt(hash), obj->GetHash());
  CHECK_EQ(hash, obj->property_array()->Hash());
}

void CheckDictionaryObject(Isolate* isolate, Handle<JSObject> obj, int hash) {
  CHECK(!obj->HasFastProperties());
  CHECK(obj->raw_properties_or_hash()->IsDictionary());
  CHECK_EQ(Smi::FromInt(hash), obj->GetHash());
  CHECK_EQ(hash, obj->property_dictionary()->Hash());
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
  CHECK_EQ(isolate->heap()->empty_fixed_array(), obj->raw_properties_or_hash());

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
  CheckFastObject(isolate, obj, hash);
}

TEST(AddHashCodeToSlowObject) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<JSObject> obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  CHECK(obj->HasFastProperties());
  JSObject::NormalizeProperties(obj, CLEAR_INOBJECT_PROPERTIES, 0,
                                "cctest/test-hashcode");
  CHECK(obj->raw_properties_or_hash()->IsDictionary());

  int hash = AddToSetAndGetHash(isolate, obj, false);
  CheckDictionaryObject(isolate, obj, hash);
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

  int length = obj->property_array()->length();
  CompileRun("x.e = 5;");
  CHECK(obj->property_array()->length() > length);
  CheckFastObject(isolate, obj, hash);
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
  CHECK(obj->raw_properties_or_hash()->IsPropertyArray());

  int hash = AddToSetAndGetHash(isolate, obj, true);
  CHECK_EQ(hash, obj->property_array()->Hash());

  int length = obj->property_array()->length();
  CompileRun("x.f = 2; x.g = 5; x.h = 2");
  CHECK(obj->property_array()->length() > length);
  CheckFastObject(isolate, obj, hash);
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
  CHECK(obj->raw_properties_or_hash()->IsPropertyArray());

  int hash = AddToSetAndGetHash(isolate, obj, true);
  CHECK(obj->raw_properties_or_hash()->IsPropertyArray());
  CHECK_EQ(hash, obj->property_array()->Hash());

  JSObject::NormalizeProperties(obj, KEEP_INOBJECT_PROPERTIES, 0,
                                "cctest/test-hashcode");
  CheckDictionaryObject(isolate, obj, hash);
}

TEST(TransitionSlowToSlow) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  const char* source = " var x = {}; ";
  CompileRun(source);

  Handle<JSObject> obj = GetGlobal<JSObject>("x");
  JSObject::NormalizeProperties(obj, CLEAR_INOBJECT_PROPERTIES, 0,
                                "cctest/test-hashcode");
  CHECK(obj->raw_properties_or_hash()->IsDictionary());

  int hash = AddToSetAndGetHash(isolate, obj, false);
  CHECK_EQ(hash, obj->property_dictionary()->Hash());

  int length = obj->property_dictionary()->length();
  CompileRun("for(var i = 0; i < 10; i++) { x['f'+i] = i };");
  CHECK(obj->property_dictionary()->length() > length);
  CheckDictionaryObject(isolate, obj, hash);
}

TEST(TransitionSlowToFastWithoutProperties) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<JSObject> obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  JSObject::NormalizeProperties(obj, CLEAR_INOBJECT_PROPERTIES, 0,
                                "cctest/test-hashcode");
  CHECK(obj->raw_properties_or_hash()->IsDictionary());

  int hash = AddToSetAndGetHash(isolate, obj, false);
  CHECK_EQ(hash, obj->property_dictionary()->Hash());

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
  CHECK(obj->raw_properties_or_hash()->IsDictionary());

  int hash = AddToSetAndGetHash(isolate, obj, false);
  CHECK_EQ(hash, obj->property_dictionary()->Hash());

  JSObject::MigrateSlowToFast(obj, 0, "cctest/test-hashcode");
  CheckFastObject(isolate, obj, hash);
}

}  // namespace internal
}  // namespace v8
