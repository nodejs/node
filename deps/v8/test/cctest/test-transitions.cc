// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/test-transitions.h"

#include <stdlib.h>

#include <utility>

#include "src/api/api-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/execution/execution.h"
#include "src/heap/factory.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/field-type.h"
#include "src/objects/objects-inl.h"
#include "src/objects/transitions-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

TEST(TransitionArray_SimpleFieldTransitions) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  DirectHandle<String> name1 = factory->InternalizeUtf8String("foo");
  DirectHandle<String> name2 = factory->InternalizeUtf8String("bar");
  PropertyAttributes attributes = NONE;

  DirectHandle<Map> map0 = Map::Create(isolate, 0);
  DirectHandle<Map> map1 =
      Map::CopyWithField(isolate, map0, name1, FieldType::Any(isolate),
                         attributes, PropertyConstness::kMutable,
                         Representation::Tagged(), OMIT_TRANSITION)
          .ToHandleChecked();
  DirectHandle<Map> map2 =
      Map::CopyWithField(isolate, map0, name2, FieldType::Any(isolate),
                         attributes, PropertyConstness::kMutable,
                         Representation::Tagged(), OMIT_TRANSITION)
          .ToHandleChecked();

  CHECK(IsSmi(map0->raw_transitions()));

  {
    TransitionsAccessor::Insert(isolate, map0, name1, map1,
                                SIMPLE_PROPERTY_TRANSITION);
  }
  {
    {
      TestTransitionsAccessor transitions(isolate, map0);
      CHECK(transitions.IsWeakRefEncoding());
      CHECK_EQ(*map1, transitions.SearchTransition(*name1, PropertyKind::kData,
                                                   attributes));
      CHECK_EQ(1, transitions.NumberOfTransitions());
      CHECK_EQ(*name1, transitions.GetKey(0));
      CHECK_EQ(*map1, transitions.GetTarget(0));
    }

    TransitionsAccessor::Insert(isolate, map0, name2, map2,
                                SIMPLE_PROPERTY_TRANSITION);
  }
  {
    TestTransitionsAccessor transitions(isolate, map0);
    CHECK(transitions.IsFullTransitionArrayEncoding());

    CHECK_EQ(*map1, transitions.SearchTransition(*name1, PropertyKind::kData,
                                                 attributes));
    CHECK_EQ(*map2, transitions.SearchTransition(*name2, PropertyKind::kData,
                                                 attributes));
    CHECK_EQ(2, transitions.NumberOfTransitions());
    for (int i = 0; i < 2; i++) {
      Tagged<Name> key = transitions.GetKey(i);
      Tagged<Map> target = transitions.GetTarget(i);
      CHECK((key == *name1 && target == *map1) ||
            (key == *name2 && target == *map2));
    }

    DCHECK(transitions.IsSortedNoDuplicates());
  }
}


