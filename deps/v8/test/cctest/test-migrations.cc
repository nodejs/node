// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <utility>

#include "src/v8.h"

#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/global-handles.h"
#include "src/ic/stub-cache.h"
#include "src/macro-assembler.h"
#include "src/smart-pointers.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;


// TODO(ishell): fix this once ReconfigureProperty supports "non equivalent"
// transitions.
const bool IS_NON_EQUIVALENT_TRANSITION_SUPPORTED = false;


// TODO(ishell): fix this once TransitionToPrototype stops generalizing
// all field representations (similar to crbug/448711 where elements kind
// and observed transitions caused generalization of all field representations).
const bool IS_PROTO_TRANS_ISSUE_FIXED = false;


// TODO(ishell): fix this once TransitionToAccessorProperty is able to always
// keep map in fast mode.
const bool IS_ACCESSOR_FIELD_SUPPORTED = false;


// Number of properties used in the tests.
const int kPropCount = 7;


//
// Helper functions.
//

static Handle<String> MakeString(const char* str) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  return factory->InternalizeUtf8String(str);
}


static Handle<String> MakeName(const char* str, int suffix) {
  EmbeddedVector<char, 128> buffer;
  SNPrintF(buffer, "%s%d", str, suffix);
  return MakeString(buffer.start());
}


static Handle<AccessorPair> CreateAccessorPair(bool with_getter,
                                               bool with_setter) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Handle<AccessorPair> pair = factory->NewAccessorPair();
  Handle<String> empty_string = factory->empty_string();
  if (with_getter) {
    Handle<JSFunction> func = factory->NewFunction(empty_string);
    pair->set_getter(*func);
  }
  if (with_setter) {
    Handle<JSFunction> func = factory->NewFunction(empty_string);
    pair->set_setter(*func);
  }
  return pair;
}


static bool EqualDetails(DescriptorArray* descriptors, int descriptor,
                         PropertyType type, PropertyAttributes attributes,
                         Representation representation, int field_index = -1) {
  PropertyDetails details = descriptors->GetDetails(descriptor);
  if (details.type() != type) return false;
  if (details.attributes() != attributes) return false;
  if (!details.representation().Equals(representation)) return false;
  if (field_index >= 0 && details.field_index() != field_index) return false;
  return true;
}


class Expectations {
  static const int MAX_PROPERTIES = 10;
  Isolate* isolate_;
  PropertyType types_[MAX_PROPERTIES];
  PropertyAttributes attributes_[MAX_PROPERTIES];
  Representation representations_[MAX_PROPERTIES];
  // HeapType for kField, value for DATA_CONSTANT and getter for
  // ACCESSOR_CONSTANT.
  Handle<Object> values_[MAX_PROPERTIES];
  // Setter for ACCESSOR_CONSTANT.
  Handle<Object> setter_values_[MAX_PROPERTIES];
  int number_of_properties_;

 public:
  explicit Expectations(Isolate* isolate)
      : isolate_(isolate), number_of_properties_(0) {}

  void Init(int index, PropertyType type, PropertyAttributes attributes,
            Representation representation, Handle<Object> value) {
    DCHECK(index < MAX_PROPERTIES);
    types_[index] = type;
    attributes_[index] = attributes;
    representations_[index] = representation;
    values_[index] = value;
  }

  void Print() const {
    OFStream os(stdout);
    os << "Expectations: #" << number_of_properties_ << "\n";
    for (int i = 0; i < number_of_properties_; i++) {
      os << " " << i << ": ";
      os << "Descriptor @ ";
      if (types_[i] == ACCESSOR_CONSTANT) {
        os << "(get: " << Brief(*values_[i])
           << ", set: " << Brief(*setter_values_[i]) << ") ";
      } else {
        os << Brief(*values_[i]);
      }
      os << " (";
      switch (types_[i]) {
        case DATA_CONSTANT:
          os << "immutable ";
        // Fall through.
        case DATA:
          os << "data";
          break;

        case ACCESSOR_CONSTANT:
          os << "immutable ";
        // Fall through.
        case ACCESSOR:
          os << "accessor";
          break;
      }
      os << ": " << representations_[i].Mnemonic();
      os << ", attrs: " << attributes_[i] << ")\n";
    }
    os << "\n";
  }

  Handle<HeapType> GetFieldType(int index) {
    CHECK(index < MAX_PROPERTIES);
    CHECK(types_[index] == DATA || types_[index] == ACCESSOR);
    return Handle<HeapType>::cast(values_[index]);
  }

  void SetDataField(int index, PropertyAttributes attrs,
                    Representation representation, Handle<HeapType> value) {
    Init(index, DATA, attrs, representation, value);
  }

  void SetDataField(int index, Representation representation,
                    Handle<HeapType> value) {
    SetDataField(index, attributes_[index], representation, value);
  }

  void SetAccessorField(int index, PropertyAttributes attrs) {
    Init(index, ACCESSOR, attrs, Representation::Tagged(),
         HeapType::Any(isolate_));
  }

  void SetAccessorField(int index) {
    SetAccessorField(index, attributes_[index]);
  }

  void SetDataConstant(int index, PropertyAttributes attrs,
                       Handle<JSFunction> value) {
    Init(index, DATA_CONSTANT, attrs, Representation::HeapObject(), value);
  }

  void SetDataConstant(int index, Handle<JSFunction> value) {
    SetDataConstant(index, attributes_[index], value);
  }

  void SetAccessorConstant(int index, PropertyAttributes attrs,
                           Handle<Object> getter, Handle<Object> setter) {
    Init(index, ACCESSOR_CONSTANT, attrs, Representation::Tagged(), getter);
    setter_values_[index] = setter;
  }

  void SetAccessorConstantComponent(int index, PropertyAttributes attrs,
                                    AccessorComponent component,
                                    Handle<Object> accessor) {
    CHECK_EQ(ACCESSOR_CONSTANT, types_[index]);
    CHECK(index < number_of_properties_);
    if (component == ACCESSOR_GETTER) {
      values_[index] = accessor;
    } else {
      setter_values_[index] = accessor;
    }
  }

  void SetAccessorConstant(int index, PropertyAttributes attrs,
                           Handle<AccessorPair> pair) {
    Handle<Object> getter = handle(pair->getter(), isolate_);
    Handle<Object> setter = handle(pair->setter(), isolate_);
    SetAccessorConstant(index, attrs, getter, setter);
  }

  void SetAccessorConstant(int index, Handle<Object> getter,
                           Handle<Object> setter) {
    SetAccessorConstant(index, attributes_[index], getter, setter);
  }

  void SetAccessorConstant(int index, Handle<AccessorPair> pair) {
    Handle<Object> getter = handle(pair->getter(), isolate_);
    Handle<Object> setter = handle(pair->setter(), isolate_);
    SetAccessorConstant(index, getter, setter);
  }

  void GeneralizeRepresentation(int index) {
    CHECK(index < number_of_properties_);
    representations_[index] = Representation::Tagged();
    if (types_[index] == DATA || types_[index] == ACCESSOR) {
      values_[index] = HeapType::Any(isolate_);
    }
  }


  bool Check(DescriptorArray* descriptors, int descriptor) const {
    PropertyType type = types_[descriptor];
    if (!EqualDetails(descriptors, descriptor, type, attributes_[descriptor],
                      representations_[descriptor])) {
      return false;
    }
    Object* value = descriptors->GetValue(descriptor);
    Object* expected_value = *values_[descriptor];
    switch (type) {
      case DATA:
      case ACCESSOR: {
        HeapType* type = descriptors->GetFieldType(descriptor);
        return HeapType::cast(expected_value)->Equals(type);
      }

      case DATA_CONSTANT:
        return value == expected_value;

      case ACCESSOR_CONSTANT: {
        if (value == expected_value) return true;
        if (!value->IsAccessorPair()) return false;
        AccessorPair* pair = AccessorPair::cast(value);
        return pair->Equals(expected_value, *setter_values_[descriptor]);
      }
    }
    UNREACHABLE();
    return false;
  }

