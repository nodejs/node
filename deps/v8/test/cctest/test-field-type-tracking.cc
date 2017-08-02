// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <utility>

#include "test/cctest/test-api.h"

#include "src/v8.h"

#include "src/compilation-cache.h"
#include "src/compilation-dependencies.h"
#include "src/compilation-info.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/field-type.h"
#include "src/global-handles.h"
#include "src/ic/stub-cache.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/property.h"
#include "src/transitions.h"

using namespace v8::internal;


// TODO(ishell): fix this once TransitionToPrototype stops generalizing
// all field representations (similar to crbug/448711 where elements kind
// and observed transitions caused generalization of all fields).
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


class Expectations {
  static const int MAX_PROPERTIES = 10;
  Isolate* isolate_;
  ElementsKind elements_kind_;
  PropertyKind kinds_[MAX_PROPERTIES];
  PropertyLocation locations_[MAX_PROPERTIES];
  PropertyConstness constnesses_[MAX_PROPERTIES];
  PropertyAttributes attributes_[MAX_PROPERTIES];
  Representation representations_[MAX_PROPERTIES];
  // FieldType for kField, value for DATA_CONSTANT and getter for
  // ACCESSOR_CONSTANT.
  Handle<Object> values_[MAX_PROPERTIES];
  // Setter for ACCESSOR_CONSTANT.
  Handle<Object> setter_values_[MAX_PROPERTIES];
  int number_of_properties_;

 public:
  explicit Expectations(Isolate* isolate, ElementsKind elements_kind)
      : isolate_(isolate),
        elements_kind_(elements_kind),
        number_of_properties_(0) {}

  explicit Expectations(Isolate* isolate)
      : Expectations(
            isolate,
            isolate->object_function()->initial_map()->elements_kind()) {}

