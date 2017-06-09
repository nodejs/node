// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <utility>

#include "test/cctest/test-api.h"

#include "src/v8.h"

#include "src/compilation-cache.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/global-handles.h"
#include "src/ic/stub-cache.h"
#include "src/objects-inl.h"

using namespace v8::internal;


//
// Helper functions.
//

namespace {

Handle<String> MakeString(const char* str) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  return factory->InternalizeUtf8String(str);
}


Handle<String> MakeName(const char* str, int suffix) {
  EmbeddedVector<char, 128> buffer;
  SNPrintF(buffer, "%s%d", str, suffix);
  return MakeString(buffer.start());
}


template <typename T, typename M>
bool EQUALS(Handle<T> left, Handle<M> right) {
  if (*left == *right) return true;
  return JSObject::Equals(Handle<Object>::cast(left),
                          Handle<Object>::cast(right))
      .FromJust();
}


template <typename T, typename M>
bool EQUALS(Handle<T> left, M right) {
  return EQUALS(left, handle(right));
}


template <typename T, typename M>
bool EQUALS(T left, Handle<M> right) {
  return EQUALS(handle(left), right);
}

}  // namespace


//
// Tests
//

TEST(JSObjectAddingProperties) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<FixedArray> empty_fixed_array(factory->empty_fixed_array());
  Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
  Handle<Object> value(Smi::FromInt(42), isolate);

  Handle<JSObject> object = factory->NewJSObject(function);
  Handle<Map> previous_map(object->map());
  CHECK_EQ(FAST_HOLEY_ELEMENTS, previous_map->elements_kind());
  CHECK(EQUALS(object->properties(), empty_fixed_array));
  CHECK(EQUALS(object->elements(), empty_fixed_array));

  // for the default constructor function no in-object properties are reserved
  // hence adding a single property will initialize the property-array
  Handle<String> name = MakeName("property", 0);
  JSObject::DefinePropertyOrElementIgnoreAttributes(object, name, value, NONE)
      .Check();
  CHECK_NE(object->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, object->map()->elements_kind());
  CHECK_LE(1, object->properties()->length());
  CHECK(EQUALS(object->elements(), empty_fixed_array));
}


TEST(JSObjectInObjectAddingProperties) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<FixedArray> empty_fixed_array(factory->empty_fixed_array());
  Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
  int nof_inobject_properties = 10;
  // force in object properties by changing the expected_nof_properties
  function->shared()->set_expected_nof_properties(nof_inobject_properties);
  Handle<Object> value(Smi::FromInt(42), isolate);

  Handle<JSObject> object = factory->NewJSObject(function);
  Handle<Map> previous_map(object->map());
  CHECK_EQ(FAST_HOLEY_ELEMENTS, previous_map->elements_kind());
  CHECK(EQUALS(object->properties(), empty_fixed_array));
  CHECK(EQUALS(object->elements(), empty_fixed_array));

  // we have reserved space for in-object properties, hence adding up to
  // |nof_inobject_properties| will not create a property store
  for (int i = 0; i < nof_inobject_properties; i++) {
    Handle<String> name = MakeName("property", i);
    JSObject::DefinePropertyOrElementIgnoreAttributes(object, name, value, NONE)
        .Check();
  }
  CHECK_NE(object->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, object->map()->elements_kind());
  CHECK(EQUALS(object->properties(), empty_fixed_array));
  CHECK(EQUALS(object->elements(), empty_fixed_array));

  // adding one more property will not fit in the in-object properties, thus
  // creating a property store
  int index = nof_inobject_properties + 1;
  Handle<String> name = MakeName("property", index);
  JSObject::DefinePropertyOrElementIgnoreAttributes(object, name, value, NONE)
      .Check();
  CHECK_NE(object->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, object->map()->elements_kind());
  // there must be at least 1 element in the properies store
  CHECK_LE(1, object->properties()->length());
  CHECK(EQUALS(object->elements(), empty_fixed_array));
}


