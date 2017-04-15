// Copyright 2015 the V8 project authors. All rights reserved.
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

using namespace v8::base;
using namespace v8::internal;


static const int kMaxInobjectProperties =
    (JSObject::kMaxInstanceSize - JSObject::kHeaderSize) >> kPointerSizeLog2;


template <typename T>
static Handle<T> OpenHandle(v8::Local<v8::Value> value) {
  Handle<Object> obj = v8::Utils::OpenHandle(*value);
  return Handle<T>::cast(obj);
}


static inline v8::Local<v8::Value> Run(v8::Local<v8::Script> script) {
  v8::Local<v8::Value> result;
  if (script->Run(v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocal(&result)) {
    return result;
  }
  return v8::Local<v8::Value>();
}


template <typename T = Object>
Handle<T> GetGlobal(const char* name) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Handle<String> str_name = factory->InternalizeUtf8String(name);

  Handle<Object> value =
      Object::GetProperty(isolate->global_object(), str_name).ToHandleChecked();
  return Handle<T>::cast(value);
}


template <typename T = Object>
Handle<T> GetLexical(const char* name) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<String> str_name = factory->InternalizeUtf8String(name);
  Handle<ScriptContextTable> script_contexts(
      isolate->native_context()->script_context_table());

  ScriptContextTable::LookupResult lookup_result;
  if (ScriptContextTable::Lookup(script_contexts, str_name, &lookup_result)) {
    Handle<Object> result =
        FixedArray::get(*ScriptContextTable::GetContext(
                            script_contexts, lookup_result.context_index),
                        lookup_result.slot_index, isolate);
    return Handle<T>::cast(result);
  }
  return Handle<T>();
}


template <typename T = Object>
Handle<T> GetLexical(const std::string& name) {
  return GetLexical<T>(name.c_str());
}


template <typename T>
static inline Handle<T> Run(v8::Local<v8::Script> script) {
  return OpenHandle<T>(Run(script));
}


template <typename T>
static inline Handle<T> CompileRun(const char* script) {
  return OpenHandle<T>(CompileRun(script));
}


static Object* GetFieldValue(JSObject* obj, int property_index) {
  FieldIndex index = FieldIndex::ForPropertyIndex(obj->map(), property_index);
  return obj->RawFastPropertyAt(index);
}


static double GetDoubleFieldValue(JSObject* obj, FieldIndex field_index) {
  if (obj->IsUnboxedDoubleField(field_index)) {
    return obj->RawFastDoublePropertyAt(field_index);
  } else {
    Object* value = obj->RawFastPropertyAt(field_index);
    CHECK(value->IsMutableHeapNumber());
    return HeapNumber::cast(value)->value();
  }
}


static double GetDoubleFieldValue(JSObject* obj, int property_index) {
  FieldIndex index = FieldIndex::ForPropertyIndex(obj->map(), property_index);
  return GetDoubleFieldValue(obj, index);
}


bool IsObjectShrinkable(JSObject* obj) {
  Handle<Map> filler_map =
      CcTest::i_isolate()->factory()->one_pointer_filler_map();

  int inobject_properties = obj->map()->GetInObjectProperties();
  int unused = obj->map()->unused_property_fields();
  if (unused == 0) return false;

  for (int i = inobject_properties - unused; i < inobject_properties; i++) {
    if (*filler_map != GetFieldValue(obj, i)) {
      return false;
    }
  }
  return true;
}