  void Init(int index, PropertyKind kind, PropertyAttributes attributes,
            PropertyConstness constness, PropertyLocation location,
            Representation representation, Handle<Object> value) {
    CHECK(index < MAX_PROPERTIES);
    kinds_[index] = kind;
    locations_[index] = location;
    constnesses_[index] = constness;
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

      if (kinds_[i] == kData) {
        os << Brief(*values_[i]);
      } else {
        // kAccessor
        os << "(get: " << Brief(*values_[i])
           << ", set: " << Brief(*setter_values_[i]) << ") ";
      }

      os << " (";
      if (constnesses_[i] == kConst) os << "const ";
      os << (kinds_[i] == kData ? "data " : "accessor ");
      if (locations_[i] == kField) {
        os << "field"
           << ": " << representations_[i].Mnemonic();
      } else {
        os << "descriptor";
      }
      os << ", attrs: " << attributes_[i] << ")\n";
    }
    os << "\n";
  }

  void SetElementsKind(ElementsKind elements_kind) {
    elements_kind_ = elements_kind;
  }

  Handle<FieldType> GetFieldType(int index) {
    CHECK(index < MAX_PROPERTIES);
    CHECK_EQ(kField, locations_[index]);
    return Handle<FieldType>::cast(values_[index]);
  }

  void SetDataField(int index, PropertyAttributes attrs,
                    PropertyConstness constness, Representation representation,
                    Handle<FieldType> field_type) {
    Init(index, kData, attrs, constness, kField, representation, field_type);
  }

  void SetDataField(int index, PropertyConstness constness,
                    Representation representation,
                    Handle<FieldType> field_type) {
    SetDataField(index, attributes_[index], constness, representation,
                 field_type);
  }

  void SetAccessorField(int index, PropertyAttributes attrs) {
    Init(index, kAccessor, attrs, kConst, kDescriptor, Representation::Tagged(),
         FieldType::Any(isolate_));
  }

  void SetAccessorField(int index) {
    SetAccessorField(index, attributes_[index]);
  }

  void SetDataConstant(int index, PropertyAttributes attrs,
                       Handle<JSFunction> value) {
    if (FLAG_track_constant_fields) {
      Handle<FieldType> field_type(FieldType::Class(value->map()), isolate_);
      Init(index, kData, attrs, kConst, kField, Representation::HeapObject(),
           field_type);

    } else {
      Init(index, kData, attrs, kConst, kDescriptor,
           Representation::HeapObject(), value);
    }
  }

  void SetDataConstant(int index, Handle<JSFunction> value) {
    SetDataConstant(index, attributes_[index], value);
  }

  void SetAccessorConstant(int index, PropertyAttributes attrs,
                           Handle<Object> getter, Handle<Object> setter) {
    Init(index, kAccessor, attrs, kConst, kDescriptor, Representation::Tagged(),
         getter);
    setter_values_[index] = setter;
  }

  void SetAccessorConstantComponent(int index, PropertyAttributes attrs,
                                    AccessorComponent component,
                                    Handle<Object> accessor) {
    CHECK_EQ(kAccessor, kinds_[index]);
    CHECK_EQ(kDescriptor, locations_[index]);
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

  void GeneralizeField(int index) {
    CHECK(index < number_of_properties_);
    representations_[index] = Representation::Tagged();
    if (locations_[index] == kField) {
      values_[index] = FieldType::Any(isolate_);
    }
  }

  bool Check(DescriptorArray* descriptors, int descriptor) const {
    PropertyDetails details = descriptors->GetDetails(descriptor);

    if (details.kind() != kinds_[descriptor]) return false;
    if (details.location() != locations_[descriptor]) return false;
    if (details.constness() != constnesses_[descriptor]) return false;

    PropertyAttributes expected_attributes = attributes_[descriptor];
    if (details.attributes() != expected_attributes) return false;

    Representation expected_representation = representations_[descriptor];
    if (!details.representation().Equals(expected_representation)) return false;

    Object* value = descriptors->GetValue(descriptor);
    Object* expected_value = *values_[descriptor];
    if (details.location() == kField) {
      if (details.kind() == kData) {
        FieldType* type = descriptors->GetFieldType(descriptor);
        return FieldType::cast(expected_value) == type;
      } else {
        // kAccessor
        UNREACHABLE();
        return false;
      }
    } else {
      // kDescriptor
      if (details.kind() == kData) {
        CHECK(!FLAG_track_constant_fields);
        return value == expected_value;
      } else {
        // kAccessor
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
    CHECK_EQ(elements_kind_, map->elements_kind());
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

  Handle<Map> AsElementsKind(Handle<Map> map, ElementsKind elements_kind) {
    elements_kind_ = elements_kind;
    map = Map::AsElementsKind(map, elements_kind);
    CHECK_EQ(elements_kind_, map->elements_kind());
    return map;
  }

  Handle<Map> AddDataField(Handle<Map> map, PropertyAttributes attributes,
                           PropertyConstness constness,
                           Representation representation,
                           Handle<FieldType> heap_type) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    PropertyConstness expected_constness = constness;
    Representation expected_representation = representation;
    Handle<FieldType> expected_heap_type = heap_type;
    if (IsTransitionableFastElementsKind(map->elements_kind())) {
      // Maps with transitionable elements kinds must have non in-place
      // generalizable fields.
      if (FLAG_track_constant_fields && FLAG_modify_map_inplace) {
        expected_constness = kMutable;
      }
      if (representation.IsHeapObject() && heap_type->IsClass()) {
        expected_heap_type = FieldType::Any(isolate_);
      }
    }
    SetDataField(property_index, attributes, expected_constness,
                 expected_representation, expected_heap_type);

    Handle<String> name = MakeName("prop", property_index);
    return Map::CopyWithField(map, name, heap_type, attributes, constness,
                              representation, INSERT_TRANSITION)
        .ToHandleChecked();
  }

  Handle<Map> AddDataConstant(Handle<Map> map, PropertyAttributes attributes,
                              Handle<JSFunction> value) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataConstant(property_index, attributes, value);

    Handle<String> name = MakeName("prop", property_index);
    return Map::CopyWithConstant(map, name, value, attributes,
                                 INSERT_TRANSITION)
        .ToHandleChecked();
  }

  Handle<Map> TransitionToDataField(Handle<Map> map,
                                    PropertyAttributes attributes,
                                    PropertyConstness constness,
                                    Representation representation,
                                    Handle<FieldType> heap_type,
                                    Handle<Object> value) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataField(property_index, attributes, constness, representation,
                 heap_type);

    Handle<String> name = MakeName("prop", property_index);
    return Map::TransitionToDataProperty(
        map, name, value, attributes, constness,
        Object::CERTAINLY_NOT_STORE_FROM_KEYED);
  }

  Handle<Map> TransitionToDataConstant(Handle<Map> map,
                                       PropertyAttributes attributes,
                                       Handle<JSFunction> value) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataConstant(property_index, attributes, value);

    Handle<String> name = MakeName("prop", property_index);
    return Map::TransitionToDataProperty(
        map, name, value, attributes, kConst,
        Object::CERTAINLY_NOT_STORE_FROM_KEYED);
  }

  Handle<Map> FollowDataTransition(Handle<Map> map,
                                   PropertyAttributes attributes,
                                   PropertyConstness constness,
                                   Representation representation,
                                   Handle<FieldType> heap_type) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetDataField(property_index, attributes, constness, representation,
                 heap_type);

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

    Descriptor d = Descriptor::AccessorConstant(name, pair, attributes);
    return Map::CopyInsertDescriptor(map, &d, INSERT_TRANSITION);
  }

  Handle<Map> AddAccessorConstant(Handle<Map> map,
                                  PropertyAttributes attributes,
                                  Handle<Object> getter,
                                  Handle<Object> setter) {
    CHECK_EQ(number_of_properties_, map->NumberOfOwnDescriptors());
    int property_index = number_of_properties_++;
    SetAccessorConstant(property_index, attributes, getter, setter);

    Handle<String> name = MakeName("prop", property_index);

    CHECK(!getter->IsNull(isolate_) || !setter->IsNull(isolate_));
    Factory* factory = isolate_->factory();

    if (!getter->IsNull(isolate_)) {
      Handle<AccessorPair> pair = factory->NewAccessorPair();
      pair->SetComponents(*getter, *factory->null_value());
      Descriptor d = Descriptor::AccessorConstant(name, pair, attributes);
      map = Map::CopyInsertDescriptor(map, &d, INSERT_TRANSITION);
    }
    if (!setter->IsNull(isolate_)) {
      Handle<AccessorPair> pair = factory->NewAccessorPair();
      pair->SetComponents(*getter, *setter);
      Descriptor d = Descriptor::AccessorConstant(name, pair, attributes);
      map = Map::CopyInsertDescriptor(map, &d, INSERT_TRANSITION);
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

    int descriptor =
        map->instance_descriptors()->SearchWithCache(isolate, *name, *map);
    map = Map::TransitionToAccessorProperty(isolate, map, name, descriptor,
                                            getter, setter, attributes);
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
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> none_type = FieldType::None(isolate);
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
      map, 0, kData, NONE, Representation::None(), none_type);
  // |map| did not change except marked unstable.
  CHECK(!map->is_deprecated());
  CHECK(!map->is_stable());
  CHECK(expectations.Check(*map));

  // Property kind reconfiguration always makes the field mutable.
  expectations.SetDataField(0, NONE, kMutable, Representation::None(),
                            none_type);

  CHECK(!new_map->is_deprecated());
  CHECK(new_map->is_stable());
  CHECK(expectations.Check(*new_map));

  Handle<Map> new_map2 = Map::ReconfigureProperty(
      map, 0, kData, NONE, Representation::None(), none_type);
  CHECK_EQ(*new_map, *new_map2);

  Handle<Object> value(Smi::kZero, isolate);
  Handle<Map> prepared_map =
      Map::PrepareForDataProperty(new_map, 0, kConst, value);
  // None to Smi generalization is trivial, map does not change.
  CHECK_EQ(*new_map, *prepared_map);

  expectations.SetDataField(0, NONE, kMutable, Representation::Smi(), any_type);
  CHECK(prepared_map->is_stable());
  CHECK(expectations.Check(*prepared_map));

  // Now create an object with |map|, migrate it to |prepared_map| and ensure
  // that the data property is uninitialized.
  Factory* factory = isolate->factory();
  Handle<JSObject> obj = factory->NewJSObjectFromMap(map);
  JSObject::MigrateToMap(obj, prepared_map);
  FieldIndex index = FieldIndex::ForDescriptor(*prepared_map, 0);
  CHECK(obj->RawFastPropertyAt(index)->IsUninitialized(isolate));
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
  JSObject::SetOwnPropertyIgnoreAttributes(obj, foo_str, value, NONE).Check();

  // Check that the property contains |value|.
  CHECK_EQ(1, obj->map()->NumberOfOwnDescriptors());
  FieldIndex index = FieldIndex::ForDescriptor(obj->map(), 0);
  Object* the_value = obj->RawFastPropertyAt(index);
  CHECK(the_value->IsSmi());
  CHECK_EQ(42, Smi::cast(the_value)->value());
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests for field generalization case.
//

// <Constness, Representation, FieldType> data.
struct CRFTData {
  PropertyConstness constness;
  Representation representation;
  Handle<FieldType> type;
};

// This test ensures that field generalization at |property_index| is done
// correctly independently of the fact that the |map| is detached from
// transition tree or not.
//
//  {} - p0 - p1 - p2: |detach_point_map|
//                  |
//                  X - detached at |detach_property_at_index|
//                  |
//                  + - p3 - p4: |map|
//
// Detaching does not happen if |detach_property_at_index| is -1.
//
static void TestGeneralizeField(int detach_property_at_index,
                                int property_index, const CRFTData& from,
                                const CRFTData& to, const CRFTData& expected,
                                bool expected_deprecation,
                                bool expected_field_type_dependency) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

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
      map = expectations.AddDataField(map, NONE, from.constness,
                                      from.representation, from.type);
    } else {
      map = expectations.AddDataField(map, NONE, kDefaultFieldConstness,
                                      Representation::Smi(), any_type);
      if (i == detach_property_at_index) {
        detach_point_map = map;
      }
    }
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  Zone zone(isolate->allocator(), ZONE_NAME);

  if (is_detached_map) {
    detach_point_map = Map::ReconfigureProperty(
        detach_point_map, detach_property_at_index, kData, NONE,
        Representation::Tagged(), any_type);
    expectations.SetDataField(detach_property_at_index, kDefaultFieldConstness,
                              Representation::Tagged(), any_type);
    CHECK(map->is_deprecated());
    CHECK(expectations.Check(*detach_point_map,
                             detach_point_map->NumberOfOwnDescriptors()));
  }

  // Create new maps by generalizing representation of propX field.
  Handle<Map> field_owner(map->FindFieldOwner(property_index), isolate);
  CompilationDependencies dependencies(isolate, &zone);
  CHECK(!dependencies.HasAborted());

  dependencies.AssumeFieldOwner(field_owner);

  Handle<Map> new_map = Map::ReconfigureProperty(
      map, property_index, kData, NONE, to.representation, to.type);

  expectations.SetDataField(property_index, expected.constness,
                            expected.representation, expected.type);

  CHECK(!new_map->is_deprecated());
  CHECK(expectations.Check(*new_map));

  if (is_detached_map) {
    CHECK(!map->is_stable());
    CHECK(map->is_deprecated());
    CHECK_NE(*map, *new_map);
    CHECK_EQ(expected_field_type_dependency && !field_owner->is_deprecated(),
             dependencies.HasAborted());

  } else if (expected_deprecation) {
    CHECK(!map->is_stable());
    CHECK(map->is_deprecated());
    CHECK(field_owner->is_deprecated());
    CHECK_NE(*map, *new_map);
    CHECK(!dependencies.HasAborted());

  } else {
    CHECK(!field_owner->is_deprecated());
    CHECK(map->is_stable());  // Map did not change, must be left stable.
    CHECK_EQ(*map, *new_map);

    CHECK_EQ(expected_field_type_dependency, dependencies.HasAborted());
  }

  {
    // Check that all previous maps are not stable.
    Map* tmp = *new_map;
    while (true) {
      Object* back = tmp->GetBackPointer();
      if (back->IsUndefined(isolate)) break;
      tmp = Map::cast(back);
      CHECK(!tmp->is_stable());
    }
  }

  dependencies.Rollback();  // Properly cleanup compilation info.

  // Update all deprecated maps and check that they are now the same.
  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*new_map, *updated_map);
}

static void TestGeneralizeField(const CRFTData& from, const CRFTData& to,
                                const CRFTData& expected,
                                bool expected_deprecation,
                                bool expected_field_type_dependency) {
  // Check the cases when the map being reconfigured is a part of the
  // transition tree.
  STATIC_ASSERT(kPropCount > 4);
  int indices[] = {0, 2, kPropCount - 1};
  for (int i = 0; i < static_cast<int>(arraysize(indices)); i++) {
    TestGeneralizeField(-1, indices[i], from, to, expected,
                        expected_deprecation, expected_field_type_dependency);
  }

  if (!from.representation.IsNone()) {
    // Check the cases when the map being reconfigured is NOT a part of the
    // transition tree. "None -> anything" representation changes make sense
    // only for "attached" maps.
    int indices[] = {0, kPropCount - 1};
    for (int i = 0; i < static_cast<int>(arraysize(indices)); i++) {
      TestGeneralizeField(indices[i], 2, from, to, expected,
                          expected_deprecation, expected_field_type_dependency);
    }

    // Check that reconfiguration to the very same field works correctly.
    CRFTData data = from;
    TestGeneralizeField(-1, 2, data, data, data, false, false);
  }
}