TEST(TransitionArray_FullFieldTransitions) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  DirectHandle<String> name1 = factory->InternalizeUtf8String("foo");
  DirectHandle<String> name2 = factory->InternalizeUtf8String("bar");
  PropertyAttributes attributes = NONE;

  DirectHandle<Map> map0 = Map::Create(isolate, 0);
  DirectHandle<Map> map1 =
      Map::CopyWithField(isolate, map0, name1, FieldType::Any(isolate),
                         attributes, PropertyConstness::kMutable,
                         Representation::Tagged(), OMIT_TRANSITION)
          .ToHandleChecked();
  DirectHandle<Map> map2 =
      Map::CopyWithField(isolate, map0, name2, FieldType::Any(isolate),
                         attributes, PropertyConstness::kMutable,
                         Representation::Tagged(), OMIT_TRANSITION)
          .ToHandleChecked();

  CHECK(IsSmi(map0->raw_transitions()));

  {
    TransitionsAccessor::Insert(isolate, map0, name1, map1,
                                PROPERTY_TRANSITION);
  }
  {
    {
      TestTransitionsAccessor transitions(isolate, map0);
      CHECK(transitions.IsFullTransitionArrayEncoding());
      CHECK_EQ(*map1, transitions.SearchTransition(*name1, PropertyKind::kData,
                                                   attributes));
      CHECK_EQ(1, transitions.NumberOfTransitions());
      CHECK_EQ(*name1, transitions.GetKey(0));
      CHECK_EQ(*map1, transitions.GetTarget(0));
    }

    TransitionsAccessor::Insert(isolate, map0, name2, map2,
                                PROPERTY_TRANSITION);
  }
  {
    TestTransitionsAccessor transitions(isolate, map0);
    CHECK(transitions.IsFullTransitionArrayEncoding());

    CHECK_EQ(*map1, transitions.SearchTransition(*name1, PropertyKind::kData,
                                                 attributes));
    CHECK_EQ(*map2, transitions.SearchTransition(*name2, PropertyKind::kData,
                                                 attributes));
    CHECK_EQ(2, transitions.NumberOfTransitions());
    for (int i = 0; i < 2; i++) {
      Tagged<Name> key = transitions.GetKey(i);
      Tagged<Map> target = transitions.GetTarget(i);
      CHECK((key == *name1 && target == *map1) ||
            (key == *name2 && target == *map2));
    }

    DCHECK(transitions.IsSortedNoDuplicates());
  }
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

  DirectHandle<Map> map0 = Map::Create(isolate, 0);
  CHECK(IsSmi(map0->raw_transitions()));

  for (int i = 0; i < PROPS_COUNT; i++) {
    base::EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "prop%d", i);
    Handle<String> name = factory->InternalizeUtf8String(buffer.begin());
    Handle<Map> map =
        Map::CopyWithField(isolate, map0, name, FieldType::Any(isolate),
                           attributes, PropertyConstness::kMutable,
                           Representation::Tagged(), OMIT_TRANSITION)
            .ToHandleChecked();
    names[i] = name;
    maps[i] = map;

    TransitionsAccessor::Insert(isolate, map0, name, map, PROPERTY_TRANSITION);
  }

  TransitionsAccessor transitions(isolate, *map0);
  for (int i = 0; i < PROPS_COUNT; i++) {
    CHECK_EQ(*maps[i], transitions.SearchTransition(
                           *names[i], PropertyKind::kData, attributes));
  }
  for (int i = 0; i < PROPS_COUNT; i++) {
    Tagged<Name> key = transitions.GetKey(i);
    Tagged<Map> target = transitions.GetTarget(i);
    for (int j = 0; j < PROPS_COUNT; j++) {
      if (*names[i] == key) {
        CHECK_EQ(*maps[i], target);
        break;
      }
    }
  }

  DCHECK(transitions.IsSortedNoDuplicates());
}


TEST(TransitionArray_SameFieldNamesDifferentAttributesSimple) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  DirectHandle<Map> map0 = Map::Create(isolate, 0);
  CHECK(IsSmi(map0->raw_transitions()));

  const int ATTRS_COUNT = (READ_ONLY | DONT_ENUM | DONT_DELETE) + 1;
  static_assert(ATTRS_COUNT == 8);
  Handle<Map> attr_maps[ATTRS_COUNT];
  DirectHandle<String> name = factory->InternalizeUtf8String("foo");

  // Add transitions for same field name but different attributes.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    auto attributes = PropertyAttributesFromInt(i);

    Handle<Map> map =
        Map::CopyWithField(isolate, map0, name, FieldType::Any(isolate),
                           attributes, PropertyConstness::kMutable,
                           Representation::Tagged(), OMIT_TRANSITION)
            .ToHandleChecked();
    attr_maps[i] = map;

    TransitionsAccessor::Insert(isolate, map0, name, map, PROPERTY_TRANSITION);
  }

  // Ensure that transitions for |name| field are valid.
  TransitionsAccessor transitions(isolate, *map0);
  for (int i = 0; i < ATTRS_COUNT; i++) {
    auto attributes = PropertyAttributesFromInt(i);
    CHECK_EQ(*attr_maps[i], transitions.SearchTransition(
                                *name, PropertyKind::kData, attributes));
    // All transitions use the same key, so this check doesn't need to
    // care about ordering.
    CHECK_EQ(*name, transitions.GetKey(i));
  }

  DCHECK(transitions.IsSortedNoDuplicates());
}