TEST(JSObjectBasic) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  const char* source =
      "function A() {"
      "  this.a = 42;"
      "  this.d = 4.2;"
      "  this.o = this;"
      "}";
  CompileRun(source);

  Handle<JSFunction> func = GetGlobal<JSFunction>("A");

  // Zero instances were created so far.
  CHECK(!func->has_initial_map());

  v8::Local<v8::Script> new_A_script = v8_compile("new A();");

  Handle<JSObject> obj = Run<JSObject>(new_A_script);

  CHECK(func->has_initial_map());
  Handle<Map> initial_map(func->initial_map());

  // One instance created.
  CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
           initial_map->construction_counter());
  CHECK(initial_map->IsInobjectSlackTrackingInProgress());

  // There must be at least some slack.
  CHECK_LT(5, obj->map()->GetInObjectProperties());
  CHECK_EQ(Smi::FromInt(42), GetFieldValue(*obj, 0));
  CHECK_EQ(4.2, GetDoubleFieldValue(*obj, 1));
  CHECK_EQ(*obj, GetFieldValue(*obj, 2));
  CHECK(IsObjectShrinkable(*obj));

  // Create several objects to complete the tracking.
  for (int i = 1; i < Map::kGenerousAllocationCount; i++) {
    CHECK(initial_map->IsInobjectSlackTrackingInProgress());
    Handle<JSObject> tmp = Run<JSObject>(new_A_script);
    CHECK_EQ(initial_map->IsInobjectSlackTrackingInProgress(),
             IsObjectShrinkable(*tmp));
  }
  CHECK(!initial_map->IsInobjectSlackTrackingInProgress());
  CHECK(!IsObjectShrinkable(*obj));

  // No slack left.
  CHECK_EQ(3, obj->map()->GetInObjectProperties());
}


TEST(JSObjectBasicNoInlineNew) {
  FLAG_inline_new = false;
  TestJSObjectBasic();
}


TEST(JSObjectComplex) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  const char* source =
      "function A(n) {"
      "  if (n > 0) this.a = 42;"
      "  if (n > 1) this.d = 4.2;"
      "  if (n > 2) this.o1 = this;"
      "  if (n > 3) this.o2 = this;"
      "  if (n > 4) this.o3 = this;"
      "  if (n > 5) this.o4 = this;"
      "}";
  CompileRun(source);

  Handle<JSFunction> func = GetGlobal<JSFunction>("A");

  // Zero instances were created so far.
  CHECK(!func->has_initial_map());

  Handle<JSObject> obj1 = CompileRun<JSObject>("new A(1);");
  Handle<JSObject> obj3 = CompileRun<JSObject>("new A(3);");
  Handle<JSObject> obj5 = CompileRun<JSObject>("new A(5);");

  CHECK(func->has_initial_map());
  Handle<Map> initial_map(func->initial_map());

  // Three instances created.
  CHECK_EQ(Map::kSlackTrackingCounterStart - 3,
           initial_map->construction_counter());
  CHECK(initial_map->IsInobjectSlackTrackingInProgress());

  // There must be at least some slack.
  CHECK_LT(5, obj3->map()->GetInObjectProperties());
  CHECK_EQ(Smi::FromInt(42), GetFieldValue(*obj3, 0));
  CHECK_EQ(4.2, GetDoubleFieldValue(*obj3, 1));
  CHECK_EQ(*obj3, GetFieldValue(*obj3, 2));
  CHECK(IsObjectShrinkable(*obj1));
  CHECK(IsObjectShrinkable(*obj3));
  CHECK(IsObjectShrinkable(*obj5));

  // Create several objects to complete the tracking.
  for (int i = 3; i < Map::kGenerousAllocationCount; i++) {
    CHECK(initial_map->IsInobjectSlackTrackingInProgress());
    CompileRun("new A(3);");
  }
  CHECK(!initial_map->IsInobjectSlackTrackingInProgress());

  // obj1 and obj2 stays shrinkable because we don't clear unused fields.
  CHECK(IsObjectShrinkable(*obj1));
  CHECK(IsObjectShrinkable(*obj3));
  CHECK(!IsObjectShrinkable(*obj5));

  CHECK_EQ(5, obj1->map()->GetInObjectProperties());
  CHECK_EQ(4, obj1->map()->unused_property_fields());

  CHECK_EQ(5, obj3->map()->GetInObjectProperties());
  CHECK_EQ(2, obj3->map()->unused_property_fields());

  CHECK_EQ(5, obj5->map()->GetInObjectProperties());
  CHECK_EQ(0, obj5->map()->unused_property_fields());

  // Since slack tracking is complete, the new objects should not be shrinkable.
  obj1 = CompileRun<JSObject>("new A(1);");
  obj3 = CompileRun<JSObject>("new A(3);");
  obj5 = CompileRun<JSObject>("new A(5);");

  CHECK(!IsObjectShrinkable(*obj1));
  CHECK(!IsObjectShrinkable(*obj3));
  CHECK(!IsObjectShrinkable(*obj5));
}