static void TestGeneralizeField(const CRFTData& from, const CRFTData& to,
                                const CRFTData& expected) {
  const bool expected_deprecation = true;
  const bool expected_field_type_dependency = false;

  TestGeneralizeField(from, to, expected, expected_deprecation,
                      expected_field_type_dependency);
}

static void TestGeneralizeFieldTrivial(
    const CRFTData& from, const CRFTData& to, const CRFTData& expected,
    bool expected_field_type_dependency = true) {
  const bool expected_deprecation = false;

  TestGeneralizeField(from, to, expected, expected_deprecation,
                      expected_field_type_dependency);
}

TEST(GeneralizeSmiFieldToDouble) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  TestGeneralizeField({kMutable, Representation::Smi(), any_type},
                      {kMutable, Representation::Double(), any_type},
                      {kMutable, Representation::Double(), any_type});
}

TEST(GeneralizeSmiFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  TestGeneralizeField({kMutable, Representation::Smi(), any_type},
                      {kMutable, Representation::HeapObject(), value_type},
                      {kMutable, Representation::Tagged(), any_type});
}

TEST(GeneralizeDoubleFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  TestGeneralizeField({kMutable, Representation::Double(), any_type},
                      {kMutable, Representation::HeapObject(), value_type},
                      {kMutable, Representation::Tagged(), any_type});
}

TEST(GeneralizeHeapObjectFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  TestGeneralizeField({kMutable, Representation::HeapObject(), value_type},
                      {kMutable, Representation::Smi(), any_type},
                      {kMutable, Representation::Tagged(), any_type});
}

TEST(GeneralizeHeapObjectFieldToHeapObject) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Handle<FieldType> current_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  Handle<FieldType> new_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  Handle<FieldType> expected_type = any_type;

  TestGeneralizeFieldTrivial(
      {kMutable, Representation::HeapObject(), current_type},
      {kMutable, Representation::HeapObject(), new_type},
      {kMutable, Representation::HeapObject(), expected_type});
  current_type = expected_type;

  new_type = FieldType::Class(Map::Create(isolate, 0), isolate);

  TestGeneralizeFieldTrivial({kMutable, Representation::HeapObject(), any_type},
                             {kMutable, Representation::HeapObject(), new_type},
                             {kMutable, Representation::HeapObject(), any_type},
                             false);
}

TEST(GeneralizeNoneFieldToSmi) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> none_type = FieldType::None(isolate);
  Handle<FieldType> any_type = FieldType::Any(isolate);

  // None -> Smi representation change is trivial.
  TestGeneralizeFieldTrivial({kMutable, Representation::None(), none_type},
                             {kMutable, Representation::Smi(), any_type},
                             {kMutable, Representation::Smi(), any_type});
}

TEST(GeneralizeNoneFieldToDouble) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> none_type = FieldType::None(isolate);
  Handle<FieldType> any_type = FieldType::Any(isolate);

  // None -> Double representation change is NOT trivial.
  TestGeneralizeField({kMutable, Representation::None(), none_type},
                      {kMutable, Representation::Double(), any_type},
                      {kMutable, Representation::Double(), any_type});
}

TEST(GeneralizeNoneFieldToHeapObject) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> none_type = FieldType::None(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  // None -> HeapObject representation change is trivial.
  TestGeneralizeFieldTrivial(
      {kMutable, Representation::None(), none_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::HeapObject(), value_type});
}

TEST(GeneralizeNoneFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> none_type = FieldType::None(isolate);
  Handle<FieldType> any_type = FieldType::Any(isolate);

  // None -> HeapObject representation change is trivial.
  TestGeneralizeFieldTrivial({kMutable, Representation::None(), none_type},
                             {kMutable, Representation::Tagged(), any_type},
                             {kMutable, Representation::Tagged(), any_type});
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests for field generalization case with kAccessor properties.
//

TEST(GeneralizeFieldWithAccessorProperties) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
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
      map = expectations.AddDataField(map, NONE, kMutable,
                                      Representation::Smi(), any_type);
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
        map, i, kData, NONE, Representation::Double(), any_type);
    maps[i] = new_map;

    expectations.SetDataField(i, kMutable, Representation::Double(), any_type);

    CHECK(!map->is_stable());
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

// This test ensures that field generalization is correctly propagated from one
// branch of transition tree (|map2|) to another (|map|).
//
//             + - p2B - p3 - p4: |map2|
//             |
//  {} - p0 - p1 - p2A - p3 - p4: |map|
//
// where "p2A" and "p2B" differ only in the attributes.
//
static void TestReconfigureDataFieldAttribute_GeneralizeField(
    const CRFTData& from, const CRFTData& to, const CRFTData& expected) {
  Isolate* isolate = CcTest::i_isolate();

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, from.constness,
                                    from.representation, from.type);
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
    map2 = expectations2.FollowDataTransition(map2, NONE, from.constness,
                                              from.representation, from.type);
  }
  map2 = expectations2.AddDataField(map2, READ_ONLY, to.constness,
                                    to.representation, to.type);

  for (int i = kSplitProp + 1; i < kPropCount; i++) {
    map2 = expectations2.AddDataField(map2, NONE, to.constness,
                                      to.representation, to.type);
  }
  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));

  Zone zone(isolate->allocator(), ZONE_NAME);
  Handle<Map> field_owner(map->FindFieldOwner(kSplitProp), isolate);
  CompilationDependencies dependencies(isolate, &zone);
  CHECK(!dependencies.HasAborted());
  dependencies.AssumeFieldOwner(field_owner);

  // Reconfigure attributes of property |kSplitProp| of |map2| to NONE, which
  // should generalize representations in |map1|.
  Handle<Map> new_map =
      Map::ReconfigureExistingProperty(map2, kSplitProp, kData, NONE);

  // |map2| should be left unchanged but marked unstable.
  CHECK(!map2->is_stable());
  CHECK(!map2->is_deprecated());
  CHECK_NE(*map2, *new_map);
  CHECK(expectations2.Check(*map2));

  // |map| should be deprecated and |new_map| should match new expectations.
  for (int i = kSplitProp; i < kPropCount; i++) {
    expectations.SetDataField(i, expected.constness, expected.representation,
                              expected.type);
  }
  CHECK(map->is_deprecated());
  CHECK(!dependencies.HasAborted());
  dependencies.Rollback();  // Properly cleanup compilation info.
  CHECK_NE(*map, *new_map);

  CHECK(!new_map->is_deprecated());
  CHECK(expectations.Check(*new_map));

  // Update deprecated |map|, it should become |new_map|.
  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*new_map, *updated_map);
}

// This test ensures that trivial field generalization (from HeapObject to
// HeapObject) is correctly propagated from one branch of transition tree
// (|map2|) to another (|map|).
//
//             + - p2B - p3 - p4: |map2|
//             |
//  {} - p0 - p1 - p2A - p3 - p4: |map|
//
// where "p2A" and "p2B" differ only in the attributes.
//
static void TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
    const CRFTData& from, const CRFTData& to, const CRFTData& expected,
    bool expected_field_type_dependency = true) {
  Isolate* isolate = CcTest::i_isolate();

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, from.constness,
                                    from.representation, from.type);
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
    map2 = expectations2.FollowDataTransition(map2, NONE, from.constness,
                                              from.representation, from.type);
  }
  map2 = expectations2.AddDataField(map2, READ_ONLY, to.constness,
                                    to.representation, to.type);

  for (int i = kSplitProp + 1; i < kPropCount; i++) {
    map2 = expectations2.AddDataField(map2, NONE, to.constness,
                                      to.representation, to.type);
  }
  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));

  Zone zone(isolate->allocator(), ZONE_NAME);
  Handle<Map> field_owner(map->FindFieldOwner(kSplitProp), isolate);
  CompilationDependencies dependencies(isolate, &zone);
  CHECK(!dependencies.HasAborted());
  dependencies.AssumeFieldOwner(field_owner);

  // Reconfigure attributes of property |kSplitProp| of |map2| to NONE, which
  // should generalize representations in |map1|.
  Handle<Map> new_map =
      Map::ReconfigureExistingProperty(map2, kSplitProp, kData, NONE);

  // |map2| should be left unchanged but marked unstable.
  CHECK(!map2->is_stable());
  CHECK(!map2->is_deprecated());
  CHECK_NE(*map2, *new_map);
  CHECK(expectations2.Check(*map2));

  // In trivial case |map| should be returned as a result of the property
  // reconfiguration, respective field types should be generalized and
  // respective code dependencies should be invalidated. |map| should be NOT
  // deprecated and it should match new expectations.
  for (int i = kSplitProp; i < kPropCount; i++) {
    expectations.SetDataField(i, expected.constness, expected.representation,
                              expected.type);
  }
  CHECK(!map->is_deprecated());
  CHECK_EQ(*map, *new_map);
  CHECK_EQ(expected_field_type_dependency, dependencies.HasAborted());
  dependencies.Rollback();  // Properly cleanup compilation info.

  CHECK(!new_map->is_deprecated());
  CHECK(expectations.Check(*new_map));

  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*new_map, *updated_map);
}