TEST(JSObjectAddingElements) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<String> name;
  Handle<FixedArray> empty_fixed_array(factory->empty_fixed_array());
  Handle<JSFunction> function = factory->NewFunction(factory->empty_string());
  Handle<Object> value(Smi::FromInt(42), isolate);

  Handle<JSObject> object = factory->NewJSObject(function);
  Handle<Map> previous_map(object->map());
  CHECK_EQ(FAST_HOLEY_ELEMENTS, previous_map->elements_kind());
  CHECK(EQUALS(object->properties(), empty_fixed_array));
  CHECK(EQUALS(object->elements(), empty_fixed_array));

  // Adding an indexed element initializes the elements array
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(object, name, value, NONE)
      .Check();
  // no change in elements_kind => no map transition
  CHECK_EQ(object->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, object->map()->elements_kind());
  CHECK(EQUALS(object->properties(), empty_fixed_array));
  CHECK_LE(1, object->elements()->length());

  // Adding more consecutive elements without a change in the backing store
  int non_dict_backing_store_limit = 100;
  for (int i = 1; i < non_dict_backing_store_limit; i++) {
    name = MakeName("", i);
    JSObject::DefinePropertyOrElementIgnoreAttributes(object, name, value, NONE)
        .Check();
  }
  // no change in elements_kind => no map transition
  CHECK_EQ(object->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, object->map()->elements_kind());
  CHECK(EQUALS(object->properties(), empty_fixed_array));
  CHECK_LE(non_dict_backing_store_limit, object->elements()->length());

  // Adding an element at an very large index causes a change to
  // DICTIONARY_ELEMENTS
  name = MakeString("100000000");
  JSObject::DefinePropertyOrElementIgnoreAttributes(object, name, value, NONE)
      .Check();
  // change in elements_kind => map transition
  CHECK_NE(object->map(), *previous_map);
  CHECK_EQ(DICTIONARY_ELEMENTS, object->map()->elements_kind());
  CHECK(EQUALS(object->properties(), empty_fixed_array));
  CHECK_LE(non_dict_backing_store_limit, object->elements()->length());
}


TEST(JSArrayAddingProperties) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<FixedArray> empty_fixed_array(factory->empty_fixed_array());
  Handle<Object> value(Smi::FromInt(42), isolate);

  Handle<JSArray> array =
      factory->NewJSArray(ElementsKind::FAST_SMI_ELEMENTS, 0, 0);
  Handle<Map> previous_map(array->map());
  CHECK_EQ(FAST_SMI_ELEMENTS, previous_map->elements_kind());
  CHECK(EQUALS(array->properties(), empty_fixed_array));
  CHECK(EQUALS(array->elements(), empty_fixed_array));
  CHECK_EQ(0, Smi::cast(array->length())->value());

  // for the default constructor function no in-object properties are reserved
  // hence adding a single property will initialize the property-array
  Handle<String> name = MakeName("property", 0);
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value, NONE)
      .Check();
  // No change in elements_kind but added property => new map
  CHECK_NE(array->map(), *previous_map);
  CHECK_EQ(FAST_SMI_ELEMENTS, array->map()->elements_kind());
  CHECK_LE(1, array->properties()->length());
  CHECK(EQUALS(array->elements(), empty_fixed_array));
  CHECK_EQ(0, Smi::cast(array->length())->value());
}