TEST(JSObjectComplexNoInlineNew) {
  FLAG_inline_new = false;
  TestJSObjectComplex();
}


TEST(JSGeneratorObjectBasic) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  const char* source =
      "function* A() {"
      "  var i = 0;"
      "  while(true) {"
      "    yield i++;"
      "  }"
      "};"
      "function CreateGenerator() {"
      "  var o = A();"
      "  o.a = 42;"
      "  o.d = 4.2;"
      "  o.o = o;"
      "  return o;"
      "}";
  CompileRun(source);

  Handle<JSFunction> func = GetGlobal<JSFunction>("A");

  // Zero instances were created so far.
  CHECK(!func->has_initial_map());

  v8::Local<v8::Script> new_A_script = v8_compile("CreateGenerator();");

  Handle<JSObject> obj = Run<JSObject>(new_A_script);

  CHECK(func->has_initial_map());
  Handle<Map> initial_map(func->initial_map());

  // One instance created.
  CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
           initial_map->construction_counter());
  CHECK(initial_map->IsInobjectSlackTrackingInProgress());

  // There must be at least some slack.
  CHECK_LT(5, obj->map()->GetInObjectProperties());
  CHECK_EQ(Smi::FromInt(42), GetFieldValue(*obj, 0));
  CHECK_EQ(4.2, GetDoubleFieldValue(*obj, 1));
  CHECK_EQ(*obj, GetFieldValue(*obj, 2));
  CHECK(IsObjectShrinkable(*obj));

  // Create several objects to complete the tracking.
  for (int i = 1; i < Map::kGenerousAllocationCount; i++) {
    CHECK(initial_map->IsInobjectSlackTrackingInProgress());
    Handle<JSObject> tmp = Run<JSObject>(new_A_script);
    CHECK_EQ(initial_map->IsInobjectSlackTrackingInProgress(),
             IsObjectShrinkable(*tmp));
  }
  CHECK(!initial_map->IsInobjectSlackTrackingInProgress());
  CHECK(!IsObjectShrinkable(*obj));

  // No slack left.
  CHECK_EQ(3, obj->map()->GetInObjectProperties());
}


TEST(JSGeneratorObjectBasicNoInlineNew) {
  FLAG_inline_new = false;
  TestJSGeneratorObjectBasic();
}


TEST(SubclassBasicNoBaseClassInstances) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Check that base class' and subclass' slack tracking do not interfere with
  // each other.
  // In this test we never create base class instances.

  const char* source =
      "'use strict';"
      "class A {"
      "  constructor(...args) {"
      "    this.aa = 42;"
      "    this.ad = 4.2;"
      "    this.ao = this;"
      "  }"
      "};"
      "class B extends A {"
      "  constructor(...args) {"
      "    super(...args);"
      "    this.ba = 142;"
      "    this.bd = 14.2;"
      "    this.bo = this;"
      "  }"
      "};";
  CompileRun(source);

  Handle<JSFunction> a_func = GetLexical<JSFunction>("A");
  Handle<JSFunction> b_func = GetLexical<JSFunction>("B");

  // Zero instances were created so far.
  CHECK(!a_func->has_initial_map());
  CHECK(!b_func->has_initial_map());

  v8::Local<v8::Script> new_B_script = v8_compile("new B();");

  Handle<JSObject> obj = Run<JSObject>(new_B_script);

  CHECK(a_func->has_initial_map());
  Handle<Map> a_initial_map(a_func->initial_map());

  CHECK(b_func->has_initial_map());
  Handle<Map> b_initial_map(b_func->initial_map());

  // Zero instances of A created.
  CHECK_EQ(Map::kSlackTrackingCounterStart,
           a_initial_map->construction_counter());
  CHECK(a_initial_map->IsInobjectSlackTrackingInProgress());

  // One instance of B created.
  CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
           b_initial_map->construction_counter());
  CHECK(b_initial_map->IsInobjectSlackTrackingInProgress());

  // There must be at least some slack.
  CHECK_LT(10, obj->map()->GetInObjectProperties());
  CHECK_EQ(Smi::FromInt(42), GetFieldValue(*obj, 0));
  CHECK_EQ(4.2, GetDoubleFieldValue(*obj, 1));
  CHECK_EQ(*obj, GetFieldValue(*obj, 2));
  CHECK_EQ(Smi::FromInt(142), GetFieldValue(*obj, 3));
  CHECK_EQ(14.2, GetDoubleFieldValue(*obj, 4));
  CHECK_EQ(*obj, GetFieldValue(*obj, 5));
  CHECK(IsObjectShrinkable(*obj));

  // Create several subclass instances to complete the tracking.
  for (int i = 1; i < Map::kGenerousAllocationCount; i++) {
    CHECK(b_initial_map->IsInobjectSlackTrackingInProgress());
    Handle<JSObject> tmp = Run<JSObject>(new_B_script);
    CHECK_EQ(b_initial_map->IsInobjectSlackTrackingInProgress(),
             IsObjectShrinkable(*tmp));
  }
  CHECK(!b_initial_map->IsInobjectSlackTrackingInProgress());
  CHECK(!IsObjectShrinkable(*obj));

  // Zero instances of A created.
  CHECK_EQ(Map::kSlackTrackingCounterStart,
           a_initial_map->construction_counter());
  CHECK(a_initial_map->IsInobjectSlackTrackingInProgress());

  // No slack left.
  CHECK_EQ(6, obj->map()->GetInObjectProperties());
}