  bool Check(Map* map, int expected_nof) const {
    CHECK(number_of_properties_ <= MAX_PROPERTIES);
    CHECK_EQ(expected_nof, map->NumberOfOwnDescriptors());
    CHECK(!map->is_dictionary_map());

    DescriptorArray* descriptors = map->instance_descriptors();
    CHECK(expected_nof <= number_of_properties_);
    for (int i = 0; i < expected_nof; i++) {
      if (!Check(descriptors, i)) {
        Print();
#ifdef OBJECT_PRINT
        descriptors->Print();
#endif
        Check(descriptors, i);
        return false;
      }
    }
    return true;
  }

  bool Check(Map* map) const { return Check(map, number_of_properties_); }


  //
  // Helper methods for initializing expectations and adding properties to
  // given |map|.
  //

  Handle<Map> AddDataField(Handle<Map> map, PropertyAttributes attributes,
                           Representation representation,
                           Handle<HeapType> heap_type) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataField(property_index, attributes, representation, heap_type);

    Handle<String> name = MakeName("prop", property_index);
    return Map::CopyWithField(map, name, heap_type, attributes, representation,
                              INSERT_TRANSITION).ToHandleChecked();
  }

  Handle<Map> AddDataConstant(Handle<Map> map, PropertyAttributes attributes,
                              Handle<JSFunction> value) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataConstant(property_index, attributes, value);

    Handle<String> name = MakeName("prop", property_index);
    return Map::CopyWithConstant(map, name, value, attributes,
                                 INSERT_TRANSITION).ToHandleChecked();
  }

  Handle<Map> TransitionToDataField(Handle<Map> map,
                                    PropertyAttributes attributes,
                                    Representation representation,
                                    Handle<HeapType> heap_type,
                                    Handle<Object> value) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataField(property_index, attributes, representation, heap_type);

    Handle<String> name = MakeName("prop", property_index);
    return Map::TransitionToDataProperty(
        map, name, value, attributes, Object::CERTAINLY_NOT_STORE_FROM_KEYED);
  }

  Handle<Map> TransitionToDataConstant(Handle<Map> map,
                                       PropertyAttributes attributes,
                                       Handle<JSFunction> value) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataConstant(property_index, attributes, value);

    Handle<String> name = MakeName("prop", property_index);
    return Map::TransitionToDataProperty(
        map, name, value, attributes, Object::CERTAINLY_NOT_STORE_FROM_KEYED);
  }

  Handle<Map> FollowDataTransition(Handle<Map> map,
                                   PropertyAttributes attributes,
                                   Representation representation,
                                   Handle<HeapType> heap_type) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataField(property_index, attributes, representation, heap_type);

    Handle<String> name = MakeName("prop", property_index);
    Map* target =
        TransitionArray::SearchTransition(*map, kData, *name, attributes);
    CHECK(target != NULL);
    return handle(target);
  }

  Handle<Map> AddAccessorConstant(Handle<Map> map,
                                  PropertyAttributes attributes,
                                  Handle<AccessorPair> pair) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetAccessorConstant(property_index, attributes, pair);

    Handle<String> name = MakeName("prop", property_index);

    AccessorConstantDescriptor new_desc(name, pair, attributes);
    return Map::CopyInsertDescriptor(map, &new_desc, INSERT_TRANSITION);
  }

  Handle<Map> AddAccessorConstant(Handle<Map> map,
                                  PropertyAttributes attributes,
                                  Handle<Object> getter,
                                  Handle<Object> setter) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetAccessorConstant(property_index, attributes, getter, setter);

    Handle<String> name = MakeName("prop", property_index);

    CHECK(!getter->IsNull() || !setter->IsNull());
    Factory* factory = isolate_->factory();

    if (!getter->IsNull()) {
      Handle<AccessorPair> pair = factory->NewAccessorPair();
      pair->SetComponents(*getter, *factory->null_value());
      AccessorConstantDescriptor new_desc(name, pair, attributes);
      map = Map::CopyInsertDescriptor(map, &new_desc, INSERT_TRANSITION);
    }
    if (!setter->IsNull()) {
      Handle<AccessorPair> pair = factory->NewAccessorPair();
      pair->SetComponents(*getter, *setter);
      AccessorConstantDescriptor new_desc(name, pair, attributes);
      map = Map::CopyInsertDescriptor(map, &new_desc, INSERT_TRANSITION);
    }
    return map;
  }

  Handle<Map> TransitionToAccessorConstant(Handle<Map> map,
                                           PropertyAttributes attributes,
                                           Handle<AccessorPair> pair) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetAccessorConstant(property_index, attributes, pair);

    Handle<String> name = MakeName("prop", property_index);

    Isolate* isolate = CcTest::i_isolate();
    Handle<Object> getter(pair->getter(), isolate);
    Handle<Object> setter(pair->setter(), isolate);

    map = Map::TransitionToAccessorProperty(map, name, ACCESSOR_GETTER, getter,
                                            attributes);
    CHECK(!map->is_deprecated());
    CHECK(!map->is_dictionary_map());

    map = Map::TransitionToAccessorProperty(map, name, ACCESSOR_SETTER, setter,
                                            attributes);
    CHECK(!map->is_deprecated());
    CHECK(!map->is_dictionary_map());
    return map;
  }
};


////////////////////////////////////////////////////////////////////////////////
// A set of tests for property reconfiguration that makes new transition tree
// branch.
//

TEST(ReconfigureAccessorToNonExistingDataField) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> none_type = HeapType::None(isolate);
  Handle<AccessorPair> pair = CreateAccessorPair(true, true);

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  map = expectations.AddAccessorConstant(map, NONE, pair);

  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  Handle<Map> new_map = Map::ReconfigureProperty(
      map, 0, kData, NONE, Representation::None(), none_type, FORCE_FIELD);
  // |map| did not change.
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  expectations.SetDataField(0, NONE, Representation::None(), none_type);

  CHECK(!new_map->is_deprecated());
  CHECK(new_map->is_stable());
  CHECK(expectations.Check(*new_map));

  Handle<Map> new_map2 = Map::ReconfigureProperty(
      map, 0, kData, NONE, Representation::None(), none_type, FORCE_FIELD);
  CHECK_EQ(*new_map, *new_map2);

  Handle<Object> value(Smi::FromInt(0), isolate);
  Handle<Map> prepared_map = Map::PrepareForDataProperty(new_map, 0, value);
  // None to Smi generalization is trivial, map does not change.
  CHECK_EQ(*new_map, *prepared_map);

  expectations.SetDataField(0, NONE, Representation::Smi(), any_type);
  CHECK(prepared_map->is_stable());
  CHECK(expectations.Check(*prepared_map));

  // Now create an object with |map|, migrate it to |prepared_map| and ensure
  // that the data property is uninitialized.
  Factory* factory = isolate->factory();
  Handle<JSObject> obj = factory->NewJSObjectFromMap(map);
  JSObject::MigrateToMap(obj, prepared_map);
  FieldIndex index = FieldIndex::ForDescriptor(*prepared_map, 0);
  CHECK(obj->RawFastPropertyAt(index)->IsUninitialized());
#ifdef VERIFY_HEAP
  obj->ObjectVerify();
#endif
}


