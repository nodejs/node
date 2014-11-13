// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <utility>

#include "src/v8.h"

#include "src/compilation-cache.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/global-handles.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;


//
// Helper functions.
//

static void ConnectTransition(Handle<Map> parent,
                              Handle<TransitionArray> transitions,
                              Handle<Map> child) {
  if (!parent->HasTransitionArray() || *transitions != parent->transitions()) {
    parent->set_transitions(*transitions);
  }
  child->SetBackPointer(*parent);
}


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
      Map::CopyWithField(map0, name1, handle(HeapType::Any(), isolate),
                         attributes, Representation::Tagged(),
                         OMIT_TRANSITION).ToHandleChecked();
  Handle<Map> map2 =
      Map::CopyWithField(map0, name2, handle(HeapType::Any(), isolate),
                         attributes, Representation::Tagged(),
                         OMIT_TRANSITION).ToHandleChecked();

  CHECK(!map0->HasTransitionArray());
  Handle<TransitionArray> transitions = TransitionArray::Allocate(isolate, 0);
  CHECK(transitions->IsFullTransitionArray());

  int transition;
  transitions =
      transitions->Insert(map0, name1, map1, SIMPLE_PROPERTY_TRANSITION);
  ConnectTransition(map0, transitions, map1);
  CHECK(transitions->IsSimpleTransition());
  transition = transitions->Search(FIELD, *name1, attributes);
  CHECK_EQ(TransitionArray::kSimpleTransitionIndex, transition);
  CHECK_EQ(*name1, transitions->GetKey(transition));
  CHECK_EQ(*map1, transitions->GetTarget(transition));

  transitions =
      transitions->Insert(map0, name2, map2, SIMPLE_PROPERTY_TRANSITION);
  ConnectTransition(map0, transitions, map2);
  CHECK(transitions->IsFullTransitionArray());

  transition = transitions->Search(FIELD, *name1, attributes);
  CHECK_EQ(*name1, transitions->GetKey(transition));
  CHECK_EQ(*map1, transitions->GetTarget(transition));

  transition = transitions->Search(FIELD, *name2, attributes);
  CHECK_EQ(*name2, transitions->GetKey(transition));
  CHECK_EQ(*map2, transitions->GetTarget(transition));

  DCHECK(transitions->IsSortedNoDuplicates());
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
      Map::CopyWithField(map0, name1, handle(HeapType::Any(), isolate),
                         attributes, Representation::Tagged(),
                         OMIT_TRANSITION).ToHandleChecked();
  Handle<Map> map2 =
      Map::CopyWithField(map0, name2, handle(HeapType::Any(), isolate),
                         attributes, Representation::Tagged(),
                         OMIT_TRANSITION).ToHandleChecked();

  CHECK(!map0->HasTransitionArray());
  Handle<TransitionArray> transitions = TransitionArray::Allocate(isolate, 0);
  CHECK(transitions->IsFullTransitionArray());

  int transition;
  transitions = transitions->Insert(map0, name1, map1, PROPERTY_TRANSITION);
  ConnectTransition(map0, transitions, map1);
  CHECK(transitions->IsFullTransitionArray());
  transition = transitions->Search(FIELD, *name1, attributes);
  CHECK_EQ(*name1, transitions->GetKey(transition));
  CHECK_EQ(*map1, transitions->GetTarget(transition));

  transitions = transitions->Insert(map0, name2, map2, PROPERTY_TRANSITION);
  ConnectTransition(map0, transitions, map2);
  CHECK(transitions->IsFullTransitionArray());

  transition = transitions->Search(FIELD, *name1, attributes);
  CHECK_EQ(*name1, transitions->GetKey(transition));
  CHECK_EQ(*map1, transitions->GetTarget(transition));

  transition = transitions->Search(FIELD, *name2, attributes);
  CHECK_EQ(*name2, transitions->GetKey(transition));
  CHECK_EQ(*map2, transitions->GetTarget(transition));

  DCHECK(transitions->IsSortedNoDuplicates());
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
  CHECK(!map0->HasTransitionArray());
  Handle<TransitionArray> transitions = TransitionArray::Allocate(isolate, 0);
  CHECK(transitions->IsFullTransitionArray());

  for (int i = 0; i < PROPS_COUNT; i++) {
    EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "prop%d", i);
    Handle<String> name = factory->InternalizeUtf8String(buffer.start());
    Handle<Map> map =
        Map::CopyWithField(map0, name, handle(HeapType::Any(), isolate),
                           attributes, Representation::Tagged(),
                           OMIT_TRANSITION).ToHandleChecked();
    names[i] = name;
    maps[i] = map;

    transitions = transitions->Insert(map0, name, map, PROPERTY_TRANSITION);
    ConnectTransition(map0, transitions, map);
  }

  for (int i = 0; i < PROPS_COUNT; i++) {
    int transition = transitions->Search(FIELD, *names[i], attributes);
    CHECK_EQ(*names[i], transitions->GetKey(transition));
    CHECK_EQ(*maps[i], transitions->GetTarget(transition));
  }

  DCHECK(transitions->IsSortedNoDuplicates());
}