TEST(ReconfigureDataFieldAttribute_GeneralizeSmiFieldToDouble) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kConst, Representation::Smi(), any_type},
        {kConst, Representation::Double(), any_type},
        {kConst, Representation::Double(), any_type});

    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kConst, Representation::Smi(), any_type},
        {kMutable, Representation::Double(), any_type},
        {kMutable, Representation::Double(), any_type});

    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kMutable, Representation::Smi(), any_type},
        {kConst, Representation::Double(), any_type},
        {kMutable, Representation::Double(), any_type});
  }

  TestReconfigureDataFieldAttribute_GeneralizeField(
      {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::Double(), any_type},
      {kMutable, Representation::Double(), any_type});
}

TEST(ReconfigureDataFieldAttribute_GeneralizeSmiFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kConst, Representation::Smi(), any_type},
        {kConst, Representation::HeapObject(), value_type},
        {kConst, Representation::Tagged(), any_type});

    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kConst, Representation::Smi(), any_type},
        {kMutable, Representation::HeapObject(), value_type},
        {kMutable, Representation::Tagged(), any_type});

    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kMutable, Representation::Smi(), any_type},
        {kConst, Representation::HeapObject(), value_type},
        {kMutable, Representation::Tagged(), any_type});
  }

  TestReconfigureDataFieldAttribute_GeneralizeField(
      {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Tagged(), any_type});
}

TEST(ReconfigureDataFieldAttribute_GeneralizeDoubleFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kConst, Representation::Double(), any_type},
        {kConst, Representation::HeapObject(), value_type},
        {kConst, Representation::Tagged(), any_type});

    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kConst, Representation::Double(), any_type},
        {kMutable, Representation::HeapObject(), value_type},
        {kMutable, Representation::Tagged(), any_type});

    TestReconfigureDataFieldAttribute_GeneralizeField(
        {kMutable, Representation::Double(), any_type},
        {kConst, Representation::HeapObject(), value_type},
        {kMutable, Representation::Tagged(), any_type});
  }

  TestReconfigureDataFieldAttribute_GeneralizeField(
      {kMutable, Representation::Double(), any_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Tagged(), any_type});
}

TEST(ReconfigureDataFieldAttribute_GeneralizeHeapObjFieldToHeapObj) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Handle<FieldType> current_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  Handle<FieldType> new_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  Handle<FieldType> expected_type = any_type;

  // Check generalizations that trigger deopts.
  if (FLAG_track_constant_fields) {
    TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
        {kConst, Representation::HeapObject(), current_type},
        {kConst, Representation::HeapObject(), new_type},
        {kConst, Representation::HeapObject(), expected_type});

    if (FLAG_modify_map_inplace) {
      // kConst to kMutable migration does not create a new map, therefore
      // trivial generalization.
      TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
          {kConst, Representation::HeapObject(), current_type},
          {kMutable, Representation::HeapObject(), new_type},
          {kMutable, Representation::HeapObject(), expected_type});
    } else {
      // kConst to kMutable migration causes map change, therefore
      // non-trivial generalization.
      TestReconfigureDataFieldAttribute_GeneralizeField(
          {kConst, Representation::HeapObject(), current_type},
          {kMutable, Representation::HeapObject(), new_type},
          {kMutable, Representation::HeapObject(), expected_type});
    }
    TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
        {kMutable, Representation::HeapObject(), current_type},
        {kConst, Representation::HeapObject(), new_type},
        {kMutable, Representation::HeapObject(), expected_type});
  }
  TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
      {kMutable, Representation::HeapObject(), current_type},
      {kMutable, Representation::HeapObject(), new_type},
      {kMutable, Representation::HeapObject(), expected_type});
  current_type = expected_type;

  // Check generalizations that do not trigger deopts.
  new_type = FieldType::Class(Map::Create(isolate, 0), isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
        {kConst, Representation::HeapObject(), any_type},
        {kConst, Representation::HeapObject(), new_type},
        {kConst, Representation::HeapObject(), any_type}, false);

    if (FLAG_modify_map_inplace) {
      // kConst to kMutable migration does not create a new map, therefore
      // trivial generalization.
      TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
          {kConst, Representation::HeapObject(), any_type},
          {kMutable, Representation::HeapObject(), new_type},
          {kMutable, Representation::HeapObject(), any_type});
    } else {
      // kConst to kMutable migration causes map change, therefore
      // non-trivial generalization.
      TestReconfigureDataFieldAttribute_GeneralizeField(
          {kConst, Representation::HeapObject(), any_type},
          {kMutable, Representation::HeapObject(), new_type},
          {kMutable, Representation::HeapObject(), any_type});
    }
    TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
        {kMutable, Representation::HeapObject(), any_type},
        {kConst, Representation::HeapObject(), new_type},
        {kMutable, Representation::HeapObject(), any_type}, false);
  }
  TestReconfigureDataFieldAttribute_GeneralizeFieldTrivial(
      {kMutable, Representation::HeapObject(), any_type},
      {kMutable, Representation::HeapObject(), new_type},
      {kMutable, Representation::HeapObject(), any_type}, false);
}

TEST(ReconfigureDataFieldAttribute_GeneralizeHeapObjectFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  TestReconfigureDataFieldAttribute_GeneralizeField(
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::Tagged(), any_type});
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
    // |map| was not reconfigured, therefore it should stay stable.
    CHECK(map->is_stable());
    CHECK(!map->is_deprecated());
    CHECK_EQ(*map, *new_map);

    CHECK(!new_map->is_deprecated());
    CHECK(expectations.Check(*new_map));

    // Update deprecated |map|, it should become |new_map|.
    Handle<Map> updated_map = Map::Update(map);
    CHECK_EQ(*new_map, *updated_map);
  }
};


// Checks that given |map| is NOT deprecated and matches expectations.
// |new_map| is unrelated to |map|.
struct CheckUnrelated {
  void Check(Handle<Map> map, Handle<Map> new_map,
             const Expectations& expectations) {
    CHECK(!map->is_deprecated());
    CHECK_NE(*map, *new_map);
    CHECK(expectations.Check(*map));

    CHECK(new_map->is_stable());
    CHECK(!new_map->is_deprecated());
  }
};


// Checks that given |map| is NOT deprecated, and |new_map| is a result of
// copy-generalize-all-representations.
struct CheckCopyGeneralizeAllFields {
  void Check(Handle<Map> map, Handle<Map> new_map, Expectations& expectations) {
    CHECK(!map->is_deprecated());
    CHECK_NE(*map, *new_map);

    CHECK(new_map->GetBackPointer()->IsUndefined(map->GetIsolate()));
    for (int i = 0; i < kPropCount; i++) {
      expectations.GeneralizeField(i);
    }

    CHECK(!new_map->is_deprecated());
    CHECK(expectations.Check(*new_map));
  }
};