// This test checks that the LookupIterator machinery involved in
// JSObject::SetOwnPropertyIgnoreAttributes() does not try to migrate object
// to a map with a property with None representation.
TEST(ReconfigureAccessorToNonExistingDataFieldHeavy) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function getter() { return 1; };"
      "function setter() {};"
      "var o = {};"
      "Object.defineProperty(o, 'foo', "
      "                      { get: getter, set: setter, "
      "                        configurable: true, enumerable: true});");

  Handle<String> foo_str = factory->InternalizeUtf8String("foo");
  Handle<String> obj_name = factory->InternalizeUtf8String("o");

  Handle<Object> obj_value =
      Object::GetProperty(isolate->global_object(), obj_name).ToHandleChecked();
  CHECK(obj_value->IsJSObject());
  Handle<JSObject> obj = Handle<JSObject>::cast(obj_value);

  CHECK_EQ(1, obj->map()->NumberOfOwnDescriptors());
  CHECK(obj->map()->instance_descriptors()->GetValue(0)->IsAccessorPair());

  Handle<Object> value(Smi::FromInt(42), isolate);
  JSObject::SetOwnPropertyIgnoreAttributes(
      obj, foo_str, value, NONE, JSObject::DONT_FORCE_FIELD).ToHandleChecked();

  // Check that the property contains |value|.
  CHECK_EQ(1, obj->map()->NumberOfOwnDescriptors());
  FieldIndex index = FieldIndex::ForDescriptor(obj->map(), 0);
  Object* the_value = obj->RawFastPropertyAt(index);
  CHECK(the_value->IsSmi());
  CHECK_EQ(42, Smi::cast(the_value)->value());
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests for representation generalization case.
//

// This test ensures that representation/field type generalization at
// |property_index| is done correctly independently of the fact that the |map|
// is detached from transition tree or not.
//
//  {} - p0 - p1 - p2: |detach_point_map|
//                  |
//                  X - detached at |detach_property_at_index|
//                  |
//                  + - p3 - p4: |map|
//
// Detaching does not happen if |detach_property_at_index| is -1.
//
static void TestGeneralizeRepresentation(
    int detach_property_at_index, int property_index,
    Representation from_representation, Handle<HeapType> from_type,
    Representation to_representation, Handle<HeapType> to_type,
    Representation expected_representation, Handle<HeapType> expected_type,
    bool expected_deprecation, bool expected_field_type_dependency) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  CHECK(detach_property_at_index >= -1 &&
        detach_property_at_index < kPropCount);
  CHECK(property_index < kPropCount);
  CHECK_NE(detach_property_at_index, property_index);

  const bool is_detached_map = detach_property_at_index >= 0;

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  Handle<Map> detach_point_map;
  for (int i = 0; i < kPropCount; i++) {
    if (i == property_index) {
      map =
          expectations.AddDataField(map, NONE, from_representation, from_type);
    } else {
      map =
          expectations.AddDataField(map, NONE, Representation::Smi(), any_type);
      if (i == detach_property_at_index) {
        detach_point_map = map;
      }
    }
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  Zone zone;
  FakeStubForTesting stub(isolate);

  if (is_detached_map) {
    detach_point_map = Map::ReconfigureProperty(
        detach_point_map, detach_property_at_index, kData, NONE,
        Representation::Tagged(), any_type, FORCE_FIELD);
    expectations.SetDataField(detach_property_at_index,
                              Representation::Tagged(), any_type);
    CHECK(map->is_deprecated());
    CHECK(expectations.Check(*detach_point_map,
                             detach_point_map->NumberOfOwnDescriptors()));
  }

  // Create new maps by generalizing representation of propX field.
  Handle<Map> field_owner(map->FindFieldOwner(property_index), isolate);
  CompilationInfo info(&stub, isolate, &zone);
  CHECK(!info.HasAbortedDueToDependencyChange());

  Map::AddDependentCompilationInfo(field_owner, DependentCode::kFieldTypeGroup,
                                   &info);

  Handle<Map> new_map =
      Map::ReconfigureProperty(map, property_index, kData, NONE,
                               to_representation, to_type, FORCE_FIELD);

  expectations.SetDataField(property_index, expected_representation,
                            expected_type);

  CHECK(!new_map->is_deprecated());
  CHECK(expectations.Check(*new_map));

  if (is_detached_map) {
    CHECK(map->is_deprecated());
    CHECK_NE(*map, *new_map);
    CHECK_EQ(expected_field_type_dependency && !field_owner->is_deprecated(),
             info.HasAbortedDueToDependencyChange());

  } else if (expected_deprecation) {
    CHECK(map->is_deprecated());
    CHECK(field_owner->is_deprecated());
    CHECK_NE(*map, *new_map);
    CHECK(!info.HasAbortedDueToDependencyChange());

  } else {
    CHECK(!field_owner->is_deprecated());
    CHECK_EQ(*map, *new_map);

    CHECK_EQ(expected_field_type_dependency,
             info.HasAbortedDueToDependencyChange());
  }

  info.RollbackDependencies();  // Properly cleanup compilation info.

  // Update all deprecated maps and check that they are now the same.
  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*new_map, *updated_map);
}


static void TestGeneralizeRepresentation(
    Representation from_representation, Handle<HeapType> from_type,
    Representation to_representation, Handle<HeapType> to_type,
    Representation expected_representation, Handle<HeapType> expected_type,
    bool expected_deprecation, bool expected_field_type_dependency) {
  // Check the cases when the map being reconfigured is a part of the
  // transition tree.
  STATIC_ASSERT(kPropCount > 4);
  int indices[] = {0, 2, kPropCount - 1};
  for (int i = 0; i < static_cast<int>(arraysize(indices)); i++) {
    TestGeneralizeRepresentation(
        -1, indices[i], from_representation, from_type, to_representation,
        to_type, expected_representation, expected_type, expected_deprecation,
        expected_field_type_dependency);
  }

  if (!from_representation.IsNone()) {
    // Check the cases when the map being reconfigured is NOT a part of the
    // transition tree. "None -> anything" representation changes make sense
    // only for "attached" maps.
    int indices[] = {0, kPropCount - 1};
    for (int i = 0; i < static_cast<int>(arraysize(indices)); i++) {
      TestGeneralizeRepresentation(
          indices[i], 2, from_representation, from_type, to_representation,
          to_type, expected_representation, expected_type, expected_deprecation,
          expected_field_type_dependency);
    }
  }
}


static void TestGeneralizeRepresentation(Representation from_representation,
                                         Handle<HeapType> from_type,
                                         Representation to_representation,
                                         Handle<HeapType> to_type,
                                         Representation expected_representation,
                                         Handle<HeapType> expected_type) {
  const bool expected_deprecation = true;
  const bool expected_field_type_dependency = false;

  TestGeneralizeRepresentation(
      from_representation, from_type, to_representation, to_type,
      expected_representation, expected_type, expected_deprecation,
      expected_field_type_dependency);
}


static void TestGeneralizeRepresentationTrivial(
    Representation from_representation, Handle<HeapType> from_type,
    Representation to_representation, Handle<HeapType> to_type,
    Representation expected_representation, Handle<HeapType> expected_type,
    bool expected_field_type_dependency = true) {
  const bool expected_deprecation = false;

  TestGeneralizeRepresentation(
      from_representation, from_type, to_representation, to_type,
      expected_representation, expected_type, expected_deprecation,
      expected_field_type_dependency);
}


TEST(GeneralizeRepresentationSmiToDouble) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  TestGeneralizeRepresentation(Representation::Smi(), any_type,
                               Representation::Double(), any_type,
                               Representation::Double(), any_type);
}


TEST(GeneralizeRepresentationSmiToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  TestGeneralizeRepresentation(Representation::Smi(), any_type,
                               Representation::HeapObject(), value_type,
                               Representation::Tagged(), any_type);
}


TEST(GeneralizeRepresentationDoubleToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  TestGeneralizeRepresentation(Representation::Double(), any_type,
                               Representation::HeapObject(), value_type,
                               Representation::Tagged(), any_type);
}


TEST(GeneralizeRepresentationHeapObjectToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  TestGeneralizeRepresentation(Representation::HeapObject(), value_type,
                               Representation::Smi(), any_type,
                               Representation::Tagged(), any_type);
}


