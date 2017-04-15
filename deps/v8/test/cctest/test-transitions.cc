// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <utility>

#include "src/v8.h"

#include "src/compilation-cache.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/field-type.h"
#include "src/global-handles.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/field-type.h -> src/objects-inl.h
#include "src/objects-inl.h"
#include "src/transitions.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;


//
// Helper functions.
//

TEST(TransitionArray_SimpleFieldTransitions) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<String> name1 = factory->InternalizeUtf8String("foo");
  Handle<String> name2 = factory->InternalizeUtf8String("bar");
  PropertyAttributes attributes = NONE;

  Handle<Map> map0 = Map::Create(isolate, 0);
  Handle<Map> map1 =
      Map::CopyWithField(map0, name1, handle(FieldType::Any(), isolate),
                         attributes, Representation::Tagged(), OMIT_TRANSITION)
          .ToHandleChecked();
  Handle<Map> map2 =
      Map::CopyWithField(map0, name2, handle(FieldType::Any(), isolate),
                         attributes, Representation::Tagged(), OMIT_TRANSITION)
          .ToHandleChecked();

  CHECK(map0->raw_transitions()->IsSmi());

  TransitionArray::Insert(map0, name1, map1, SIMPLE_PROPERTY_TRANSITION);
  CHECK(TransitionArray::IsSimpleTransition(map0->raw_transitions()));
  CHECK_EQ(*map1,
           TransitionArray::SearchTransition(*map0, kData, *name1, attributes));
  CHECK_EQ(1, TransitionArray::NumberOfTransitions(map0->raw_transitions()));
  CHECK_EQ(*name1, TransitionArray::GetKey(map0->raw_transitions(), 0));
  CHECK_EQ(*map1, TransitionArray::GetTarget(map0->raw_transitions(), 0));

  TransitionArray::Insert(map0, name2, map2, SIMPLE_PROPERTY_TRANSITION);
  CHECK(TransitionArray::IsFullTransitionArray(map0->raw_transitions()));

  CHECK_EQ(*map1,
           TransitionArray::SearchTransition(*map0, kData, *name1, attributes));
  CHECK_EQ(*map2,
           TransitionArray::SearchTransition(*map0, kData, *name2, attributes));
  CHECK_EQ(2, TransitionArray::NumberOfTransitions(map0->raw_transitions()));
  for (int i = 0; i < 2; i++) {
    Name* key = TransitionArray::GetKey(map0->raw_transitions(), i);
    Map* target = TransitionArray::GetTarget(map0->raw_transitions(), i);
    CHECK((key == *name1 && target == *map1) ||
          (key == *name2 && target == *map2));
  }

#ifdef DEBUG
  CHECK(TransitionArray::IsSortedNoDuplicates(*map0));
#endif
}


TEST(TransitionArray_FullFieldTransitions) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<String> name1 = factory->InternalizeUtf8String("foo");
  Handle<String> name2 = factory->InternalizeUtf8String("bar");
  PropertyAttributes attributes = NONE;

  Handle<Map> map0 = Map::Create(isolate, 0);
  Handle<Map> map1 =
      Map::CopyWithField(map0, name1, handle(FieldType::Any(), isolate),
                         attributes, Representation::Tagged(), OMIT_TRANSITION)
          .ToHandleChecked();
  Handle<Map> map2 =
      Map::CopyWithField(map0, name2, handle(FieldType::Any(), isolate),
                         attributes, Representation::Tagged(), OMIT_TRANSITION)
          .ToHandleChecked();

  CHECK(map0->raw_transitions()->IsSmi());

  TransitionArray::Insert(map0, name1, map1, PROPERTY_TRANSITION);
  CHECK(TransitionArray::IsFullTransitionArray(map0->raw_transitions()));
  CHECK_EQ(*map1,
           TransitionArray::SearchTransition(*map0, kData, *name1, attributes));
  CHECK_EQ(1, TransitionArray::NumberOfTransitions(map0->raw_transitions()));
  CHECK_EQ(*name1, TransitionArray::GetKey(map0->raw_transitions(), 0));
  CHECK_EQ(*map1, TransitionArray::GetTarget(map0->raw_transitions(), 0));

  TransitionArray::Insert(map0, name2, map2, PROPERTY_TRANSITION);
  CHECK(TransitionArray::IsFullTransitionArray(map0->raw_transitions()));

  CHECK_EQ(*map1,
           TransitionArray::SearchTransition(*map0, kData, *name1, attributes));
  CHECK_EQ(*map2,
           TransitionArray::SearchTransition(*map0, kData, *name2, attributes));
  CHECK_EQ(2, TransitionArray::NumberOfTransitions(map0->raw_transitions()));
  for (int i = 0; i < 2; i++) {
    Name* key = TransitionArray::GetKey(map0->raw_transitions(), i);
    Map* target = TransitionArray::GetTarget(map0->raw_transitions(), i);
    CHECK((key == *name1 && target == *map1) ||
          (key == *name2 && target == *map2));
  }

#ifdef DEBUG
  CHECK(TransitionArray::IsSortedNoDuplicates(*map0));
#endif
}