TEST(SubclassBasicNoBaseClassInstancesNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassBasicNoBaseClassInstances();
}


TEST(SubclassBasic) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Check that base class' and subclass' slack tracking do not interfere with
  // each other.
  // In this test we first create enough base class instances to complete
  // the slack tracking and then proceed creating subclass instances.

  const char* source =
      "'use strict';"
      "class A {"
      "  constructor(...args) {"
      "    this.aa = 42;"
      "    this.ad = 4.2;"
      "    this.ao = this;"
      "  }"
      "};"
      "class B extends A {"
      "  constructor(...args) {"
      "    super(...args);"
      "    this.ba = 142;"
      "    this.bd = 14.2;"
      "    this.bo = this;"
      "  }"
      "};";
  CompileRun(source);

  Handle<JSFunction> a_func = GetLexical<JSFunction>("A");
  Handle<JSFunction> b_func = GetLexical<JSFunction>("B");

  // Zero instances were created so far.
  CHECK(!a_func->has_initial_map());
  CHECK(!b_func->has_initial_map());

  v8::Local<v8::Script> new_A_script = v8_compile("new A();");
  v8::Local<v8::Script> new_B_script = v8_compile("new B();");

  Handle<JSObject> a_obj = Run<JSObject>(new_A_script);
  Handle<JSObject> b_obj = Run<JSObject>(new_B_script);

  CHECK(a_func->has_initial_map());
  Handle<Map> a_initial_map(a_func->initial_map());

  CHECK(b_func->has_initial_map());
  Handle<Map> b_initial_map(b_func->initial_map());

  // One instance of a base class created.
  CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
           a_initial_map->construction_counter());
  CHECK(a_initial_map->IsInobjectSlackTrackingInProgress());

  // One instance of a subclass created.
  CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
           b_initial_map->construction_counter());
  CHECK(b_initial_map->IsInobjectSlackTrackingInProgress());

  // Create several base class instances to complete the tracking.
  for (int i = 1; i < Map::kGenerousAllocationCount; i++) {
    CHECK(a_initial_map->IsInobjectSlackTrackingInProgress());
    Handle<JSObject> tmp = Run<JSObject>(new_A_script);
    CHECK_EQ(a_initial_map->IsInobjectSlackTrackingInProgress(),
             IsObjectShrinkable(*tmp));
  }
  CHECK(!a_initial_map->IsInobjectSlackTrackingInProgress());
  CHECK(!IsObjectShrinkable(*a_obj));

  // No slack left.
  CHECK_EQ(3, a_obj->map()->GetInObjectProperties());

  // There must be at least some slack.
  CHECK_LT(10, b_obj->map()->GetInObjectProperties());
  CHECK_EQ(Smi::FromInt(42), GetFieldValue(*b_obj, 0));
  CHECK_EQ(4.2, GetDoubleFieldValue(*b_obj, 1));
  CHECK_EQ(*b_obj, GetFieldValue(*b_obj, 2));
  CHECK_EQ(Smi::FromInt(142), GetFieldValue(*b_obj, 3));
  CHECK_EQ(14.2, GetDoubleFieldValue(*b_obj, 4));
  CHECK_EQ(*b_obj, GetFieldValue(*b_obj, 5));
  CHECK(IsObjectShrinkable(*b_obj));

  // Create several subclass instances to complete the tracking.
  for (int i = 1; i < Map::kGenerousAllocationCount; i++) {
    CHECK(b_initial_map->IsInobjectSlackTrackingInProgress());
    Handle<JSObject> tmp = Run<JSObject>(new_B_script);
    CHECK_EQ(b_initial_map->IsInobjectSlackTrackingInProgress(),
             IsObjectShrinkable(*tmp));
  }
  CHECK(!b_initial_map->IsInobjectSlackTrackingInProgress());
  CHECK(!IsObjectShrinkable(*b_obj));

  // No slack left.
  CHECK_EQ(6, b_obj->map()->GetInObjectProperties());
}