TEST(GeneralizeRepresentationHeapObjectToHeapObject) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  const int kMaxClassesPerFieldType = 1;
  Handle<HeapType> current_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  for (int i = 0; i < kMaxClassesPerFieldType; i++) {
    Handle<HeapType> new_type =
        HeapType::Class(Map::Create(isolate, 0), isolate);

    Handle<HeapType> expected_type =
        (i < kMaxClassesPerFieldType - 1)
            ? HeapType::Union(current_type, new_type, isolate)
            : any_type;

    TestGeneralizeRepresentationTrivial(
        Representation::HeapObject(), current_type,
        Representation::HeapObject(), new_type, Representation::HeapObject(),
        expected_type);
    current_type = expected_type;
  }

  Handle<HeapType> new_type = HeapType::Class(Map::Create(isolate, 0), isolate);

  TestGeneralizeRepresentationTrivial(
      Representation::HeapObject(), any_type, Representation::HeapObject(),
      new_type, Representation::HeapObject(), any_type, false);
}


TEST(GeneralizeRepresentationNoneToSmi) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> none_type = HeapType::None(isolate);
  Handle<HeapType> any_type = HeapType::Any(isolate);

  // None -> Smi representation change is trivial.
  TestGeneralizeRepresentationTrivial(Representation::None(), none_type,
                                      Representation::Smi(), any_type,
                                      Representation::Smi(), any_type);
}


TEST(GeneralizeRepresentationNoneToDouble) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> none_type = HeapType::None(isolate);
  Handle<HeapType> any_type = HeapType::Any(isolate);

  // None -> Double representation change is NOT trivial.
  TestGeneralizeRepresentation(Representation::None(), none_type,
                               Representation::Double(), any_type,
                               Representation::Double(), any_type);
}


TEST(GeneralizeRepresentationNoneToHeapObject) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> none_type = HeapType::None(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  // None -> HeapObject representation change is trivial.
  TestGeneralizeRepresentationTrivial(Representation::None(), none_type,
                                      Representation::HeapObject(), value_type,
                                      Representation::HeapObject(), value_type);
}


TEST(GeneralizeRepresentationNoneToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> none_type = HeapType::None(isolate);
  Handle<HeapType> any_type = HeapType::Any(isolate);

  // None -> HeapObject representation change is trivial.
  TestGeneralizeRepresentationTrivial(Representation::None(), none_type,
                                      Representation::Tagged(), any_type,
                                      Representation::Tagged(), any_type);
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests for representation generalization case with kAccessor
// properties.
//

TEST(GeneralizeRepresentationWithAccessorProperties) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<AccessorPair> pair = CreateAccessorPair(true, true);

  const int kAccessorProp = kPropCount / 2;
  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    if (i == kAccessorProp) {
      map = expectations.AddAccessorConstant(map, NONE, pair);
    } else {
      map =
          expectations.AddDataField(map, NONE, Representation::Smi(), any_type);
    }
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  // Create new maps by generalizing representation of propX field.
  Handle<Map> maps[kPropCount];
  for (int i = 0; i < kPropCount; i++) {
    if (i == kAccessorProp) {
      // Skip accessor property reconfiguration.
      maps[i] = maps[i - 1];
      continue;
    }
    Handle<Map> new_map = Map::ReconfigureProperty(
        map, i, kData, NONE, Representation::Double(), any_type, FORCE_FIELD);
    maps[i] = new_map;

    expectations.SetDataField(i, Representation::Double(), any_type);

    CHECK(map->is_deprecated());
    CHECK_NE(*map, *new_map);
    CHECK(i == 0 || maps[i - 1]->is_deprecated());

    CHECK(!new_map->is_deprecated());
    CHECK(expectations.Check(*new_map));
  }

  Handle<Map> active_map = maps[kPropCount - 1];
  CHECK(!active_map->is_deprecated());

  // Update all deprecated maps and check that they are now the same.
  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*active_map, *updated_map);
  for (int i = 0; i < kPropCount; i++) {
    updated_map = Map::Update(maps[i]);
    CHECK_EQ(*active_map, *updated_map);
  }
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests for attribute reconfiguration case.
//

// This test ensures that representation/field type generalization is correctly
// propagated from one branch of transition tree (|map2|) to another (|map|).
//
//             + - p2B - p3 - p4: |map2|
//             |
//  {} - p0 - p1 - p2A - p3 - p4: |map|
//
// where "p2A" and "p2B" differ only in the attributes.
//
static void TestReconfigureDataFieldAttribute_GeneralizeRepresentation(
    Representation from_representation, Handle<HeapType> from_type,
    Representation to_representation, Handle<HeapType> to_type,
    Representation expected_representation, Handle<HeapType> expected_type) {
  Isolate* isolate = CcTest::i_isolate();

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, from_representation, from_type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));


  // Create another branch in transition tree (property at index |kSplitProp|
  // has different attributes), initialize expectations.
  const int kSplitProp = kPropCount / 2;
  Expectations expectations2(isolate);

  Handle<Map> map2 = initial_map;
  for (int i = 0; i < kSplitProp; i++) {
    map2 = expectations2.FollowDataTransition(map2, NONE, from_representation,
                                              from_type);
  }
  map2 =
      expectations2.AddDataField(map2, READ_ONLY, to_representation, to_type);

  for (int i = kSplitProp + 1; i < kPropCount; i++) {
    map2 = expectations2.AddDataField(map2, NONE, to_representation, to_type);
  }
  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));

  Zone zone;
  FakeStubForTesting stub(isolate);
  Handle<Map> field_owner(map->FindFieldOwner(kSplitProp), isolate);
  CompilationInfo info(&stub, isolate, &zone);
  CHECK(!info.HasAbortedDueToDependencyChange());
  Map::AddDependentCompilationInfo(field_owner, DependentCode::kFieldTypeGroup,
                                   &info);

  // Reconfigure attributes of property |kSplitProp| of |map2| to NONE, which
  // should generalize representations in |map1|.
  Handle<Map> new_map =
      Map::ReconfigureExistingProperty(map2, kSplitProp, kData, NONE);

  // |map2| should be left unchanged.
  CHECK(!map2->is_deprecated());
  CHECK_NE(*map2, *new_map);
  CHECK(expectations2.Check(*map2));

  // |map| should be deprecated and |new_map| should match new expectations.
  for (int i = kSplitProp; i < kPropCount; i++) {
    expectations.SetDataField(i, expected_representation, expected_type);
  }
  CHECK(map->is_deprecated());
  CHECK(!info.HasAbortedDueToDependencyChange());
  info.RollbackDependencies();  // Properly cleanup compilation info.
  CHECK_NE(*map, *new_map);

  CHECK(!new_map->is_deprecated());
  CHECK(expectations.Check(*new_map));

  // Update deprecated |map|, it should become |new_map|.
  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*new_map, *updated_map);
}