// This test ensures that field generalization is correctly propagated from one
// branch of transition tree (|map2|) to another (|map1|).
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
  Handle<FieldType> any_type = FieldType::Any(isolate);

  const int kCustomPropIndex = kPropCount - 2;
  Expectations expectations(isolate);

  const int kSplitProp = 2;
  CHECK(kSplitProp < kCustomPropIndex);

  const PropertyConstness constness = kMutable;
  const Representation representation = Representation::Smi();

  // Create common part of transition tree.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kSplitProp; i++) {
    map = expectations.AddDataField(map, NONE, constness, representation,
                                    any_type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));


  // Create branch to |map1|.
  Handle<Map> map1 = map;
  Expectations expectations1 = expectations;
  for (int i = kSplitProp; i < kCustomPropIndex; i++) {
    map1 = expectations1.AddDataField(map1, NONE, constness, representation,
                                      any_type);
  }
  map1 = config.AddPropertyAtBranch(1, expectations1, map1);
  for (int i = kCustomPropIndex + 1; i < kPropCount; i++) {
    map1 = expectations1.AddDataField(map1, NONE, constness, representation,
                                      any_type);
  }
  CHECK(!map1->is_deprecated());
  CHECK(map1->is_stable());
  CHECK(expectations1.Check(*map1));


  // Create another branch in transition tree (property at index |kSplitProp|
  // has different attributes), initialize expectations.
  Handle<Map> map2 = map;
  Expectations expectations2 = expectations;
  map2 = expectations2.AddDataField(map2, READ_ONLY, constness, representation,
                                    any_type);
  for (int i = kSplitProp + 1; i < kCustomPropIndex; i++) {
    map2 = expectations2.AddDataField(map2, NONE, constness, representation,
                                      any_type);
  }
  map2 = config.AddPropertyAtBranch(2, expectations2, map2);
  for (int i = kCustomPropIndex + 1; i < kPropCount; i++) {
    map2 = expectations2.AddDataField(map2, NONE, constness, representation,
                                      any_type);
  }
  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));


  // Reconfigure attributes of property |kSplitProp| of |map2| to NONE, which
  // should generalize representations in |map1|.
  Handle<Map> new_map =
      Map::ReconfigureExistingProperty(map2, kSplitProp, kData, NONE);

  // |map2| should be left unchanged but marked unstable.
  CHECK(!map2->is_stable());
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
    Handle<FieldType> function_type_;
    TestConfig() {
      Isolate* isolate = CcTest::i_isolate();
      Factory* factory = isolate->factory();
      Handle<String> name = factory->empty_string();
      Handle<Map> sloppy_map =
          factory->CreateSloppyFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE);
      Handle<SharedFunctionInfo> info = factory->NewSharedFunctionInfo(
          name, MaybeHandle<Code>(), sloppy_map->is_constructor());
      function_type_ = FieldType::Class(sloppy_map, isolate);
      CHECK(sloppy_map->is_stable());

      js_func1_ =
          factory->NewFunction(sloppy_map, info, isolate->native_context());

      js_func2_ =
          factory->NewFunction(sloppy_map, info, isolate->native_context());
    }

    Handle<Map> AddPropertyAtBranch(int branch_id, Expectations& expectations,
                                    Handle<Map> map) {
      CHECK(branch_id == 1 || branch_id == 2);
      Handle<JSFunction> js_func = branch_id == 1 ? js_func1_ : js_func2_;
      return expectations.AddDataConstant(map, NONE, js_func);
    }

    void UpdateExpectations(int property_index, Expectations& expectations) {
      PropertyConstness expected_constness =
          FLAG_track_constant_fields ? kConst : kMutable;
      expectations.SetDataField(property_index, expected_constness,
                                Representation::HeapObject(), function_type_);
    }
  };

  TestConfig config;
  if (FLAG_track_constant_fields) {
    CheckSameMap checker;
    TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);

  } else {
    // Two branches are "incompatible" so the |map1| should be deprecated.
    CheckDeprecated checker;
    TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
  }
}


TEST(ReconfigureDataFieldAttribute_DataConstantToAccConstantAfterTargetMap) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  struct TestConfig {
    Handle<JSFunction> js_func_;
    Handle<AccessorPair> pair_;
    TestConfig() {
      Isolate* isolate = CcTest::i_isolate();
      Factory* factory = isolate->factory();
      js_func_ = factory->NewFunction(factory->empty_string());
      pair_ = CreateAccessorPair(true, true);
    }

    Handle<Map> AddPropertyAtBranch(int branch_id, Expectations& expectations,
                                    Handle<Map> map) {
      CHECK(branch_id == 1 || branch_id == 2);
      if (branch_id == 1) {
        return expectations.AddDataConstant(map, NONE, js_func_);
      } else {
        return expectations.AddAccessorConstant(map, NONE, pair_);
      }
    }

    void UpdateExpectations(int property_index, Expectations& expectations) {}
  };

  TestConfig config;
  // These are completely separate branches in transition tree.
  CheckUnrelated checker;
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

    void UpdateExpectations(int property_index, Expectations& expectations) {
      // Two branches are "compatible" so the |map1| should NOT be deprecated.
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
    CheckCopyGeneralizeAllFields checker;
    TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
  } else {
    // Currently we have a copy-generalize-all-representations case.
    CheckCopyGeneralizeAllFields checker;
    TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
  }
}


TEST(ReconfigureDataFieldAttribute_AccConstantToDataFieldAfterTargetMap) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  struct TestConfig {
    Handle<AccessorPair> pair_;
    TestConfig() { pair_ = CreateAccessorPair(true, true); }

    Handle<Map> AddPropertyAtBranch(int branch_id, Expectations& expectations,
                                    Handle<Map> map) {
      CHECK(branch_id == 1 || branch_id == 2);
      if (branch_id == 1) {
        return expectations.AddAccessorConstant(map, NONE, pair_);
      } else {
        Isolate* isolate = CcTest::i_isolate();
        Handle<FieldType> any_type = FieldType::Any(isolate);
        return expectations.AddDataField(map, NONE, kDefaultFieldConstness,
                                         Representation::Smi(), any_type);
      }
    }

    void UpdateExpectations(int property_index, Expectations& expectations) {}
  };

  TestConfig config;
  // These are completely separate branches in transition tree.
  CheckUnrelated checker;
  TestReconfigureProperty_CustomPropertyAfterTargetMap(config, checker);
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests for elements kind reconfiguration case.
//

// This test ensures that field generalization is correctly propagated from one
// branch of transition tree (|map2) to another (|map|).
//
//   + - p0 - p1 - p2A - p3 - p4: |map|
//   |
//  ek
//   |
//  {} - p0 - p1 - p2B - p3 - p4: |map2|
//
// where "p2A" and "p2B" differ only in the representation/field type.
//
static void TestReconfigureElementsKind_GeneralizeField(
    const CRFTData& from, const CRFTData& to, const CRFTData& expected) {
  Isolate* isolate = CcTest::i_isolate();

  Expectations expectations(isolate, FAST_SMI_ELEMENTS);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  initial_map->set_elements_kind(FAST_SMI_ELEMENTS);

  Handle<Map> map = initial_map;
  map = expectations.AsElementsKind(map, FAST_ELEMENTS);
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, from.constness,
                                    from.representation, from.type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  // Create another branch in transition tree (property at index |kDiffProp|
  // has different representatio/field type), initialize expectations.
  const int kDiffProp = kPropCount / 2;
  Expectations expectations2(isolate, FAST_SMI_ELEMENTS);

  Handle<Map> map2 = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    if (i == kDiffProp) {
      map2 = expectations2.AddDataField(map2, NONE, to.constness,
                                        to.representation, to.type);
    } else {
      map2 = expectations2.AddDataField(map2, NONE, from.constness,
                                        from.representation, from.type);
    }
  }
  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));

  Zone zone(isolate->allocator(), ZONE_NAME);
  Handle<Map> field_owner(map->FindFieldOwner(kDiffProp), isolate);
  CompilationDependencies dependencies(isolate, &zone);
  CHECK(!dependencies.HasAborted());
  dependencies.AssumeFieldOwner(field_owner);

  // Reconfigure elements kinds of |map2|, which should generalize
  // representations in |map|.
  Handle<Map> new_map = Map::ReconfigureElementsKind(map2, FAST_ELEMENTS);

  // |map2| should be left unchanged but marked unstable.
  CHECK(!map2->is_stable());
  CHECK(!map2->is_deprecated());
  CHECK_NE(*map2, *new_map);
  CHECK(expectations2.Check(*map2));

  // |map| should be deprecated and |new_map| should match new expectations.
  expectations.SetDataField(kDiffProp, expected.constness,
                            expected.representation, expected.type);

  CHECK(map->is_deprecated());
  CHECK(!dependencies.HasAborted());
  dependencies.Rollback();  // Properly cleanup compilation info.
  CHECK_NE(*map, *new_map);

  CHECK(!new_map->is_deprecated());
  CHECK(expectations.Check(*new_map));

  // Update deprecated |map|, it should become |new_map|.
  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*new_map, *updated_map);

  // Ensure Map::FindElementsKindTransitionedMap() is able to find the
  // transitioned map.
  {
    MapHandles map_list;
    map_list.push_back(updated_map);
    Map* transitioned_map = map2->FindElementsKindTransitionedMap(map_list);
    CHECK_EQ(*updated_map, transitioned_map);
  }
}