TEST(JSArrayAddingElements) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<String> name;
  Handle<FixedArray> empty_fixed_array(factory->empty_fixed_array());
  Handle<Object> value(Smi::FromInt(42), isolate);

  Handle<JSArray> array =
      factory->NewJSArray(ElementsKind::FAST_SMI_ELEMENTS, 0, 0);
  Handle<Map> previous_map(array->map());
  CHECK_EQ(FAST_SMI_ELEMENTS, previous_map->elements_kind());
  CHECK(EQUALS(array->properties(), empty_fixed_array));
  CHECK(EQUALS(array->elements(), empty_fixed_array));
  CHECK_EQ(0, Smi::cast(array->length())->value());

  // Adding an indexed element initializes the elements array
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value, NONE)
      .Check();
  // no change in elements_kind => no map transition
  CHECK_EQ(array->map(), *previous_map);
  CHECK_EQ(FAST_SMI_ELEMENTS, array->map()->elements_kind());
  CHECK(EQUALS(array->properties(), empty_fixed_array));
  CHECK_LE(1, array->elements()->length());
  CHECK_EQ(1, Smi::cast(array->length())->value());

  // Adding more consecutive elements without a change in the backing store
  int non_dict_backing_store_limit = 100;
  for (int i = 1; i < non_dict_backing_store_limit; i++) {
    name = MakeName("", i);
    JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value, NONE)
        .Check();
  }
  // no change in elements_kind => no map transition
  CHECK_EQ(array->map(), *previous_map);
  CHECK_EQ(FAST_SMI_ELEMENTS, array->map()->elements_kind());
  CHECK(EQUALS(array->properties(), empty_fixed_array));
  CHECK_LE(non_dict_backing_store_limit, array->elements()->length());
  CHECK_EQ(non_dict_backing_store_limit, Smi::cast(array->length())->value());

  // Adding an element at an very large index causes a change to
  // DICTIONARY_ELEMENTS
  int index = 100000000;
  name = MakeName("", index);
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value, NONE)
      .Check();
  // change in elements_kind => map transition
  CHECK_NE(array->map(), *previous_map);
  CHECK_EQ(DICTIONARY_ELEMENTS, array->map()->elements_kind());
  CHECK(EQUALS(array->properties(), empty_fixed_array));
  CHECK_LE(non_dict_backing_store_limit, array->elements()->length());
  CHECK_LE(array->elements()->length(), index);
  CHECK_EQ(index + 1, Smi::cast(array->length())->value());
}


TEST(JSArrayAddingElementsGeneralizingiFastSmiElements) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<String> name;
  Handle<Object> value_smi(Smi::FromInt(42), isolate);
  Handle<Object> value_string(MakeString("value"));
  Handle<Object> value_double = factory->NewNumber(3.1415);

  Handle<JSArray> array =
      factory->NewJSArray(ElementsKind::FAST_SMI_ELEMENTS, 0, 0);
  Handle<Map> previous_map(array->map());
  CHECK_EQ(FAST_SMI_ELEMENTS, previous_map->elements_kind());
  CHECK_EQ(0, Smi::cast(array->length())->value());

  // `array[0] = smi_value` doesn't change the elements_kind
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_smi,
                                                    NONE)
      .Check();
  // no change in elements_kind => no map transition
  CHECK_EQ(array->map(), *previous_map);
  CHECK_EQ(FAST_SMI_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(1, Smi::cast(array->length())->value());

  // `delete array[0]` does not alter length, but changes the elments_kind
  name = MakeString("0");
  CHECK(JSReceiver::DeletePropertyOrElement(array, name).FromMaybe(false));
  CHECK_NE(array->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_SMI_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(1, Smi::cast(array->length())->value());
  previous_map = handle(array->map());

  // add a couple of elements again
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_smi,
                                                    NONE)
      .Check();
  name = MakeString("1");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_smi,
                                                    NONE)
      .Check();
  CHECK_EQ(array->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_SMI_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(2, Smi::cast(array->length())->value());

  // Adding a string to the array changes from FAST_HOLEY_SMI to FAST_HOLEY
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_string,
                                                    NONE)
      .Check();
  CHECK_NE(array->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(2, Smi::cast(array->length())->value());
  previous_map = handle(array->map());

  // We don't transition back to FAST_SMI even if we remove the string
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_smi,
                                                    NONE)
      .Check();
  CHECK_EQ(array->map(), *previous_map);

  // Adding a double doesn't change the map either
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_double,
                                                    NONE)
      .Check();
  CHECK_EQ(array->map(), *previous_map);
}