TEST(TransitionArray_SameFieldNamesDifferentAttributes) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  const int PROPS_COUNT = 10;
  Handle<String> names[PROPS_COUNT];
  Handle<Map> maps[PROPS_COUNT];

  DirectHandle<Map> map0 = Map::Create(isolate, 0);
  CHECK(IsSmi(map0->raw_transitions()));

  // Some number of fields.
  for (int i = 0; i < PROPS_COUNT; i++) {
    base::EmbeddedVector<char, 64> buffer;
    SNPrintF(buffer, "prop%d", i);
    Handle<String> name = factory->InternalizeUtf8String(buffer.begin());
    Handle<Map> map =
        Map::CopyWithField(isolate, map0, name, FieldType::Any(isolate), NONE,
                           PropertyConstness::kMutable,
                           Representation::Tagged(), OMIT_TRANSITION)
            .ToHandleChecked();
    names[i] = name;
    maps[i] = map;

    TransitionsAccessor::Insert(isolate, map0, name, map, PROPERTY_TRANSITION);
  }

  const int ATTRS_COUNT = (READ_ONLY | DONT_ENUM | DONT_DELETE) + 1;
  static_assert(ATTRS_COUNT == 8);
  Handle<Map> attr_maps[ATTRS_COUNT];
  DirectHandle<String> name = factory->InternalizeUtf8String("foo");

  // Add transitions for same field name but different attributes.
  for (int i = 0; i < ATTRS_COUNT; i++) {
    auto attributes = PropertyAttributesFromInt(i);

    Handle<Map> map =
        Map::CopyWithField(isolate, map0, name, FieldType::Any(isolate),
                           attributes, PropertyConstness::kMutable,
                           Representation::Tagged(), OMIT_TRANSITION)
            .ToHandleChecked();
    attr_maps[i] = map;

    TransitionsAccessor::Insert(isolate, map0, name, map, PROPERTY_TRANSITION);
  }

  // Ensure that transitions for |name| field are valid.
  TransitionsAccessor transitions(isolate, *map0);
  for (int i = 0; i < ATTRS_COUNT; i++) {
    auto attr = PropertyAttributesFromInt(i);
    CHECK_EQ(*attr_maps[i],
             transitions.SearchTransition(*name, PropertyKind::kData, attr));
  }

  // Ensure that info about the other fields still valid.
  CHECK_EQ(PROPS_COUNT + ATTRS_COUNT, transitions.NumberOfTransitions());
  for (int i = 0; i < PROPS_COUNT + ATTRS_COUNT; i++) {
    Tagged<Name> key = transitions.GetKey(i);
    Tagged<Map> target = transitions.GetTarget(i);
    if (key == *name) {
      // Attributes transition.
      PropertyAttributes attributes =
          target->GetLastDescriptorDetails(isolate).attributes();
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

  DCHECK(transitions.IsSortedNoDuplicates());
}

UNINITIALIZED_TEST(TransitionArray_InsertToBinarySearchSizeAfterRehashing) {
  v8_flags.rehash_snapshot = true;
  v8_flags.hash_seed = 42;
  v8_flags.allow_natives_syntax = true;
  DisableEmbeddedBlobRefcounting();
  v8::StartupData blob;
  int initial_size = TransitionArray::kMaxElementsForLinearSearch - 3;
  int final_size = TransitionArray::kMaxElementsForLinearSearch + 3;

  {
    v8::Isolate::CreateParams testing_params;
    testing_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    v8::SnapshotCreator creator(testing_params);
    v8::Isolate* isolate = creator.GetIsolate();
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      // Get the cached map for empty objects
      Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
      v8::Local<v8::Object> obj = v8::Object::New(isolate);
      DirectHandle<Map> first_map =
          direct_handle(v8::Utils::OpenDirectHandle(*obj)->map(), i_isolate);

      // Ensure that the transition array is empty initially.
      {
        TestTransitionsAccessor transitions(i_isolate, first_map);
        CHECK_EQ(0, transitions.NumberOfTransitions());
        DCHECK(transitions.transitions()->IsSortedNoDuplicates());
      }

      // Insert some transitions to grow the transition array, we'll rehash
      // later using the snapshot which "shuffles" the entries in hash order.
      v8::Local<v8::Value> null_value = v8::Null(isolate);
      v8::LocalVector<v8::Value> objects(isolate);
      for (int i = 0; i < initial_size; i++) {
        std::string prop_name = "prop_" + std::to_string(i);
        v8::Local<v8::String> name =
            v8::String::NewFromUtf8(isolate, prop_name.c_str(),
                                    v8::NewStringType::kNormal)
                .ToLocalChecked();
        v8::Local<v8::Object> new_obj = v8::Object::New(isolate);
        new_obj->Set(context, name, null_value).Check();
        objects.push_back(new_obj);
      }
      context->Global()
          ->Set(context, v8_str("objects_for_transitions"),
                v8::Array::New(isolate, objects.data(), objects.size()))
          .Check();

      creator.SetDefaultContext(context);

      // Verify that the cached map has initial_size transitions.
      TestTransitionsAccessor transitions(i_isolate, first_map);
      CHECK_EQ(initial_size, transitions.NumberOfTransitions());
      DCHECK(transitions.transitions()->IsSortedNoDuplicates());
    }
    blob =
        creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
    CHECK(blob.CanBeRehashed());
  }

  // Create a new isolate from the snapshot with rehashing enabled.
  v8_flags.hash_seed = 1337;
  v8::Isolate::CreateParams testing_params;
  testing_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  testing_params.snapshot_blob = &blob;
  v8::Isolate* isolate = v8::Isolate::New(testing_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    // Check that rehashing has been performed.
    CHECK_EQ(static_cast<uint64_t>(1337),
             HashSeed(reinterpret_cast<Isolate*>(isolate)).seed());
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    CHECK(!context.IsEmpty());
    v8::Context::Scope context_scope(context);

    // Get the cached map for empty objects.
    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
    v8::Local<v8::Object> obj = v8::Object::New(isolate);
    DirectHandle<Map> first_map =
        direct_handle(v8::Utils::OpenDirectHandle(*obj)->map(), i_isolate);

    // Verify that the cached map still has initial_size transitions.
    {
      TestTransitionsAccessor transitions(i_isolate, first_map);
      CHECK_EQ(initial_size, transitions.NumberOfTransitions());
      DCHECK(transitions.transitions()->IsSortedNoDuplicates());
    }

    v8::Local<v8::Value> null_value = v8::Null(isolate);
    {
      v8::LocalVector<v8::Value> objects(isolate);

      // Insert some transitions to grow the rehashed transition array beyond
      // linear search size
      for (int i = initial_size; i < final_size; i++) {
        std::string prop_name = "prop_" + std::to_string(i);
        v8::Local<v8::String> name =
            v8::String::NewFromUtf8(isolate, prop_name.c_str(),
                                    v8::NewStringType::kNormal)
                .ToLocalChecked();
        v8::Local<v8::Object> new_obj = v8::Object::New(isolate);
        new_obj->Set(context, name, null_value).Check();
        objects.push_back(new_obj);
      }
      context->Global()
          ->Set(context, v8_str("objects_for_transitions_2"),
                v8::Array::New(isolate, objects.data(), objects.size()))
          .Check();
      TestTransitionsAccessor transitions(i_isolate, first_map);
      CHECK_EQ(final_size, transitions.NumberOfTransitions());
      DCHECK(transitions.transitions()->IsSortedNoDuplicates());
    }

    // Check existing transitions could be found after rehashing.
    {
      v8::LocalVector<v8::Value> objects(isolate);
      for (int i = 0; i < final_size; i++) {
        std::string prop_name = "prop_" + std::to_string(i);
        v8::Local<v8::String> name =
            v8::String::NewFromUtf8(isolate, prop_name.c_str(),
                                    v8::NewStringType::kNormal)
                .ToLocalChecked();
        v8::Local<v8::Object> new_obj = v8::Object::New(isolate);
        new_obj->Set(context, name, null_value).Check();
        objects.push_back(new_obj);
      }

      context->Global()
          ->Set(context, v8_str("objects_for_transitions_3"),
                v8::Array::New(isolate, objects.data(), objects.size()))
          .Check();

      TestTransitionsAccessor transitions(i_isolate, first_map);
      CHECK_EQ(final_size, transitions.NumberOfTransitions());
      DCHECK(transitions.transitions()->IsSortedNoDuplicates());
    }
  }

  isolate->Dispose();
  delete[] blob.data;
  FreeCurrentEmbeddedBlob();
}

}  // namespace internal
}  // namespace v8