// This test ensures that trivial field generalization (from HeapObject to
// HeapObject) is correctly propagated from one branch of transition tree
// (|map2|) to another (|map|).
//
//   + - p0 - p1 - p2A - p3 - p4: |map|
//   |
//  ek
//   |
//  {} - p0 - p1 - p2B - p3 - p4: |map2|
//
// where "p2A" and "p2B" differ only in the representation/field type.
//
static void TestReconfigureElementsKind_GeneralizeFieldTrivial(
    const CRFTData& from, const CRFTData& to, const CRFTData& expected) {
  Isolate* isolate = CcTest::i_isolate();

  Expectations expectations(isolate, FAST_SMI_ELEMENTS);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  initial_map->set_elements_kind(FAST_SMI_ELEMENTS);

  Handle<Map> map = initial_map;
  map = expectations.AsElementsKind(map, FAST_ELEMENTS);
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, from.constness,
                                    from.representation, from.type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  // Create another branch in transition tree (property at index |kDiffProp|
  // has different attributes), initialize expectations.
  const int kDiffProp = kPropCount / 2;
  Expectations expectations2(isolate, FAST_SMI_ELEMENTS);

  Handle<Map> map2 = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    if (i == kDiffProp) {
      map2 = expectations2.AddDataField(map2, NONE, to.constness,
                                        to.representation, to.type);
    } else {
      map2 = expectations2.AddDataField(map2, NONE, from.constness,
                                        from.representation, from.type);
    }
  }
  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));

  Zone zone(isolate->allocator(), ZONE_NAME);
  Handle<Map> field_owner(map->FindFieldOwner(kDiffProp), isolate);
  CompilationDependencies dependencies(isolate, &zone);
  CHECK(!dependencies.HasAborted());
  dependencies.AssumeFieldOwner(field_owner);

  // Reconfigure elements kinds of |map2|, which should generalize
  // representations in |map|.
  Handle<Map> new_map = Map::ReconfigureElementsKind(map2, FAST_ELEMENTS);

  // |map2| should be left unchanged but marked unstable.
  CHECK(!map2->is_stable());
  CHECK(!map2->is_deprecated());
  CHECK_NE(*map2, *new_map);
  CHECK(expectations2.Check(*map2));

  // In trivial case |map| should be returned as a result of the elements
  // kind reconfiguration, respective field types should be generalized and
  // respective code dependencies should be invalidated. |map| should be NOT
  // deprecated and it should match new expectations.
  expectations.SetDataField(kDiffProp, expected.constness,
                            expected.representation, expected.type);
  CHECK(!map->is_deprecated());
  CHECK_EQ(*map, *new_map);
  CHECK(!dependencies.HasAborted());
  dependencies.Rollback();  // Properly cleanup compilation info.

  CHECK(!new_map->is_deprecated());
  CHECK(expectations.Check(*new_map));

  Handle<Map> updated_map = Map::Update(map);
  CHECK_EQ(*new_map, *updated_map);

  // Ensure Map::FindElementsKindTransitionedMap() is able to find the
  // transitioned map.
  {
    MapHandles map_list;
    map_list.push_back(updated_map);
    Map* transitioned_map = map2->FindElementsKindTransitionedMap(map_list);
    CHECK_EQ(*updated_map, transitioned_map);
  }
}

TEST(ReconfigureElementsKind_GeneralizeSmiFieldToDouble) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureElementsKind_GeneralizeField(
        {kConst, Representation::Smi(), any_type},
        {kConst, Representation::Double(), any_type},
        {kConst, Representation::Double(), any_type});

    TestReconfigureElementsKind_GeneralizeField(
        {kConst, Representation::Smi(), any_type},
        {kMutable, Representation::Double(), any_type},
        {kMutable, Representation::Double(), any_type});

    TestReconfigureElementsKind_GeneralizeField(
        {kMutable, Representation::Smi(), any_type},
        {kConst, Representation::Double(), any_type},
        {kMutable, Representation::Double(), any_type});
  }
  TestReconfigureElementsKind_GeneralizeField(
      {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::Double(), any_type},
      {kMutable, Representation::Double(), any_type});
}

TEST(ReconfigureElementsKind_GeneralizeSmiFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureElementsKind_GeneralizeField(
        {kConst, Representation::Smi(), any_type},
        {kConst, Representation::HeapObject(), value_type},
        {kConst, Representation::Tagged(), any_type});

    TestReconfigureElementsKind_GeneralizeField(
        {kConst, Representation::Smi(), any_type},
        {kMutable, Representation::HeapObject(), value_type},
        {kMutable, Representation::Tagged(), any_type});

    TestReconfigureElementsKind_GeneralizeField(
        {kMutable, Representation::Smi(), any_type},
        {kConst, Representation::HeapObject(), value_type},
        {kMutable, Representation::Tagged(), any_type});
  }
  TestReconfigureElementsKind_GeneralizeField(
      {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Tagged(), any_type});
}

TEST(ReconfigureElementsKind_GeneralizeDoubleFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureElementsKind_GeneralizeField(
        {kConst, Representation::Double(), any_type},
        {kConst, Representation::HeapObject(), value_type},
        {kConst, Representation::Tagged(), any_type});

    TestReconfigureElementsKind_GeneralizeField(
        {kConst, Representation::Double(), any_type},
        {kMutable, Representation::HeapObject(), value_type},
        {kMutable, Representation::Tagged(), any_type});

    TestReconfigureElementsKind_GeneralizeField(
        {kMutable, Representation::Double(), any_type},
        {kConst, Representation::HeapObject(), value_type},
        {kMutable, Representation::Tagged(), any_type});
  }
  TestReconfigureElementsKind_GeneralizeField(
      {kMutable, Representation::Double(), any_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Tagged(), any_type});
}

TEST(ReconfigureElementsKind_GeneralizeHeapObjFieldToHeapObj) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Handle<FieldType> current_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  Handle<FieldType> new_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  Handle<FieldType> expected_type = any_type;

  // Check generalizations that trigger deopts.
  if (FLAG_track_constant_fields) {
    TestReconfigureElementsKind_GeneralizeFieldTrivial(
        {kConst, Representation::HeapObject(), current_type},
        {kConst, Representation::HeapObject(), new_type},
        {kConst, Representation::HeapObject(), expected_type});

    if (FLAG_modify_map_inplace) {
      // kConst to kMutable migration does not create a new map, therefore
      // trivial generalization.
      TestReconfigureElementsKind_GeneralizeFieldTrivial(
          {kConst, Representation::HeapObject(), current_type},
          {kMutable, Representation::HeapObject(), new_type},
          {kMutable, Representation::HeapObject(), expected_type});
    } else {
      // kConst to kMutable migration causes map change, therefore
      // non-trivial generalization.
      TestReconfigureElementsKind_GeneralizeField(
          {kConst, Representation::HeapObject(), current_type},
          {kMutable, Representation::HeapObject(), new_type},
          {kMutable, Representation::HeapObject(), expected_type});
    }
    TestReconfigureElementsKind_GeneralizeFieldTrivial(
        {kMutable, Representation::HeapObject(), current_type},
        {kConst, Representation::HeapObject(), new_type},
        {kMutable, Representation::HeapObject(), expected_type});
  }
  TestReconfigureElementsKind_GeneralizeFieldTrivial(
      {kMutable, Representation::HeapObject(), current_type},
      {kMutable, Representation::HeapObject(), new_type},
      {kMutable, Representation::HeapObject(), expected_type});
  current_type = expected_type;

  // Check generalizations that do not trigger deopts.
  new_type = FieldType::Class(Map::Create(isolate, 0), isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureElementsKind_GeneralizeFieldTrivial(
        {kConst, Representation::HeapObject(), any_type},
        {kConst, Representation::HeapObject(), new_type},
        {kConst, Representation::HeapObject(), any_type});

    if (FLAG_modify_map_inplace) {
      // kConst to kMutable migration does not create a new map, therefore
      // trivial generalization.
      TestReconfigureElementsKind_GeneralizeFieldTrivial(
          {kConst, Representation::HeapObject(), any_type},
          {kMutable, Representation::HeapObject(), new_type},
          {kMutable, Representation::HeapObject(), any_type});
    } else {
      // kConst to kMutable migration causes map change, therefore
      // non-trivial generalization.
      TestReconfigureElementsKind_GeneralizeField(
          {kConst, Representation::HeapObject(), any_type},
          {kMutable, Representation::HeapObject(), new_type},
          {kMutable, Representation::HeapObject(), any_type});
    }

    TestReconfigureElementsKind_GeneralizeFieldTrivial(
        {kMutable, Representation::HeapObject(), any_type},
        {kConst, Representation::HeapObject(), new_type},
        {kMutable, Representation::HeapObject(), any_type});
  }
  TestReconfigureElementsKind_GeneralizeFieldTrivial(
      {kMutable, Representation::HeapObject(), any_type},
      {kMutable, Representation::HeapObject(), new_type},
      {kMutable, Representation::HeapObject(), any_type});
}