// This test ensures that trivial representation/field type generalization
// (from HeapObject to HeapObject) is correctly propagated from one branch of
// transition tree (|map2|) to another (|map|).
//
//             + - p2B - p3 - p4: |map2|
//             |
//  {} - p0 - p1 - p2A - p3 - p4: |map|
//
// where "p2A" and "p2B" differ only in the attributes.
//
static void TestReconfigureDataFieldAttribute_GeneralizeRepresentationTrivial(
    Representation from_representation, Handle<HeapType> from_type,
    Representation to_representation, Handle<HeapType> to_type,
    Representation expected_representation, Handle<HeapType> expected_type,
    bool expected_field_type_dependency = true) {
  Isolate* isolate = CcTest::i_isolate();

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, from_representation, from_type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));


  // Create another branch in transition tree (property at index |kSplitProp|
  // has different attributes), initialize expectations.
  const int kSplitProp = kPropCount / 2;
  Expectations expectations2(isolate);

  Handle<Map> map2 = initial_map;
  for (int i = 0; i < kSplitProp; i++) {
    map2 = expectations2.FollowDataTransition(map2, NONE, from_representation,
                                              from_type);
  }
  map2 =
      expectations2.AddDataField(map2, READ_ONLY, to_representation, to_type);

  for (int i = kSplitProp + 1; i < kPropCount; i++) {
    map2 = expectations2.AddDataField(map2, NONE, to_representation, to_type);
  }
  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));

  Zone zone;
  FakeStubForTesting stub(isolate);
  Handle<Map> field_owner(map->FindFieldOwner(kSplitProp), isolate);
  CompilationInfo info(&stub, isolate, &zone);
  CHECK(!info.HasAbortedDueToDependencyChange());
  Map::AddDependentCompilationInfo(field_owner, DependentCode::kFieldTypeGroup,
                                   &info);

  // Reconfigure attributes of property |kSplitProp| of |map2| to NONE, which
  // should generalize representations in |map1|.
  Handle<Map> new_map =
      Map::ReconfigureExistingProperty(map2, kSplitProp, kData, NONE);

  // |map2| should be left unchanged.
  CHECK(!map2->is_deprecated());
  CHECK_NE(*map2, *new_map);
  CHECK(expectations2.Check(*map2));

  // In trivial case |map| should be returned as a result of the property
  // reconfiguration, respective field types should be generalized and
  // respective code dependencies should be invalidated. |map| should be NOT
  // deprecated and it should match new expectations.
  for (int i = kSplitProp; i < kPropCount; i++) {
    expectations.SetDataField(i, expected_representation, expected_type);
  }
  CHECK(!map->is_deprecated());
  CHECK_EQ(*map, *new_map);
  CHECK_EQ(expected_field_type_dependency,
           info.HasAbortedDueToDependencyChange());
  info.RollbackDependencies();  // Properly cleanup compilation info.

  CHECK(!new_map->is_deprecated());
  CHECK(expectations.Check(*new_map));

  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*new_map, *updated_map);
}


TEST(ReconfigureDataFieldAttribute_GeneralizeRepresentationSmiToDouble) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  TestReconfigureDataFieldAttribute_GeneralizeRepresentation(
      Representation::Smi(), any_type, Representation::Double(), any_type,
      Representation::Double(), any_type);
}


TEST(ReconfigureDataFieldAttribute_GeneralizeRepresentationSmiToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  TestReconfigureDataFieldAttribute_GeneralizeRepresentation(
      Representation::Smi(), any_type, Representation::HeapObject(), value_type,
      Representation::Tagged(), any_type);
}


TEST(ReconfigureDataFieldAttribute_GeneralizeRepresentationDoubleToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  TestReconfigureDataFieldAttribute_GeneralizeRepresentation(
      Representation::Double(), any_type, Representation::HeapObject(),
      value_type, Representation::Tagged(), any_type);
}


TEST(ReconfigureDataFieldAttribute_GeneralizeRepresentationHeapObjToHeapObj) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  const int kMaxClassesPerFieldType = 1;
  Handle<HeapType> current_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  for (int i = 0; i < kMaxClassesPerFieldType; i++) {
    Handle<HeapType> new_type =
        HeapType::Class(Map::Create(isolate, 0), isolate);

    Handle<HeapType> expected_type =
        (i < kMaxClassesPerFieldType - 1)
            ? HeapType::Union(current_type, new_type, isolate)
            : any_type;

    TestReconfigureDataFieldAttribute_GeneralizeRepresentationTrivial(
        Representation::HeapObject(), current_type,
        Representation::HeapObject(), new_type, Representation::HeapObject(),
        expected_type);
    current_type = expected_type;
  }

  Handle<HeapType> new_type = HeapType::Class(Map::Create(isolate, 0), isolate);

  TestReconfigureDataFieldAttribute_GeneralizeRepresentationTrivial(
      Representation::HeapObject(), any_type, Representation::HeapObject(),
      new_type, Representation::HeapObject(), any_type, false);
}


TEST(ReconfigureDataFieldAttribute_GeneralizeRepresentationHeapObjectToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  TestReconfigureDataFieldAttribute_GeneralizeRepresentation(
      Representation::HeapObject(), value_type, Representation::Smi(), any_type,
      Representation::Tagged(), any_type);
}


// Checks that given |map| is deprecated and that it updates to given |new_map|
// which in turn should match expectations.
struct CheckDeprecated {
  void Check(Handle<Map> map, Handle<Map> new_map,
             const Expectations& expectations) {
    CHECK(map->is_deprecated());
    CHECK_NE(*map, *new_map);

    CHECK(!new_map->is_deprecated());
    CHECK(expectations.Check(*new_map));

    // Update deprecated |map|, it should become |new_map|.
    Handle<Map> updated_map = Map::Update(map);
    CHECK_EQ(*new_map, *updated_map);
  }
};


// Checks that given |map| is NOT deprecated, equals to given |new_map| and
// matches expectations.
struct CheckSameMap {
  void Check(Handle<Map> map, Handle<Map> new_map,
             const Expectations& expectations) {
    CHECK(!map->is_deprecated());
    CHECK_EQ(*map, *new_map);

    CHECK(!new_map->is_deprecated());
    CHECK(expectations.Check(*new_map));

    // Update deprecated |map|, it should become |new_map|.
    Handle<Map> updated_map = Map::Update(map);
    CHECK_EQ(*new_map, *updated_map);
  }
};


// Checks that given |map| is NOT deprecated, and |new_map| is a result of
// copy-generalize-all-representations.
struct CheckCopyGeneralizeAllRepresentations {
  void Check(Handle<Map> map, Handle<Map> new_map, Expectations& expectations) {
    CHECK(!map->is_deprecated());
    CHECK_NE(*map, *new_map);

    CHECK(new_map->GetBackPointer()->IsUndefined());
    for (int i = 0; i < kPropCount; i++) {
      expectations.GeneralizeRepresentation(i);
    }

    CHECK(!new_map->is_deprecated());
    CHECK(expectations.Check(*new_map));
  }
};


// This test ensures that representation/field type generalization is correctly
// propagated from one branch of transition tree (|map2|) to another (|map1|).
//
//             + - p2B - p3 - p4: |map2|
//             |
//  {} - p0 - p1: |map|
//             |
//             + - p2A - p3 - p4: |map1|
//                        |
//                        + - the property customized by the TestConfig provided
//
// where "p2A" and "p2B" differ only in the attributes.
//
template <typename TestConfig, typename Checker>
static void TestReconfigureProperty_CustomPropertyAfterTargetMap(
    TestConfig& config, Checker& checker) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  const int kCustomPropIndex = kPropCount - 2;
  Expectations expectations(isolate);

  const int kSplitProp = 2;
  CHECK(kSplitProp < kCustomPropIndex);

  const Representation representation = Representation::Smi();

  // Create common part of transition tree.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kSplitProp; i++) {
    map = expectations.AddDataField(map, NONE, representation, any_type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));


  // Create branch to |map1|.
  Handle<Map> map1 = map;
  Expectations expectations1 = expectations;
  for (int i = kSplitProp; i < kCustomPropIndex; i++) {
    map1 = expectations1.AddDataField(map1, NONE, representation, any_type);
  }
  map1 = config.AddPropertyAtBranch(1, expectations1, map1);
  for (int i = kCustomPropIndex + 1; i < kPropCount; i++) {
    map1 = expectations1.AddDataField(map1, NONE, representation, any_type);
  }
  CHECK(!map1->is_deprecated());
  CHECK(map1->is_stable());
  CHECK(expectations1.Check(*map1));


  // Create another branch in transition tree (property at index |kSplitProp|
  // has different attributes), initialize expectations.
  Handle<Map> map2 = map;
  Expectations expectations2 = expectations;
  map2 = expectations2.AddDataField(map2, READ_ONLY, representation, any_type);
  for (int i = kSplitProp + 1; i < kCustomPropIndex; i++) {
    map2 = expectations2.AddDataField(map2, NONE, representation, any_type);
  }
  map2 = config.AddPropertyAtBranch(2, expectations2, map2);
  for (int i = kCustomPropIndex + 1; i < kPropCount; i++) {
    map2 = expectations2.AddDataField(map2, NONE, representation, any_type);
  }
  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));


  // Reconfigure attributes of property |kSplitProp| of |map2| to NONE, which
  // should generalize representations in |map1|.
  Handle<Map> new_map =
      Map::ReconfigureExistingProperty(map2, kSplitProp, kData, NONE);

  // |map2| should be left unchanged.
  CHECK(!map2->is_deprecated());
  CHECK_NE(*map2, *new_map);
  CHECK(expectations2.Check(*map2));

  config.UpdateExpectations(kCustomPropIndex, expectations1);
  checker.Check(map1, new_map, expectations1);
}