TEST(SubclassBasicNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassBasic();
}


// Creates class hierachy of length matching the |hierarchy_desc| length and
// with the number of fields at i'th level equal to |hierarchy_desc[i]|.
static void CreateClassHierarchy(const std::vector<int>& hierarchy_desc) {
  std::ostringstream os;
  os << "'use strict';\n\n";

  int n = static_cast<int>(hierarchy_desc.size());
  for (int cur_class = 0; cur_class < n; cur_class++) {
    os << "class A" << cur_class;
    if (cur_class > 0) {
      os << " extends A" << (cur_class - 1);
    }
    os << " {\n"
          "  constructor(...args) {\n";
    if (cur_class > 0) {
      os << "    super(...args);\n";
    }
    int fields_count = hierarchy_desc[cur_class];
    for (int k = 0; k < fields_count; k++) {
      os << "    this.f" << cur_class << "_" << k << " = " << k << ";\n";
    }
    os << "  }\n"
          "};\n\n";
  }
  CompileRun(os.str().c_str());
}


static std::string GetClassName(int class_index) {
  std::ostringstream os;
  os << "A" << class_index;
  return os.str();
}


static v8::Local<v8::Script> GetNewObjectScript(const std::string& class_name) {
  std::ostringstream os;
  os << "new " << class_name << "();";
  return v8_compile(os.str().c_str());
}


// Test that in-object slack tracking works as expected for first |n| classes
// in the hierarchy.
// This test works only for if the total property count is less than maximum
// in-object properties count.
static void TestClassHierarchy(const std::vector<int>& hierarchy_desc, int n) {
  int fields_count = 0;
  for (int cur_class = 0; cur_class < n; cur_class++) {
    std::string class_name = GetClassName(cur_class);
    int fields_count_at_current_level = hierarchy_desc[cur_class];
    fields_count += fields_count_at_current_level;

    // This test is not suitable for in-object properties count overflow case.
    CHECK_LT(fields_count, kMaxInobjectProperties);

    // Create |class_name| objects and check slack tracking.
    v8::Local<v8::Script> new_script = GetNewObjectScript(class_name);

    Handle<JSFunction> func = GetLexical<JSFunction>(class_name);

    Handle<JSObject> obj = Run<JSObject>(new_script);

    CHECK(func->has_initial_map());
    Handle<Map> initial_map(func->initial_map());

    // There must be at least some slack.
    CHECK_LT(fields_count, obj->map()->GetInObjectProperties());

    // One instance was created.
    CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
             initial_map->construction_counter());
    CHECK(initial_map->IsInobjectSlackTrackingInProgress());

    // Create several instances to complete the tracking.
    for (int i = 1; i < Map::kGenerousAllocationCount; i++) {
      CHECK(initial_map->IsInobjectSlackTrackingInProgress());
      Handle<JSObject> tmp = Run<JSObject>(new_script);
      CHECK_EQ(initial_map->IsInobjectSlackTrackingInProgress(),
               IsObjectShrinkable(*tmp));
      CHECK_EQ(Map::kSlackTrackingCounterStart - i - 1,
               initial_map->construction_counter());
    }
    CHECK(!initial_map->IsInobjectSlackTrackingInProgress());
    CHECK(!IsObjectShrinkable(*obj));

    // No slack left.
    CHECK_EQ(fields_count, obj->map()->GetInObjectProperties());
  }
}