TEST(TransitionArray_DifferentFieldNames) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  const int PROPS_COUNT = 10;
  Handle<String> names[PROPS_COUNT];
  Handle<Map> maps[PROPS_COUNT];
  PropertyAttributes attributes = NONE;

  Handle<Map> map0 = Map::Create(isolate, 0);
  CHECK(map0->raw_transitions()->IsSmi());

  for (int i = 0; i < PROPS_COUNT; i++) {
    EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "prop%d", i);
    Handle<String> name = factory->InternalizeUtf8String(buffer.start());
    Handle<Map> map = Map::CopyWithField(
                          map0, name, handle(FieldType::Any(), isolate),
                          attributes, Representation::Tagged(), OMIT_TRANSITION)
                          .ToHandleChecked();
    names[i] = name;
    maps[i] = map;

    TransitionArray::Insert(map0, name, map, PROPERTY_TRANSITION);
  }

  for (int i = 0; i < PROPS_COUNT; i++) {
    CHECK_EQ(*maps[i], TransitionArray::SearchTransition(
                           *map0, kData, *names[i], attributes));
  }
  for (int i = 0; i < PROPS_COUNT; i++) {
    Name* key = TransitionArray::GetKey(map0->raw_transitions(), i);
    Map* target = TransitionArray::GetTarget(map0->raw_transitions(), i);
    for (int j = 0; j < PROPS_COUNT; j++) {
      if (*names[i] == key) {
        CHECK_EQ(*maps[i], target);
        break;
      }
    }
  }

#ifdef DEBUG
  CHECK(TransitionArray::IsSortedNoDuplicates(*map0));
#endif
}


TEST(TransitionArray_SameFieldNamesDifferentAttributesSimple) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<Map> map0 = Map::Create(isolate, 0);
  CHECK(map0->raw_transitions()->IsSmi());

  const int ATTRS_COUNT = (READ_ONLY | DONT_ENUM | DONT_DELETE) + 1;
  STATIC_ASSERT(ATTRS_COUNT == 8);
  Handle<Map> attr_maps[ATTRS_COUNT];
  Handle<String> name = factory->InternalizeUtf8String("foo");

  // Add transitions for same field name but different attributes.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    PropertyAttributes attributes = static_cast<PropertyAttributes>(i);

    Handle<Map> map = Map::CopyWithField(
                          map0, name, handle(FieldType::Any(), isolate),
                          attributes, Representation::Tagged(), OMIT_TRANSITION)
                          .ToHandleChecked();
    attr_maps[i] = map;

    TransitionArray::Insert(map0, name, map, PROPERTY_TRANSITION);
  }

  // Ensure that transitions for |name| field are valid.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    PropertyAttributes attributes = static_cast<PropertyAttributes>(i);
    CHECK_EQ(*attr_maps[i], TransitionArray::SearchTransition(
                                *map0, kData, *name, attributes));
    // All transitions use the same key, so this check doesn't need to
    // care about ordering.
    CHECK_EQ(*name, TransitionArray::GetKey(map0->raw_transitions(), i));
  }

#ifdef DEBUG
  CHECK(TransitionArray::IsSortedNoDuplicates(*map0));
#endif
}


TEST(TransitionArray_SameFieldNamesDifferentAttributes) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  const int PROPS_COUNT = 10;
  Handle<String> names[PROPS_COUNT];
  Handle<Map> maps[PROPS_COUNT];

  Handle<Map> map0 = Map::Create(isolate, 0);
  CHECK(map0->raw_transitions()->IsSmi());

  // Some number of fields.
  for (int i = 0; i < PROPS_COUNT; i++) {
    EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "prop%d", i);
    Handle<String> name = factory->InternalizeUtf8String(buffer.start());
    Handle<Map> map =
        Map::CopyWithField(map0, name, handle(FieldType::Any(), isolate), NONE,
                           Representation::Tagged(), OMIT_TRANSITION)
            .ToHandleChecked();
    names[i] = name;
    maps[i] = map;

    TransitionArray::Insert(map0, name, map, PROPERTY_TRANSITION);
  }

  const int ATTRS_COUNT = (READ_ONLY | DONT_ENUM | DONT_DELETE) + 1;
  STATIC_ASSERT(ATTRS_COUNT == 8);
  Handle<Map> attr_maps[ATTRS_COUNT];
  Handle<String> name = factory->InternalizeUtf8String("foo");

  // Add transitions for same field name but different attributes.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    PropertyAttributes attributes = static_cast<PropertyAttributes>(i);

    Handle<Map> map = Map::CopyWithField(
                          map0, name, handle(FieldType::Any(), isolate),
                          attributes, Representation::Tagged(), OMIT_TRANSITION)
                          .ToHandleChecked();
    attr_maps[i] = map;

    TransitionArray::Insert(map0, name, map, PROPERTY_TRANSITION);
  }

  // Ensure that transitions for |name| field are valid.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    PropertyAttributes attr = static_cast<PropertyAttributes>(i);
    CHECK_EQ(*attr_maps[i],
             TransitionArray::SearchTransition(*map0, kData, *name, attr));
  }

  // Ensure that info about the other fields still valid.
  CHECK_EQ(PROPS_COUNT + ATTRS_COUNT,
           TransitionArray::NumberOfTransitions(map0->raw_transitions()));
  for (int i = 0; i < PROPS_COUNT + ATTRS_COUNT; i++) {
    Name* key = TransitionArray::GetKey(map0->raw_transitions(), i);
    Map* target = TransitionArray::GetTarget(map0->raw_transitions(), i);
    if (key == *name) {
      // Attributes transition.
      PropertyAttributes attributes =
          target->GetLastDescriptorDetails().attributes();
      CHECK_EQ(*attr_maps[static_cast<int>(attributes)], target);
    } else {
      for (int j = 0; j < PROPS_COUNT; j++) {
        if (*names[j] == key) {
          CHECK_EQ(*maps[j], target);
          break;
        }
      }
    }
  }

#ifdef DEBUG
  CHECK(TransitionArray::IsSortedNoDuplicates(*map0));
#endif
}