TEST(ReconfigureDataFieldAttribute_SameDataConstantAfterTargetMap) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  struct TestConfig {
    Handle<JSFunction> js_func_;
    TestConfig() {
      Isolate* isolate = CcTest::i_isolate();
      Factory* factory = isolate->factory();
      js_func_ = factory->NewFunction(factory->empty_string());
    }

    Handle<Map> AddPropertyAtBranch(int branch_id, Expectations& expectations,
                                    Handle<Map> map) {
      CHECK(branch_id == 1 || branch_id == 2);
      // Add the same data constant property at both transition tree branches.
      return expectations.AddDataConstant(map, NONE, js_func_);
    }

    void UpdateExpectations(int property_index, Expectations& expectations) {
      // Expectations stay the same.
    }
  };

  TestConfig config;
  // Two branches are "compatible" so the |map1| should NOT be deprecated.
  CheckSameMap checker;
  TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
}


TEST(ReconfigureDataFieldAttribute_DataConstantToDataFieldAfterTargetMap) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  struct TestConfig {
    Handle<JSFunction> js_func1_;
    Handle<JSFunction> js_func2_;
    TestConfig() {
      Isolate* isolate = CcTest::i_isolate();
      Factory* factory = isolate->factory();
      js_func1_ = factory->NewFunction(factory->empty_string());
      js_func2_ = factory->NewFunction(factory->empty_string());
    }

    Handle<Map> AddPropertyAtBranch(int branch_id, Expectations& expectations,
                                    Handle<Map> map) {
      CHECK(branch_id == 1 || branch_id == 2);
      Handle<JSFunction> js_func = branch_id == 1 ? js_func1_ : js_func2_;
      return expectations.AddDataConstant(map, NONE, js_func);
    }

    void UpdateExpectations(int property_index, Expectations& expectations) {
      Isolate* isolate = CcTest::i_isolate();
      Handle<HeapType> any_type = HeapType::Any(isolate);
      expectations.SetDataField(property_index, Representation::HeapObject(),
                                any_type);
    }
  };

  TestConfig config;
  // Two branches are "incompatible" so the |map1| should be deprecated.
  CheckDeprecated checker;
  TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
}


TEST(ReconfigureDataFieldAttribute_SameAccessorConstantAfterTargetMap) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  struct TestConfig {
    Handle<AccessorPair> pair_;
    TestConfig() { pair_ = CreateAccessorPair(true, true); }

    Handle<Map> AddPropertyAtBranch(int branch_id, Expectations& expectations,
                                    Handle<Map> map) {
      CHECK(branch_id == 1 || branch_id == 2);
      // Add the same accessor constant property at both transition tree
      // branches.
      return expectations.AddAccessorConstant(map, NONE, pair_);
    }

    bool UpdateExpectations(int property_index, Expectations& expectations) {
      // Two branches are "compatible" so the |map1| should NOT be deprecated.
      return false;
    }
  };

  TestConfig config;
  CheckSameMap checker;
  TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
}


TEST(ReconfigureDataFieldAttribute_AccConstantToAccFieldAfterTargetMap) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  struct TestConfig {
    Handle<AccessorPair> pair1_;
    Handle<AccessorPair> pair2_;
    TestConfig() {
      pair1_ = CreateAccessorPair(true, true);
      pair2_ = CreateAccessorPair(true, true);
    }

    Handle<Map> AddPropertyAtBranch(int branch_id, Expectations& expectations,
                                    Handle<Map> map) {
      CHECK(branch_id == 1 || branch_id == 2);
      Handle<AccessorPair> pair = branch_id == 1 ? pair1_ : pair2_;
      return expectations.AddAccessorConstant(map, NONE, pair);
    }

    void UpdateExpectations(int property_index, Expectations& expectations) {
      if (IS_ACCESSOR_FIELD_SUPPORTED) {
        expectations.SetAccessorField(property_index);
      } else {
        // Currently we have a copy-generalize-all-representations case and
        // ACCESSOR property becomes ACCESSOR_CONSTANT.
        expectations.SetAccessorConstant(property_index, pair2_);
      }
    }
  };

  TestConfig config;
  if (IS_ACCESSOR_FIELD_SUPPORTED) {
    CheckCopyGeneralizeAllRepresentations checker;
    TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
  } else {
    // Currently we have a copy-generalize-all-representations case.
    CheckCopyGeneralizeAllRepresentations checker;
    TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
  }
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests checking split map deprecation.
//