static void TestSubclassChain(const std::vector<int>& hierarchy_desc) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  CreateClassHierarchy(hierarchy_desc);
  TestClassHierarchy(hierarchy_desc, static_cast<int>(hierarchy_desc.size()));
}


TEST(LongSubclassChain1) {
  std::vector<int> hierarchy_desc;
  for (int i = 0; i < 7; i++) {
    hierarchy_desc.push_back(i * 10);
  }
  TestSubclassChain(hierarchy_desc);
}


TEST(LongSubclassChain2) {
  std::vector<int> hierarchy_desc;
  hierarchy_desc.push_back(10);
  for (int i = 0; i < 42; i++) {
    hierarchy_desc.push_back(0);
  }
  hierarchy_desc.push_back(230);
  TestSubclassChain(hierarchy_desc);
}


TEST(LongSubclassChain3) {
  std::vector<int> hierarchy_desc;
  for (int i = 0; i < 42; i++) {
    hierarchy_desc.push_back(5);
  }
  TestSubclassChain(hierarchy_desc);
}


TEST(InobjectPropetiesCountOverflowInSubclass) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  std::vector<int> hierarchy_desc;
  const int kNoOverflowCount = 5;
  for (int i = 0; i < kNoOverflowCount; i++) {
    hierarchy_desc.push_back(50);
  }
  // In this class we are going to have properties in the backing store.
  hierarchy_desc.push_back(100);

  CreateClassHierarchy(hierarchy_desc);

  // For the last class in the hierarchy we need different checks.
  {
    int cur_class = kNoOverflowCount;
    std::string class_name = GetClassName(cur_class);

    // Create |class_name| objects and check slack tracking.
    v8::Local<v8::Script> new_script = GetNewObjectScript(class_name);

    Handle<JSFunction> func = GetLexical<JSFunction>(class_name);

    Handle<JSObject> obj = Run<JSObject>(new_script);

    CHECK(func->has_initial_map());
    Handle<Map> initial_map(func->initial_map());

    // There must be no slack left.
    CHECK_EQ(JSObject::kMaxInstanceSize, obj->map()->instance_size());
    CHECK_EQ(kMaxInobjectProperties, obj->map()->GetInObjectProperties());

    // One instance was created.
    CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
             initial_map->construction_counter());
    CHECK(initial_map->IsInobjectSlackTrackingInProgress());

    // Create several instances to complete the tracking.
    for (int i = 1; i < Map::kGenerousAllocationCount; i++) {
      CHECK(initial_map->IsInobjectSlackTrackingInProgress());
      Handle<JSObject> tmp = Run<JSObject>(new_script);
      CHECK(!IsObjectShrinkable(*tmp));
    }
    CHECK(!initial_map->IsInobjectSlackTrackingInProgress());
    CHECK(!IsObjectShrinkable(*obj));

    // No slack left.
    CHECK_EQ(kMaxInobjectProperties, obj->map()->GetInObjectProperties());
  }

  // The other classes in the hierarchy are not affected.
  TestClassHierarchy(hierarchy_desc, kNoOverflowCount);
}


TEST(SlowModeSubclass) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  std::vector<int> hierarchy_desc;
  const int kNoOverflowCount = 5;
  for (int i = 0; i < kNoOverflowCount; i++) {
    hierarchy_desc.push_back(50);
  }
  // This class should go dictionary mode.
  hierarchy_desc.push_back(1000);

  CreateClassHierarchy(hierarchy_desc);

  // For the last class in the hierarchy we need different checks.
  {
    int cur_class = kNoOverflowCount;
    std::string class_name = GetClassName(cur_class);

    // Create |class_name| objects and check slack tracking.
    v8::Local<v8::Script> new_script = GetNewObjectScript(class_name);

    Handle<JSFunction> func = GetLexical<JSFunction>(class_name);

    Handle<JSObject> obj = Run<JSObject>(new_script);

    CHECK(func->has_initial_map());
    Handle<Map> initial_map(func->initial_map());

    // Object should go dictionary mode.
    CHECK_EQ(JSObject::kHeaderSize, obj->map()->instance_size());
    CHECK(obj->map()->is_dictionary_map());

    // One instance was created.
    CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
             initial_map->construction_counter());
    CHECK(initial_map->IsInobjectSlackTrackingInProgress());

    // Create several instances to complete the tracking.
    for (int i = 1; i < Map::kGenerousAllocationCount; i++) {
      CHECK(initial_map->IsInobjectSlackTrackingInProgress());
      Handle<JSObject> tmp = Run<JSObject>(new_script);
      CHECK(!IsObjectShrinkable(*tmp));
    }
    CHECK(!initial_map->IsInobjectSlackTrackingInProgress());
    CHECK(!IsObjectShrinkable(*obj));

    // Object should stay in dictionary mode.
    CHECK_EQ(JSObject::kHeaderSize, obj->map()->instance_size());
    CHECK(obj->map()->is_dictionary_map());
  }

  // The other classes in the hierarchy are not affected.
  TestClassHierarchy(hierarchy_desc, kNoOverflowCount);
}