TEST(JSArrayAddingElementsGeneralizingFastElements) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<String> name;
  Handle<Object> value_smi(Smi::FromInt(42), isolate);
  Handle<Object> value_string(MakeString("value"));

  Handle<JSArray> array =
      factory->NewJSArray(ElementsKind::FAST_ELEMENTS, 0, 0);
  Handle<Map> previous_map(array->map());
  CHECK_EQ(FAST_ELEMENTS, previous_map->elements_kind());
  CHECK_EQ(0, Smi::cast(array->length())->value());

  // `array[0] = smi_value` doesn't change the elements_kind
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_smi,
                                                    NONE)
      .Check();
  // no change in elements_kind => no map transition
  CHECK_EQ(array->map(), *previous_map);
  CHECK_EQ(FAST_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(1, Smi::cast(array->length())->value());

  // `delete array[0]` does not alter length, but changes the elments_kind
  name = MakeString("0");
  CHECK(JSReceiver::DeletePropertyOrElement(array, name).FromMaybe(false));
  CHECK_NE(array->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(1, Smi::cast(array->length())->value());
  previous_map = handle(array->map());

  // add a couple of elements, elements_kind stays HOLEY
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_string,
                                                    NONE)
      .Check();
  name = MakeString("1");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_smi,
                                                    NONE)
      .Check();
  CHECK_EQ(array->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(2, Smi::cast(array->length())->value());
}


TEST(JSArrayAddingElementsGeneralizingiFastDoubleElements) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  Handle<String> name;
  Handle<Object> value_smi(Smi::FromInt(42), isolate);
  Handle<Object> value_string(MakeString("value"));
  Handle<Object> value_double = factory->NewNumber(3.1415);

  Handle<JSArray> array =
      factory->NewJSArray(ElementsKind::FAST_SMI_ELEMENTS, 0, 0);
  Handle<Map> previous_map(array->map());

  // `array[0] = value_double` changes |elements_kind| to FAST_DOUBLE_ELEMENTS
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_double,
                                                    NONE)
      .Check();
  CHECK_NE(array->map(), *previous_map);
  CHECK_EQ(FAST_DOUBLE_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(1, Smi::cast(array->length())->value());
  previous_map = handle(array->map());

  // `array[1] = value_smi` doesn't alter the |elements_kind|
  name = MakeString("1");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_smi,
                                                    NONE)
      .Check();
  CHECK_EQ(array->map(), *previous_map);
  CHECK_EQ(FAST_DOUBLE_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(2, Smi::cast(array->length())->value());

  // `delete array[0]` does not alter length, but changes the elments_kind
  name = MakeString("0");
  CHECK(JSReceiver::DeletePropertyOrElement(array, name).FromMaybe(false));
  CHECK_NE(array->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_DOUBLE_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(2, Smi::cast(array->length())->value());
  previous_map = handle(array->map());

  // filling the hole `array[0] = value_smi` again doesn't transition back
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_double,
                                                    NONE)
      .Check();
  CHECK_EQ(array->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_DOUBLE_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(2, Smi::cast(array->length())->value());

  // Adding a string to the array changes to elements_kind FAST_ELEMENTS
  name = MakeString("1");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_string,
                                                    NONE)
      .Check();
  CHECK_NE(array->map(), *previous_map);
  CHECK_EQ(FAST_HOLEY_ELEMENTS, array->map()->elements_kind());
  CHECK_EQ(2, Smi::cast(array->length())->value());
  previous_map = handle(array->map());

  // Adding a double doesn't change the map
  name = MakeString("0");
  JSObject::DefinePropertyOrElementIgnoreAttributes(array, name, value_double,
                                                    NONE)
      .Check();
  CHECK_EQ(array->map(), *previous_map);
}
