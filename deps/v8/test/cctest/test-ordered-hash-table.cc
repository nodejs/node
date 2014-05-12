// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"

#include "cctest.h"
#include "factory.h"

namespace {

using namespace v8::internal;


void CheckIterResultObject(Isolate* isolate,
                           Handle<JSObject> result,
                           Handle<Object> value,
                           bool done) {
  Handle<Object> value_object =
      Object::GetProperty(isolate, result, "value").ToHandleChecked();
  Handle<Object> done_object =
      Object::GetProperty(isolate, result, "done").ToHandleChecked();

  CHECK_EQ(*value_object, *value);
  CHECK(done_object->IsBoolean());
  CHECK_EQ(done_object->BooleanValue(), done);
}


TEST(Set) {
  i::FLAG_harmony_collections = true;

  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<OrderedHashSet> ordered_set = factory->NewOrderedHashSet();
  CHECK_EQ(2, ordered_set->NumberOfBuckets());
  CHECK_EQ(0, ordered_set->NumberOfElements());
  CHECK_EQ(0, ordered_set->NumberOfDeletedElements());

  Handle<JSSetIterator> value_iterator =
      JSSetIterator::Create(ordered_set, JSSetIterator::kKindValues);
  Handle<JSSetIterator> value_iterator_2 =
      JSSetIterator::Create(ordered_set, JSSetIterator::kKindValues);

  Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  Handle<JSObject> obj = factory->NewJSObjectFromMap(map);
  CHECK(!ordered_set->Contains(obj));
  ordered_set = OrderedHashSet::Add(ordered_set, obj);
  CHECK_EQ(1, ordered_set->NumberOfElements());
  CHECK(ordered_set->Contains(obj));
  ordered_set = OrderedHashSet::Remove(ordered_set, obj);
  CHECK_EQ(0, ordered_set->NumberOfElements());
  CHECK(!ordered_set->Contains(obj));

  // Test for collisions/chaining
  Handle<JSObject> obj1 = factory->NewJSObjectFromMap(map);
  ordered_set = OrderedHashSet::Add(ordered_set, obj1);
  Handle<JSObject> obj2 = factory->NewJSObjectFromMap(map);
  ordered_set = OrderedHashSet::Add(ordered_set, obj2);
  Handle<JSObject> obj3 = factory->NewJSObjectFromMap(map);
  ordered_set = OrderedHashSet::Add(ordered_set, obj3);
  CHECK_EQ(3, ordered_set->NumberOfElements());
  CHECK(ordered_set->Contains(obj1));
  CHECK(ordered_set->Contains(obj2));
  CHECK(ordered_set->Contains(obj3));

  // Test iteration
  CheckIterResultObject(
      isolate, JSSetIterator::Next(value_iterator), obj1, false);
  CheckIterResultObject(
      isolate, JSSetIterator::Next(value_iterator), obj2, false);
  CheckIterResultObject(
      isolate, JSSetIterator::Next(value_iterator), obj3, false);
  CheckIterResultObject(isolate,
                        JSSetIterator::Next(value_iterator),
                        factory->undefined_value(),
                        true);

  // Test growth
  ordered_set = OrderedHashSet::Add(ordered_set, obj);
  Handle<JSObject> obj4 = factory->NewJSObjectFromMap(map);
  ordered_set = OrderedHashSet::Add(ordered_set, obj4);
  CHECK(ordered_set->Contains(obj));
  CHECK(ordered_set->Contains(obj1));
  CHECK(ordered_set->Contains(obj2));
  CHECK(ordered_set->Contains(obj3));
  CHECK(ordered_set->Contains(obj4));
  CHECK_EQ(5, ordered_set->NumberOfElements());
  CHECK_EQ(0, ordered_set->NumberOfDeletedElements());
  CHECK_EQ(4, ordered_set->NumberOfBuckets());

  // Test iteration after growth
  CheckIterResultObject(
      isolate, JSSetIterator::Next(value_iterator_2), obj1, false);
  CheckIterResultObject(
      isolate, JSSetIterator::Next(value_iterator_2), obj2, false);
  CheckIterResultObject(
      isolate, JSSetIterator::Next(value_iterator_2), obj3, false);
  CheckIterResultObject(
      isolate, JSSetIterator::Next(value_iterator_2), obj, false);
  CheckIterResultObject(
      isolate, JSSetIterator::Next(value_iterator_2), obj4, false);
  CheckIterResultObject(isolate,
                        JSSetIterator::Next(value_iterator_2),
                        factory->undefined_value(),
                        true);

  // Test shrinking
  ordered_set = OrderedHashSet::Remove(ordered_set, obj);
  ordered_set = OrderedHashSet::Remove(ordered_set, obj1);
  ordered_set = OrderedHashSet::Remove(ordered_set, obj2);
  ordered_set = OrderedHashSet::Remove(ordered_set, obj3);
  CHECK_EQ(1, ordered_set->NumberOfElements());
  CHECK_EQ(2, ordered_set->NumberOfBuckets());
}


TEST(Map) {
  i::FLAG_harmony_collections = true;

  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  Handle<OrderedHashMap> ordered_map = factory->NewOrderedHashMap();
  CHECK_EQ(2, ordered_map->NumberOfBuckets());
  CHECK_EQ(0, ordered_map->NumberOfElements());
  CHECK_EQ(0, ordered_map->NumberOfDeletedElements());

  Handle<JSMapIterator> value_iterator =
      JSMapIterator::Create(ordered_map, JSMapIterator::kKindValues);
  Handle<JSMapIterator> key_iterator =
      JSMapIterator::Create(ordered_map, JSMapIterator::kKindKeys);

  Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  Handle<JSObject> obj = factory->NewJSObjectFromMap(map);
  Handle<JSObject> val = factory->NewJSObjectFromMap(map);
  CHECK(ordered_map->Lookup(obj)->IsTheHole());
  ordered_map = OrderedHashMap::Put(ordered_map, obj, val);
  CHECK_EQ(1, ordered_map->NumberOfElements());
  CHECK(ordered_map->Lookup(obj)->SameValue(*val));
  ordered_map = OrderedHashMap::Put(
      ordered_map, obj, factory->the_hole_value());
  CHECK_EQ(0, ordered_map->NumberOfElements());
  CHECK(ordered_map->Lookup(obj)->IsTheHole());

  // Test for collisions/chaining
  Handle<JSObject> obj1 = factory->NewJSObjectFromMap(map);
  Handle<JSObject> obj2 = factory->NewJSObjectFromMap(map);
  Handle<JSObject> obj3 = factory->NewJSObjectFromMap(map);
  Handle<JSObject> val1 = factory->NewJSObjectFromMap(map);
  Handle<JSObject> val2 = factory->NewJSObjectFromMap(map);
  Handle<JSObject> val3 = factory->NewJSObjectFromMap(map);
  ordered_map = OrderedHashMap::Put(ordered_map, obj1, val1);
  ordered_map = OrderedHashMap::Put(ordered_map, obj2, val2);
  ordered_map = OrderedHashMap::Put(ordered_map, obj3, val3);
  CHECK_EQ(3, ordered_map->NumberOfElements());
  CHECK(ordered_map->Lookup(obj1)->SameValue(*val1));
  CHECK(ordered_map->Lookup(obj2)->SameValue(*val2));
  CHECK(ordered_map->Lookup(obj3)->SameValue(*val3));

  // Test iteration
  CheckIterResultObject(
      isolate, JSMapIterator::Next(value_iterator), val1, false);
  CheckIterResultObject(
      isolate, JSMapIterator::Next(value_iterator), val2, false);
  CheckIterResultObject(
      isolate, JSMapIterator::Next(value_iterator), val3, false);
  CheckIterResultObject(isolate,
                        JSMapIterator::Next(value_iterator),
                        factory->undefined_value(),
                        true);

  // Test growth
  ordered_map = OrderedHashMap::Put(ordered_map, obj, val);
  Handle<JSObject> obj4 = factory->NewJSObjectFromMap(map);
  Handle<JSObject> val4 = factory->NewJSObjectFromMap(map);
  ordered_map = OrderedHashMap::Put(ordered_map, obj4, val4);
  CHECK(ordered_map->Lookup(obj)->SameValue(*val));
  CHECK(ordered_map->Lookup(obj1)->SameValue(*val1));
  CHECK(ordered_map->Lookup(obj2)->SameValue(*val2));
  CHECK(ordered_map->Lookup(obj3)->SameValue(*val3));
  CHECK(ordered_map->Lookup(obj4)->SameValue(*val4));
  CHECK_EQ(5, ordered_map->NumberOfElements());
  CHECK_EQ(4, ordered_map->NumberOfBuckets());

  // Test iteration after growth
  CheckIterResultObject(
      isolate, JSMapIterator::Next(key_iterator), obj1, false);
  CheckIterResultObject(
      isolate, JSMapIterator::Next(key_iterator), obj2, false);
  CheckIterResultObject(
      isolate, JSMapIterator::Next(key_iterator), obj3, false);
  CheckIterResultObject(
      isolate, JSMapIterator::Next(key_iterator), obj, false);
  CheckIterResultObject(
      isolate, JSMapIterator::Next(key_iterator), obj4, false);
  CheckIterResultObject(isolate,
                        JSMapIterator::Next(key_iterator),
                        factory->undefined_value(),
                        true);

  // Test shrinking
  ordered_map = OrderedHashMap::Put(
      ordered_map, obj, factory->the_hole_value());
  ordered_map = OrderedHashMap::Put(
      ordered_map, obj1, factory->the_hole_value());
  ordered_map = OrderedHashMap::Put(
      ordered_map, obj2, factory->the_hole_value());
  ordered_map = OrderedHashMap::Put(
      ordered_map, obj3, factory->the_hole_value());
  CHECK_EQ(1, ordered_map->NumberOfElements());
  CHECK_EQ(2, ordered_map->NumberOfBuckets());
}


}