TEST(TransitionArray_SameFieldNamesDifferentAttributesSimple) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  Handle<Map> map0 = Map::Create(isolate, 0);
  CHECK(!map0->HasTransitionArray());
  Handle<TransitionArray> transitions = TransitionArray::Allocate(isolate, 0);
  CHECK(transitions->IsFullTransitionArray());

  const int ATTRS_COUNT = (READ_ONLY | DONT_ENUM | DONT_DELETE) + 1;
  STATIC_ASSERT(ATTRS_COUNT == 8);
  Handle<Map> attr_maps[ATTRS_COUNT];
  Handle<String> name = factory->InternalizeUtf8String("foo");

  // Add transitions for same field name but different attributes.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    PropertyAttributes attributes = static_cast<PropertyAttributes>(i);

    Handle<Map> map =
        Map::CopyWithField(map0, name, handle(HeapType::Any(), isolate),
                           attributes, Representation::Tagged(),
                           OMIT_TRANSITION).ToHandleChecked();
    attr_maps[i] = map;

    transitions = transitions->Insert(map0, name, map, PROPERTY_TRANSITION);
    ConnectTransition(map0, transitions, map);
  }

  // Ensure that transitions for |name| field are valid.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    PropertyAttributes attributes = static_cast<PropertyAttributes>(i);

    int transition = transitions->Search(FIELD, *name, attributes);
    CHECK_EQ(*name, transitions->GetKey(transition));
    CHECK_EQ(*attr_maps[i], transitions->GetTarget(transition));
  }

  DCHECK(transitions->IsSortedNoDuplicates());
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
  CHECK(!map0->HasTransitionArray());
  Handle<TransitionArray> transitions = TransitionArray::Allocate(isolate, 0);
  CHECK(transitions->IsFullTransitionArray());

  // Some number of fields.
  for (int i = 0; i < PROPS_COUNT; i++) {
    EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "prop%d", i);
    Handle<String> name = factory->InternalizeUtf8String(buffer.start());
    Handle<Map> map =
        Map::CopyWithField(map0, name, handle(HeapType::Any(), isolate), NONE,
                           Representation::Tagged(),
                           OMIT_TRANSITION).ToHandleChecked();
    names[i] = name;
    maps[i] = map;

    transitions = transitions->Insert(map0, name, map, PROPERTY_TRANSITION);
    ConnectTransition(map0, transitions, map);
  }

  const int ATTRS_COUNT = (READ_ONLY | DONT_ENUM | DONT_DELETE) + 1;
  STATIC_ASSERT(ATTRS_COUNT == 8);
  Handle<Map> attr_maps[ATTRS_COUNT];
  Handle<String> name = factory->InternalizeUtf8String("foo");

  // Add transitions for same field name but different attributes.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    PropertyAttributes attributes = static_cast<PropertyAttributes>(i);

    Handle<Map> map =
        Map::CopyWithField(map0, name, handle(HeapType::Any(), isolate),
                           attributes, Representation::Tagged(),
                           OMIT_TRANSITION).ToHandleChecked();
    attr_maps[i] = map;

    transitions = transitions->Insert(map0, name, map, PROPERTY_TRANSITION);
    ConnectTransition(map0, transitions, map);
  }

  // Ensure that transitions for |name| field are valid.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    PropertyAttributes attributes = static_cast<PropertyAttributes>(i);

    int transition = transitions->Search(FIELD, *name, attributes);
    CHECK_EQ(*name, transitions->GetKey(transition));
    CHECK_EQ(*attr_maps[i], transitions->GetTarget(transition));
  }

  // Ensure that info about the other fields still valid.
  for (int i = 0; i < PROPS_COUNT; i++) {
    int transition = transitions->Search(FIELD, *names[i], NONE);
    CHECK_EQ(*names[i], transitions->GetKey(transition));
    CHECK_EQ(*maps[i], transitions->GetTarget(transition));
  }

  DCHECK(transitions->IsSortedNoDuplicates());
}