static void TestSubclassBuiltin(const char* subclass_name,
                                InstanceType instance_type,
                                const char* builtin_name,
                                const char* ctor_arguments = "",
                                int builtin_properties_count = 0) {
  {
    std::ostringstream os;
    os << "'use strict';\n"
          "class "
       << subclass_name << " extends " << builtin_name
       << " {\n"
          "  constructor(...args) {\n"
          "    super(...args);\n"
          "    this.a = 42;\n"
          "    this.d = 4.2;\n"
          "    this.o = this;\n"
          "  }\n"
          "};\n";
    CompileRun(os.str().c_str());
  }

  Handle<JSFunction> func = GetLexical<JSFunction>(subclass_name);

  // Zero instances were created so far.
  CHECK(!func->has_initial_map());

  v8::Local<v8::Script> new_script;
  {
    std::ostringstream os;
    os << "new " << subclass_name << "(" << ctor_arguments << ");";
    new_script = v8_compile(os.str().c_str());
  }

  Run<JSObject>(new_script);

  CHECK(func->has_initial_map());
  Handle<Map> initial_map(func->initial_map());

  CHECK_EQ(instance_type, initial_map->instance_type());

  // One instance of a subclass created.
  CHECK_EQ(Map::kSlackTrackingCounterStart - 1,
           initial_map->construction_counter());
  CHECK(initial_map->IsInobjectSlackTrackingInProgress());

  // Create two instances in order to ensure that |obj|.o is a data field
  // in case of Function subclassing.
  Handle<JSObject> obj = Run<JSObject>(new_script);

  // Two instances of a subclass created.
  CHECK_EQ(Map::kSlackTrackingCounterStart - 2,
           initial_map->construction_counter());
  CHECK(initial_map->IsInobjectSlackTrackingInProgress());

  // There must be at least some slack.
  CHECK_LT(builtin_properties_count + 5, obj->map()->GetInObjectProperties());
  CHECK_EQ(Smi::FromInt(42), GetFieldValue(*obj, builtin_properties_count + 0));
  CHECK_EQ(4.2, GetDoubleFieldValue(*obj, builtin_properties_count + 1));
  CHECK_EQ(*obj, GetFieldValue(*obj, builtin_properties_count + 2));
  CHECK(IsObjectShrinkable(*obj));

  // Create several subclass instances to complete the tracking.
  for (int i = 2; i < Map::kGenerousAllocationCount; i++) {
    CHECK(initial_map->IsInobjectSlackTrackingInProgress());
    Handle<JSObject> tmp = Run<JSObject>(new_script);
    CHECK_EQ(initial_map->IsInobjectSlackTrackingInProgress(),
             IsObjectShrinkable(*tmp));
  }
  CHECK(!initial_map->IsInobjectSlackTrackingInProgress());
  CHECK(!IsObjectShrinkable(*obj));

  // No slack left.
  CHECK_EQ(builtin_properties_count + 3, obj->map()->GetInObjectProperties());

  CHECK_EQ(instance_type, obj->map()->instance_type());
}


TEST(SubclassObjectBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_OBJECT_TYPE, "Object", "true");
  TestSubclassBuiltin("A2", JS_OBJECT_TYPE, "Object", "42");
  TestSubclassBuiltin("A3", JS_OBJECT_TYPE, "Object", "'some string'");
}


TEST(SubclassObjectBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassObjectBuiltin();
}