TEST(ReconfigurePropertySplitMapTransitionsOverflow) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, Representation::Smi(), any_type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());

  // Generalize representation of property at index |kSplitProp|.
  const int kSplitProp = kPropCount / 2;
  Handle<Map> split_map;
  Handle<Map> map2 = initial_map;
  {
    for (int i = 0; i < kSplitProp + 1; i++) {
      if (i == kSplitProp) {
        split_map = map2;
      }

      Handle<String> name = MakeName("prop", i);
      Map* target =
          TransitionArray::SearchTransition(*map2, kData, *name, NONE);
      CHECK(target != NULL);
      map2 = handle(target);
    }

    map2 = Map::ReconfigureProperty(map2, kSplitProp, kData, NONE,
                                    Representation::Double(), any_type,
                                    FORCE_FIELD);
    expectations.SetDataField(kSplitProp, Representation::Double(), any_type);

    CHECK(expectations.Check(*split_map, kSplitProp));
    CHECK(expectations.Check(*map2, kSplitProp + 1));
  }

  // At this point |map| should be deprecated and disconnected from the
  // transition tree.
  CHECK(map->is_deprecated());
  CHECK(!split_map->is_deprecated());
  CHECK(!map2->is_deprecated());

  // Fill in transition tree of |map2| so that it can't have more transitions.
  for (int i = 0; i < TransitionArray::kMaxNumberOfTransitions; i++) {
    CHECK(TransitionArray::CanHaveMoreTransitions(map2));
    Handle<String> name = MakeName("foo", i);
    Map::CopyWithField(map2, name, any_type, NONE, Representation::Smi(),
                       INSERT_TRANSITION).ToHandleChecked();
  }
  CHECK(!TransitionArray::CanHaveMoreTransitions(map2));

  // Try to update |map|, since there is no place for propX transition at |map2|
  // |map| should become "copy-generalized".
  Handle<Map> updated_map = Map::Update(map);
  CHECK(updated_map->GetBackPointer()->IsUndefined());

  for (int i = 0; i < kPropCount; i++) {
    expectations.SetDataField(i, Representation::Tagged(), any_type);
  }
  CHECK(expectations.Check(*updated_map));
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests involving special transitions (such as elements kind
// transition, observed transition or prototype transition).
//

// This test ensures that representation/field type generalization is correctly
// propagated from one branch of transition tree (|map2|) to another (|map|).
//
//                            p4B: |map2|
//                             |
//                             * - special transition
//                             |
//  {} - p0 - p1 - p2A - p3 - p4A: |map|
//
// where "p4A" and "p4B" are exactly the same properties.
//
// TODO(ishell): unify this test template with
// TestReconfigureDataFieldAttribute_GeneralizeRepresentation once
// IS_PROTO_TRANS_ISSUE_FIXED and IS_NON_EQUIVALENT_TRANSITION_SUPPORTED are
// fixed.
template <typename TestConfig>
static void TestGeneralizeRepresentationWithSpecialTransition(
    TestConfig& config, Representation from_representation,
    Handle<HeapType> from_type, Representation to_representation,
    Handle<HeapType> to_type, Representation expected_representation,
    Handle<HeapType> expected_type) {
  Isolate* isolate = CcTest::i_isolate();

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, from_representation, from_type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  // Apply some special transition to |map|.
  CHECK(map->owns_descriptors());
  Handle<Map> map2 = config.Transition(map);

  // |map| should still match expectations.
  CHECK(!map->is_deprecated());
  CHECK(expectations.Check(*map));

  Expectations expectations2 = expectations;
  if (config.generalizes_representations()) {
    for (int i = 0; i < kPropCount; i++) {
      expectations2.GeneralizeRepresentation(i);
    }
  }

  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));

  // Create new maps by generalizing representation of propX field.
  Handle<Map> maps[kPropCount];
  for (int i = 0; i < kPropCount; i++) {
    Handle<Map> new_map = Map::ReconfigureProperty(
        map, i, kData, NONE, to_representation, to_type, FORCE_FIELD);
    maps[i] = new_map;

    expectations.SetDataField(i, expected_representation, expected_type);

    CHECK(map->is_deprecated());
    CHECK_NE(*map, *new_map);
    CHECK(i == 0 || maps[i - 1]->is_deprecated());
    CHECK(expectations.Check(*new_map));

    Handle<Map> new_map2 = Map::Update(map2);
    CHECK(!new_map2->is_deprecated());
    CHECK(!new_map2->is_dictionary_map());

    if (!IS_NON_EQUIVALENT_TRANSITION_SUPPORTED) {
      // In case of non-equivalent transition currently we generalize all
      // representations.
      for (int i = 0; i < kPropCount; i++) {
        expectations2.GeneralizeRepresentation(i);
      }
      CHECK(new_map2->GetBackPointer()->IsUndefined());
      CHECK(expectations2.Check(*new_map2));
    } else {
      CHECK(expectations.Check(*new_map2));
    }
  }

  Handle<Map> active_map = maps[kPropCount - 1];
  CHECK(!active_map->is_deprecated());

  // Update all deprecated maps and check that they are now the same.
  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*active_map, *updated_map);
  for (int i = 0; i < kPropCount; i++) {
    updated_map = Map::Update(maps[i]);
    CHECK_EQ(*active_map, *updated_map);
  }
}


TEST(ElementsKindTransitionFromMapOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<Map> Transition(Handle<Map> map) {
      return Map::CopyAsElementsKind(map, DICTIONARY_ELEMENTS,
                                     INSERT_TRANSITION);
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const { return false; }
  };
  TestConfig config;
  TestGeneralizeRepresentationWithSpecialTransition(
      config, Representation::Smi(), any_type, Representation::HeapObject(),
      value_type, Representation::Tagged(), any_type);
}


TEST(ElementsKindTransitionFromMapNotOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<Map> Transition(Handle<Map> map) {
      Isolate* isolate = CcTest::i_isolate();
      Handle<HeapType> any_type = HeapType::Any(isolate);

      // Add one more transition to |map| in order to prevent descriptors
      // ownership.
      CHECK(map->owns_descriptors());
      Map::CopyWithField(map, MakeString("foo"), any_type, NONE,
                         Representation::Smi(),
                         INSERT_TRANSITION).ToHandleChecked();
      CHECK(!map->owns_descriptors());

      return Map::CopyAsElementsKind(map, DICTIONARY_ELEMENTS,
                                     INSERT_TRANSITION);
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const { return false; }
  };
  TestConfig config;
  TestGeneralizeRepresentationWithSpecialTransition(
      config, Representation::Smi(), any_type, Representation::HeapObject(),
      value_type, Representation::Tagged(), any_type);
}


TEST(ForObservedTransitionFromMapOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<Map> Transition(Handle<Map> map) {
      return Map::CopyForObserved(map);
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const { return false; }
  };
  TestConfig config;
  TestGeneralizeRepresentationWithSpecialTransition(
      config, Representation::Smi(), any_type, Representation::HeapObject(),
      value_type, Representation::Tagged(), any_type);
}


TEST(ForObservedTransitionFromMapNotOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<Map> Transition(Handle<Map> map) {
      Isolate* isolate = CcTest::i_isolate();
      Handle<HeapType> any_type = HeapType::Any(isolate);

      // Add one more transition to |map| in order to prevent descriptors
      // ownership.
      CHECK(map->owns_descriptors());
      Map::CopyWithField(map, MakeString("foo"), any_type, NONE,
                         Representation::Smi(),
                         INSERT_TRANSITION).ToHandleChecked();
      CHECK(!map->owns_descriptors());

      return Map::CopyForObserved(map);
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const { return false; }
  };
  TestConfig config;
  TestGeneralizeRepresentationWithSpecialTransition(
      config, Representation::Smi(), any_type, Representation::HeapObject(),
      value_type, Representation::Tagged(), any_type);
}


TEST(PrototypeTransitionFromMapOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<JSObject> prototype_;

    TestConfig() {
      Isolate* isolate = CcTest::i_isolate();
      Factory* factory = isolate->factory();
      prototype_ = factory->NewJSObjectFromMap(Map::Create(isolate, 0));
    }

    Handle<Map> Transition(Handle<Map> map) {
      return Map::TransitionToPrototype(map, prototype_, REGULAR_PROTOTYPE);
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const {
      return !IS_PROTO_TRANS_ISSUE_FIXED;
    }
  };
  TestConfig config;
  TestGeneralizeRepresentationWithSpecialTransition(
      config, Representation::Smi(), any_type, Representation::HeapObject(),
      value_type, Representation::Tagged(), any_type);
}


TEST(PrototypeTransitionFromMapNotOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<HeapType> any_type = HeapType::Any(isolate);
  Handle<HeapType> value_type =
      HeapType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<JSObject> prototype_;

    TestConfig() {
      Isolate* isolate = CcTest::i_isolate();
      Factory* factory = isolate->factory();
      prototype_ = factory->NewJSObjectFromMap(Map::Create(isolate, 0));
    }

    Handle<Map> Transition(Handle<Map> map) {
      Isolate* isolate = CcTest::i_isolate();
      Handle<HeapType> any_type = HeapType::Any(isolate);

      // Add one more transition to |map| in order to prevent descriptors
      // ownership.
      CHECK(map->owns_descriptors());
      Map::CopyWithField(map, MakeString("foo"), any_type, NONE,
                         Representation::Smi(),
                         INSERT_TRANSITION).ToHandleChecked();
      CHECK(!map->owns_descriptors());

      return Map::TransitionToPrototype(map, prototype_, REGULAR_PROTOTYPE);
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const {
      return !IS_PROTO_TRANS_ISSUE_FIXED;
    }
  };
  TestConfig config;
  TestGeneralizeRepresentationWithSpecialTransition(
      config, Representation::Smi(), any_type, Representation::HeapObject(),
      value_type, Representation::Tagged(), any_type);
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests for higher level transitioning mechanics.
//

struct TransitionToDataFieldOperator {
  Representation representation_;
  PropertyAttributes attributes_;
  Handle<HeapType> heap_type_;
  Handle<Object> value_;

  TransitionToDataFieldOperator(Representation representation,
                                Handle<HeapType> heap_type,
                                Handle<Object> value,
                                PropertyAttributes attributes = NONE)
      : representation_(representation),
        attributes_(attributes),
        heap_type_(heap_type),
        value_(value) {}

  Handle<Map> DoTransition(Expectations& expectations, Handle<Map> map) {
    return expectations.TransitionToDataField(map, attributes_, representation_,
                                              heap_type_, value_);
  }
};


struct TransitionToDataConstantOperator {
  PropertyAttributes attributes_;
  Handle<JSFunction> value_;

  TransitionToDataConstantOperator(Handle<JSFunction> value,
                                   PropertyAttributes attributes = NONE)
      : attributes_(attributes), value_(value) {}

  Handle<Map> DoTransition(Expectations& expectations, Handle<Map> map) {
    return expectations.TransitionToDataConstant(map, attributes_, value_);
  }
};


struct TransitionToAccessorConstantOperator {
  PropertyAttributes attributes_;
  Handle<AccessorPair> pair_;

  TransitionToAccessorConstantOperator(Handle<AccessorPair> pair,
                                       PropertyAttributes attributes = NONE)
      : attributes_(attributes), pair_(pair) {}

  Handle<Map> DoTransition(Expectations& expectations, Handle<Map> map) {
    return expectations.TransitionToAccessorConstant(map, attributes_, pair_);
  }
};


struct ReconfigureAsDataPropertyOperator {
  int descriptor_;
  Representation representation_;
  PropertyAttributes attributes_;
  Handle<HeapType> heap_type_;

  ReconfigureAsDataPropertyOperator(int descriptor,
                                    Representation representation,
                                    Handle<HeapType> heap_type,
                                    PropertyAttributes attributes = NONE)
      : descriptor_(descriptor),
        representation_(representation),
        attributes_(attributes),
        heap_type_(heap_type) {}

  Handle<Map> DoTransition(Expectations& expectations, Handle<Map> map) {
    expectations.SetDataField(descriptor_, representation_, heap_type_);
    return Map::ReconfigureExistingProperty(map, descriptor_, kData,
                                            attributes_);
  }
};


struct ReconfigureAsAccessorPropertyOperator {
  int descriptor_;
  PropertyAttributes attributes_;

  ReconfigureAsAccessorPropertyOperator(int descriptor,
                                        PropertyAttributes attributes = NONE)
      : descriptor_(descriptor), attributes_(attributes) {}

  Handle<Map> DoTransition(Expectations& expectations, Handle<Map> map) {
    expectations.SetAccessorField(descriptor_);
    return Map::ReconfigureExistingProperty(map, descriptor_, kAccessor,
                                            attributes_);
  }
};


// Checks that representation/field type generalization happened.
struct FieldGeneralizationChecker {
  int descriptor_;
  Representation representation_;
  PropertyAttributes attributes_;
  Handle<HeapType> heap_type_;

  FieldGeneralizationChecker(int descriptor, Representation representation,
                             Handle<HeapType> heap_type,
                             PropertyAttributes attributes = NONE)
      : descriptor_(descriptor),
        representation_(representation),
        attributes_(attributes),
        heap_type_(heap_type) {}

  void Check(Expectations& expectations2, Handle<Map> map1, Handle<Map> map2) {
    CHECK(!map2->is_deprecated());

    CHECK(map1->is_deprecated());
    CHECK_NE(*map1, *map2);
    Handle<Map> updated_map = Map::Update(map1);
    CHECK_EQ(*map2, *updated_map);

    expectations2.SetDataField(descriptor_, representation_, heap_type_);
    CHECK(expectations2.Check(*map2));
  }
};


// Checks that existing transition was taken as is.
struct SameMapChecker {
  void Check(Expectations& expectations, Handle<Map> map1, Handle<Map> map2) {
    CHECK(!map2->is_deprecated());
    CHECK_EQ(*map1, *map2);
    CHECK(expectations.Check(*map2));
  }
};


// Checks that both |map1| and |map2| should stays non-deprecated, this is
// the case when property kind is change.
struct PropertyKindReconfigurationChecker {
  void Check(Expectations& expectations, Handle<Map> map1, Handle<Map> map2) {
    CHECK(!map1->is_deprecated());
    CHECK(!map2->is_deprecated());
    CHECK_NE(*map1, *map2);
    CHECK(expectations.Check(*map2));
  }
};


// This test transitions to various property types under different
// circumstances.
// Plan:
// 1) create a |map| with p0..p3 properties.
// 2) create |map1| by adding "p4" to |map0|.
// 3) create |map2| by transition to "p4" from |map0|.
//
//                       + - p4B: |map2|
//                       |
//  {} - p0 - p1 - pA - p3: |map|
//                       |
//                       + - p4A: |map1|
//
// where "p4A" and "p4B" differ only in the attributes.
//
template <typename TransitionOp1, typename TransitionOp2, typename Checker>
static void TestTransitionTo(TransitionOp1& transition_op1,
                             TransitionOp2& transition_op2, Checker& checker) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount - 1; i++) {
    map = expectations.AddDataField(map, NONE, Representation::Smi(), any_type);
  }
  CHECK(expectations.Check(*map));

  Expectations expectations1 = expectations;
  Handle<Map> map1 = transition_op1.DoTransition(expectations1, map);
  CHECK(expectations1.Check(*map1));

  Expectations expectations2 = expectations;
  Handle<Map> map2 = transition_op2.DoTransition(expectations2, map);

  // Let the test customization do the check.
  checker.Check(expectations2, map1, map2);
}


TEST(TransitionDataFieldToDataField) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  Handle<Object> value1 = handle(Smi::FromInt(0), isolate);
  TransitionToDataFieldOperator transition_op1(Representation::Smi(), any_type,
                                               value1);

  Handle<Object> value2 = isolate->factory()->NewHeapNumber(0);
  TransitionToDataFieldOperator transition_op2(Representation::Double(),
                                               any_type, value2);

  FieldGeneralizationChecker checker(kPropCount - 1, Representation::Double(),
                                     any_type);
  TestTransitionTo(transition_op1, transition_op2, checker);
}


TEST(TransitionDataConstantToSameDataConstant) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<JSFunction> js_func = factory->NewFunction(factory->empty_string());
  TransitionToDataConstantOperator transition_op(js_func);

  SameMapChecker checker;
  TestTransitionTo(transition_op, transition_op, checker);
}


TEST(TransitionDataConstantToAnotherDataConstant) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  Handle<JSFunction> js_func1 = factory->NewFunction(factory->empty_string());
  TransitionToDataConstantOperator transition_op1(js_func1);

  Handle<JSFunction> js_func2 = factory->NewFunction(factory->empty_string());
  TransitionToDataConstantOperator transition_op2(js_func2);

  FieldGeneralizationChecker checker(kPropCount - 1,
                                     Representation::HeapObject(), any_type);
  TestTransitionTo(transition_op1, transition_op2, checker);
}


TEST(TransitionDataConstantToDataField) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Handle<HeapType> any_type = HeapType::Any(isolate);

  Handle<JSFunction> js_func1 = factory->NewFunction(factory->empty_string());
  TransitionToDataConstantOperator transition_op1(js_func1);

  Handle<Object> value2 = isolate->factory()->NewHeapNumber(0);
  TransitionToDataFieldOperator transition_op2(Representation::Double(),
                                               any_type, value2);

  FieldGeneralizationChecker checker(kPropCount - 1, Representation::Tagged(),
                                     any_type);
  TestTransitionTo(transition_op1, transition_op2, checker);
}


TEST(TransitionAccessorConstantToSameAccessorConstant) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  Handle<AccessorPair> pair = CreateAccessorPair(true, true);
  TransitionToAccessorConstantOperator transition_op(pair);

  SameMapChecker checker;
  TestTransitionTo(transition_op, transition_op, checker);
}


// TODO(ishell): add this test once IS_ACCESSOR_FIELD_SUPPORTED is supported.
// TEST(TransitionAccessorConstantToAnotherAccessorConstant)