TEST(ReconfigureElementsKind_GeneralizeHeapObjectFieldToTagged) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  if (FLAG_track_constant_fields) {
    TestReconfigureElementsKind_GeneralizeField(
        {kConst, Representation::HeapObject(), value_type},
        {kConst, Representation::Smi(), any_type},
        {kConst, Representation::Tagged(), any_type});

    TestReconfigureElementsKind_GeneralizeField(
        {kConst, Representation::HeapObject(), value_type},
        {kMutable, Representation::Smi(), any_type},
        {kMutable, Representation::Tagged(), any_type});

    TestReconfigureElementsKind_GeneralizeField(
        {kMutable, Representation::HeapObject(), value_type},
        {kConst, Representation::Smi(), any_type},
        {kMutable, Representation::Tagged(), any_type});
  }
  TestReconfigureElementsKind_GeneralizeField(
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::Tagged(), any_type});
}

////////////////////////////////////////////////////////////////////////////////
// A set of tests checking split map deprecation.
//

TEST(ReconfigurePropertySplitMapTransitionsOverflow) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, kMutable, Representation::Smi(),
                                    any_type);
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
                                    Representation::Double(), any_type);
    expectations.SetDataField(kSplitProp, kMutable, Representation::Double(),
                              any_type);

    CHECK(expectations.Check(*split_map, kSplitProp));
    CHECK(expectations.Check(*map2, kSplitProp + 1));
  }

  // At this point |map| should be deprecated and disconnected from the
  // transition tree.
  CHECK(map->is_deprecated());
  CHECK(!split_map->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(!map2->is_deprecated());

  // Fill in transition tree of |map2| so that it can't have more transitions.
  for (int i = 0; i < TransitionArray::kMaxNumberOfTransitions; i++) {
    CHECK(TransitionArray::CanHaveMoreTransitions(map2));
    Handle<String> name = MakeName("foo", i);
    Map::CopyWithField(map2, name, any_type, NONE, kMutable,
                       Representation::Smi(), INSERT_TRANSITION)
        .ToHandleChecked();
  }
  CHECK(!TransitionArray::CanHaveMoreTransitions(map2));

  // Try to update |map|, since there is no place for propX transition at |map2|
  // |map| should become "copy-generalized".
  Handle<Map> updated_map = Map::Update(map);
  CHECK(updated_map->GetBackPointer()->IsUndefined(isolate));

  for (int i = 0; i < kPropCount; i++) {
    expectations.SetDataField(i, kMutable, Representation::Tagged(), any_type);
  }
  CHECK(expectations.Check(*updated_map));
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests involving special transitions (such as elements kind
// transition, observed transition or prototype transition).
//

// This test ensures that field generalization is correctly propagated from one
// branch of transition tree (|map2|) to another (|map|).
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
// TestReconfigureDataFieldAttribute_GeneralizeField once
// IS_PROTO_TRANS_ISSUE_FIXED and IS_NON_EQUIVALENT_TRANSITION_SUPPORTED are
// fixed.
template <typename TestConfig>
static void TestGeneralizeFieldWithSpecialTransition(TestConfig& config,
                                                     const CRFTData& from,
                                                     const CRFTData& to,
                                                     const CRFTData& expected) {
  Isolate* isolate = CcTest::i_isolate();

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount; i++) {
    map = expectations.AddDataField(map, NONE, from.constness,
                                    from.representation, from.type);
  }
  CHECK(!map->is_deprecated());
  CHECK(map->is_stable());
  CHECK(expectations.Check(*map));

  Expectations expectations2 = expectations;

  // Apply some special transition to |map|.
  CHECK(map->owns_descriptors());
  Handle<Map> map2 = config.Transition(map, expectations2);

  // |map| should still match expectations.
  CHECK(!map->is_deprecated());
  CHECK(expectations.Check(*map));

  if (config.generalizes_representations()) {
    for (int i = 0; i < kPropCount; i++) {
      expectations2.GeneralizeField(i);
    }
  }

  CHECK(!map2->is_deprecated());
  CHECK(map2->is_stable());
  CHECK(expectations2.Check(*map2));

  // Create new maps by generalizing representation of propX field.
  Handle<Map> maps[kPropCount];
  for (int i = 0; i < kPropCount; i++) {
    Handle<Map> new_map = Map::ReconfigureProperty(map, i, kData, NONE,
                                                   to.representation, to.type);
    maps[i] = new_map;

    expectations.SetDataField(i, expected.constness, expected.representation,
                              expected.type);

    CHECK(map->is_deprecated());
    CHECK_NE(*map, *new_map);
    CHECK(i == 0 || maps[i - 1]->is_deprecated());
    CHECK(expectations.Check(*new_map));

    Handle<Map> new_map2 = Map::Update(map2);
    CHECK(!new_map2->is_deprecated());
    CHECK(!new_map2->is_dictionary_map());

    Handle<Map> tmp_map;
    if (Map::TryUpdate(map2).ToHandle(&tmp_map)) {
      // If Map::TryUpdate() manages to succeed the result must match the result
      // of Map::Update().
      CHECK_EQ(*new_map2, *tmp_map);
    }

    if (config.is_non_equevalent_transition()) {
      // In case of non-equivalent transition currently we generalize all
      // representations.
      for (int i = 0; i < kPropCount; i++) {
        expectations2.GeneralizeField(i);
      }
      CHECK(new_map2->GetBackPointer()->IsUndefined(isolate));
      CHECK(expectations2.Check(*new_map2));
    } else {
      CHECK(!new_map2->GetBackPointer()->IsUndefined(isolate));
      CHECK(expectations2.Check(*new_map2));
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
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<Map> Transition(Handle<Map> map, Expectations& expectations) {
      Handle<Symbol> frozen_symbol(map->GetHeap()->frozen_symbol());
      expectations.SetElementsKind(DICTIONARY_ELEMENTS);
      return Map::CopyForPreventExtensions(map, NONE, frozen_symbol,
                                           "CopyForPreventExtensions");
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const { return false; }
    bool is_non_equevalent_transition() const { return true; }
  };
  TestConfig config;
  TestGeneralizeFieldWithSpecialTransition(
      config, {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Tagged(), any_type});
}


TEST(ElementsKindTransitionFromMapNotOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<Map> Transition(Handle<Map> map, Expectations& expectations) {
      Isolate* isolate = CcTest::i_isolate();
      Handle<FieldType> any_type = FieldType::Any(isolate);

      // Add one more transition to |map| in order to prevent descriptors
      // ownership.
      CHECK(map->owns_descriptors());
      Map::CopyWithField(map, MakeString("foo"), any_type, NONE, kMutable,
                         Representation::Smi(), INSERT_TRANSITION)
          .ToHandleChecked();
      CHECK(!map->owns_descriptors());

      Handle<Symbol> frozen_symbol(map->GetHeap()->frozen_symbol());
      expectations.SetElementsKind(DICTIONARY_ELEMENTS);
      return Map::CopyForPreventExtensions(map, NONE, frozen_symbol,
                                           "CopyForPreventExtensions");
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const { return false; }
    bool is_non_equevalent_transition() const { return true; }
  };
  TestConfig config;
  TestGeneralizeFieldWithSpecialTransition(
      config, {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Tagged(), any_type});
}


TEST(PrototypeTransitionFromMapOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<JSObject> prototype_;

    TestConfig() {
      Isolate* isolate = CcTest::i_isolate();
      Factory* factory = isolate->factory();
      prototype_ = factory->NewJSObjectFromMap(Map::Create(isolate, 0));
    }

    Handle<Map> Transition(Handle<Map> map, Expectations& expectations) {
      return Map::TransitionToPrototype(map, prototype_, REGULAR_PROTOTYPE);
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const {
      return !IS_PROTO_TRANS_ISSUE_FIXED;
    }
    bool is_non_equevalent_transition() const { return true; }
  };
  TestConfig config;
  TestGeneralizeFieldWithSpecialTransition(
      config, {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Tagged(), any_type});
}


TEST(PrototypeTransitionFromMapNotOwningDescriptor) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<FieldType> any_type = FieldType::Any(isolate);
  Handle<FieldType> value_type =
      FieldType::Class(Map::Create(isolate, 0), isolate);

  struct TestConfig {
    Handle<JSObject> prototype_;

    TestConfig() {
      Isolate* isolate = CcTest::i_isolate();
      Factory* factory = isolate->factory();
      prototype_ = factory->NewJSObjectFromMap(Map::Create(isolate, 0));
    }

    Handle<Map> Transition(Handle<Map> map, Expectations& expectations) {
      Isolate* isolate = CcTest::i_isolate();
      Handle<FieldType> any_type = FieldType::Any(isolate);

      // Add one more transition to |map| in order to prevent descriptors
      // ownership.
      CHECK(map->owns_descriptors());
      Map::CopyWithField(map, MakeString("foo"), any_type, NONE, kMutable,
                         Representation::Smi(), INSERT_TRANSITION)
          .ToHandleChecked();
      CHECK(!map->owns_descriptors());

      return Map::TransitionToPrototype(map, prototype_, REGULAR_PROTOTYPE);
    }
    // TODO(ishell): remove once IS_PROTO_TRANS_ISSUE_FIXED is removed.
    bool generalizes_representations() const {
      return !IS_PROTO_TRANS_ISSUE_FIXED;
    }
    bool is_non_equevalent_transition() const { return true; }
  };
  TestConfig config;
  TestGeneralizeFieldWithSpecialTransition(
      config, {kMutable, Representation::Smi(), any_type},
      {kMutable, Representation::HeapObject(), value_type},
      {kMutable, Representation::Tagged(), any_type});
}


////////////////////////////////////////////////////////////////////////////////
// A set of tests for higher level transitioning mechanics.
//

struct TransitionToDataFieldOperator {
  PropertyConstness constness_;
  Representation representation_;
  PropertyAttributes attributes_;
  Handle<FieldType> heap_type_;
  Handle<Object> value_;

  TransitionToDataFieldOperator(PropertyConstness constness,
                                Representation representation,
                                Handle<FieldType> heap_type,
                                Handle<Object> value,
                                PropertyAttributes attributes = NONE)
      : constness_(constness),
        representation_(representation),
        attributes_(attributes),
        heap_type_(heap_type),
        value_(value) {}

  Handle<Map> DoTransition(Expectations& expectations, Handle<Map> map) {
    return expectations.TransitionToDataField(
        map, attributes_, constness_, representation_, heap_type_, value_);
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
  Handle<FieldType> heap_type_;

  ReconfigureAsDataPropertyOperator(int descriptor,
                                    Representation representation,
                                    Handle<FieldType> heap_type,
                                    PropertyAttributes attributes = NONE)
      : descriptor_(descriptor),
        representation_(representation),
        attributes_(attributes),
        heap_type_(heap_type) {}

  Handle<Map> DoTransition(Expectations& expectations, Handle<Map> map) {
    expectations.SetDataField(descriptor_, kMutable, representation_,
                              heap_type_);
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

// Checks that field generalization happened.
struct FieldGeneralizationChecker {
  int descriptor_;
  PropertyConstness constness_;
  Representation representation_;
  PropertyAttributes attributes_;
  Handle<FieldType> heap_type_;

  FieldGeneralizationChecker(int descriptor, PropertyConstness constness,
                             Representation representation,
                             Handle<FieldType> heap_type,
                             PropertyAttributes attributes = NONE)
      : descriptor_(descriptor),
        constness_(constness),
        representation_(representation),
        attributes_(attributes),
        heap_type_(heap_type) {}

  void Check(Expectations& expectations2, Handle<Map> map1, Handle<Map> map2) {
    CHECK(!map2->is_deprecated());

    CHECK(map1->is_deprecated());
    CHECK_NE(*map1, *map2);
    Handle<Map> updated_map = Map::Update(map1);
    CHECK_EQ(*map2, *updated_map);

    expectations2.SetDataField(descriptor_, attributes_, constness_,
                               representation_, heap_type_);
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
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Expectations expectations(isolate);

  // Create a map, add required properties to it and initialize expectations.
  Handle<Map> initial_map = Map::Create(isolate, 0);
  Handle<Map> map = initial_map;
  for (int i = 0; i < kPropCount - 1; i++) {
    map = expectations.AddDataField(map, NONE, kMutable, Representation::Smi(),
                                    any_type);
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
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Handle<Object> value1 = handle(Smi::kZero, isolate);
  TransitionToDataFieldOperator transition_op1(kMutable, Representation::Smi(),
                                               any_type, value1);

  Handle<Object> value2 = isolate->factory()->NewHeapNumber(0);
  TransitionToDataFieldOperator transition_op2(
      kMutable, Representation::Double(), any_type, value2);

  FieldGeneralizationChecker checker(kPropCount - 1, kMutable,
                                     Representation::Double(), any_type);
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
  Handle<String> name = factory->empty_string();
  Handle<Map> sloppy_map =
      factory->CreateSloppyFunctionMap(FUNCTION_WITH_WRITEABLE_PROTOTYPE);
  Handle<SharedFunctionInfo> info = factory->NewSharedFunctionInfo(
      name, MaybeHandle<Code>(), sloppy_map->is_constructor());
  Handle<FieldType> function_type = FieldType::Class(sloppy_map, isolate);
  CHECK(sloppy_map->is_stable());

  Handle<JSFunction> js_func1 =
      factory->NewFunction(sloppy_map, info, isolate->native_context());
  TransitionToDataConstantOperator transition_op1(js_func1);

  Handle<JSFunction> js_func2 =
      factory->NewFunction(sloppy_map, info, isolate->native_context());
  TransitionToDataConstantOperator transition_op2(js_func2);

  if (FLAG_track_constant_fields) {
    SameMapChecker checker;
    TestTransitionTo(transition_op1, transition_op2, checker);

  } else {
    FieldGeneralizationChecker checker(
        kPropCount - 1, kMutable, Representation::HeapObject(), function_type);
    TestTransitionTo(transition_op1, transition_op2, checker);
  }
}


TEST(TransitionDataConstantToDataField) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Handle<JSFunction> js_func1 = factory->NewFunction(factory->empty_string());
  TransitionToDataConstantOperator transition_op1(js_func1);

  Handle<Object> value2 = isolate->factory()->NewHeapNumber(0);
  TransitionToDataFieldOperator transition_op2(
      kMutable, Representation::Double(), any_type, value2);

  FieldGeneralizationChecker checker(kPropCount - 1, kMutable,
                                     Representation::Tagged(), any_type);
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

TEST(FieldTypeConvertSimple) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Zone zone(isolate->allocator(), ZONE_NAME);

  CHECK_EQ(FieldType::Any()->Convert(&zone), AstType::NonInternal());
  CHECK_EQ(FieldType::None()->Convert(&zone), AstType::None());
}

// TODO(ishell): add this test once IS_ACCESSOR_FIELD_SUPPORTED is supported.
// TEST(TransitionAccessorConstantToAnotherAccessorConstant)

TEST(HoleyMutableHeapNumber) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();

  Handle<HeapNumber> mhn = isolate->factory()->NewMutableHeapNumber();
  CHECK_EQ(kHoleNanInt64, mhn->value_as_bits());

  mhn = isolate->factory()->NewHeapNumber(0.0, MUTABLE);
  CHECK_EQ(V8_UINT64_C(0), mhn->value_as_bits());

  mhn->set_value_as_bits(kHoleNanInt64);
  CHECK_EQ(kHoleNanInt64, mhn->value_as_bits());

  // Ensure that new storage for uninitialized value or mutable heap number
  // with uninitialized sentinel (kHoleNanInt64) is a mutable heap number
  // with uninitialized sentinel.
  Handle<Object> obj =
      Object::NewStorageFor(isolate, isolate->factory()->uninitialized_value(),
                            Representation::Double());
  CHECK(obj->IsMutableHeapNumber());
  CHECK_EQ(kHoleNanInt64, HeapNumber::cast(*obj)->value_as_bits());

  obj = Object::NewStorageFor(isolate, mhn, Representation::Double());
  CHECK(obj->IsMutableHeapNumber());
  CHECK_EQ(kHoleNanInt64, HeapNumber::cast(*obj)->value_as_bits());
}