TEST(SubclassFunctionBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_FUNCTION_TYPE, "Function", "'return 153;'");
  TestSubclassBuiltin("A2", JS_FUNCTION_TYPE, "Function", "'this.a = 44;'");
}


TEST(SubclassFunctionBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassFunctionBuiltin();
}


TEST(SubclassBooleanBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_VALUE_TYPE, "Boolean", "true");
  TestSubclassBuiltin("A2", JS_VALUE_TYPE, "Boolean", "false");
}


TEST(SubclassBooleanBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassBooleanBuiltin();
}


TEST(SubclassErrorBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  const int first_field = 2;
  TestSubclassBuiltin("A1", JS_ERROR_TYPE, "Error", "'err'", first_field);
  TestSubclassBuiltin("A2", JS_ERROR_TYPE, "EvalError", "'err'", first_field);
  TestSubclassBuiltin("A3", JS_ERROR_TYPE, "RangeError", "'err'", first_field);
  TestSubclassBuiltin("A4", JS_ERROR_TYPE, "ReferenceError", "'err'",
                      first_field);
  TestSubclassBuiltin("A5", JS_ERROR_TYPE, "SyntaxError", "'err'", first_field);
  TestSubclassBuiltin("A6", JS_ERROR_TYPE, "TypeError", "'err'", first_field);
  TestSubclassBuiltin("A7", JS_ERROR_TYPE, "URIError", "'err'", first_field);
}


TEST(SubclassErrorBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassErrorBuiltin();
}


TEST(SubclassNumberBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_VALUE_TYPE, "Number", "42");
  TestSubclassBuiltin("A2", JS_VALUE_TYPE, "Number", "4.2");
}


TEST(SubclassNumberBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassNumberBuiltin();
}


TEST(SubclassDateBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_DATE_TYPE, "Date", "123456789");
}


TEST(SubclassDateBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassDateBuiltin();
}


TEST(SubclassStringBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_VALUE_TYPE, "String", "'some string'");
  TestSubclassBuiltin("A2", JS_VALUE_TYPE, "String", "");
}


TEST(SubclassStringBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassStringBuiltin();
}


TEST(SubclassRegExpBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  const int first_field = 1;
  TestSubclassBuiltin("A1", JS_REGEXP_TYPE, "RegExp", "'o(..)h', 'g'",
                      first_field);
}


TEST(SubclassRegExpBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassRegExpBuiltin();
}


TEST(SubclassArrayBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_ARRAY_TYPE, "Array", "42");
}


TEST(SubclassArrayBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassArrayBuiltin();
}


TEST(SubclassTypedArrayBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

#define TYPED_ARRAY_TEST(Type, type, TYPE, elementType, size) \
  TestSubclassBuiltin("A" #Type, JS_TYPED_ARRAY_TYPE, #Type "Array", "42");

  TYPED_ARRAYS(TYPED_ARRAY_TEST)

#undef TYPED_ARRAY_TEST
}


TEST(SubclassTypedArrayBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassTypedArrayBuiltin();
}


TEST(SubclassCollectionBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_SET_TYPE, "Set", "");
  TestSubclassBuiltin("A2", JS_MAP_TYPE, "Map", "");
  TestSubclassBuiltin("A3", JS_WEAK_SET_TYPE, "WeakSet", "");
  TestSubclassBuiltin("A4", JS_WEAK_MAP_TYPE, "WeakMap", "");
}


TEST(SubclassCollectionBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassCollectionBuiltin();
}


TEST(SubclassArrayBufferBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_ARRAY_BUFFER_TYPE, "ArrayBuffer", "42");
  TestSubclassBuiltin("A2", JS_DATA_VIEW_TYPE, "DataView",
                      "new ArrayBuffer(42)");
}


TEST(SubclassArrayBufferBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassArrayBufferBuiltin();
}


TEST(SubclassPromiseBuiltin) {
  // Avoid eventual completion of in-object slack tracking.
  FLAG_inline_construct = false;
  FLAG_always_opt = false;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  TestSubclassBuiltin("A1", JS_PROMISE_TYPE, "Promise",
                      "function(resolve, reject) { resolve('ok'); }");
}


TEST(SubclassPromiseBuiltinNoInlineNew) {
  FLAG_inline_new = false;
  TestSubclassPromiseBuiltin();
}
