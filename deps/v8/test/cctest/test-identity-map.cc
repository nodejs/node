// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"
#include "src/identity-map.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/zone/zone.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/factory.h -> src/objects-inl.h
#include "src/objects-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/type-feedback-vector.h ->
// src/type-feedback-vector-inl.h
#include "src/type-feedback-vector-inl.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

// Helper for testing. A "friend" of the IdentityMapBase class, it is able to
// "move" objects to simulate GC for testing the internals of the map.
class IdentityMapTester : public HandleAndZoneScope {
 public:
  IdentityMap<void*> map;

  IdentityMapTester() : map(heap(), main_zone()) {}

  Heap* heap() { return isolate()->heap(); }
  Isolate* isolate() { return main_isolate(); }

  void TestGetFind(Handle<Object> key1, void* val1, Handle<Object> key2,
                   void* val2) {
    CHECK_NULL(map.Find(key1));
    CHECK_NULL(map.Find(key2));

    // Set {key1} the first time.
    void** entry = map.Get(key1);
    CHECK_NOT_NULL(entry);
    *entry = val1;

    for (int i = 0; i < 3; i++) {  // Get and find {key1} K times.
      {
        void** nentry = map.Get(key1);
        CHECK_EQ(entry, nentry);
        CHECK_EQ(val1, *nentry);
        CHECK_NULL(map.Find(key2));
      }
      {
        void** nentry = map.Find(key1);
        CHECK_EQ(entry, nentry);
        CHECK_EQ(val1, *nentry);
        CHECK_NULL(map.Find(key2));
      }
    }

    // Set {key2} the first time.
    void** entry2 = map.Get(key2);
    CHECK_NOT_NULL(entry2);
    *entry2 = val2;

    for (int i = 0; i < 3; i++) {  // Get and find {key1} and {key2} K times.
      {
        void** nentry = map.Get(key2);
        CHECK_EQ(entry2, nentry);
        CHECK_EQ(val2, *nentry);
      }
      {
        void** nentry = map.Find(key2);
        CHECK_EQ(entry2, nentry);
        CHECK_EQ(val2, *nentry);
      }
      {
        void** nentry = map.Find(key1);
        CHECK_EQ(val1, *nentry);
      }
    }
  }

  Handle<Smi> smi(int value) {
    return Handle<Smi>(Smi::FromInt(value), isolate());
  }

  Handle<Object> num(double value) {
    return isolate()->factory()->NewNumber(value);
  }

  void SimulateGCByIncrementingSmisBy(int shift) {
    for (int i = 0; i < map.size_; i++) {
      if (map.keys_[i]->IsSmi()) {
        map.keys_[i] = Smi::FromInt(Smi::cast(map.keys_[i])->value() + shift);
      }
    }
    map.gc_counter_ = -1;
  }

  void CheckFind(Handle<Object> key, void* value) {
    void** entry = map.Find(key);
    CHECK_NOT_NULL(entry);
    CHECK_EQ(value, *entry);
  }

  void CheckGet(Handle<Object> key, void* value) {
    void** entry = map.Get(key);
    CHECK_NOT_NULL(entry);
    CHECK_EQ(value, *entry);
  }

  void PrintMap() {
    PrintF("{\n");
    for (int i = 0; i < map.size_; i++) {
      PrintF("  %3d: %p => %p\n", i, reinterpret_cast<void*>(map.keys_[i]),
             reinterpret_cast<void*>(map.values_[i]));
    }
    PrintF("}\n");
  }

  void Resize() { map.Resize(); }

  void Rehash() { map.Rehash(); }
};


TEST(Find_smi_not_found) {
  IdentityMapTester t;
  for (int i = 0; i < 100; i++) {
    CHECK_NULL(t.map.Find(t.smi(i)));
  }
}


TEST(Find_num_not_found) {
  IdentityMapTester t;
  for (int i = 0; i < 100; i++) {
    CHECK_NULL(t.map.Find(t.num(i + 0.2)));
  }
}


TEST(GetFind_smi_13) {
  IdentityMapTester t;
  t.TestGetFind(t.smi(13), t.isolate(), t.smi(17), t.heap());
}


TEST(GetFind_num_13) {
  IdentityMapTester t;
  t.TestGetFind(t.num(13.1), t.isolate(), t.num(17.1), t.heap());
}


TEST(GetFind_smi_17m) {
  const int kInterval = 17;
  const int kShift = 1099;
  IdentityMapTester t;

  for (int i = 1; i < 100; i += kInterval) {
    t.map.Set(t.smi(i), reinterpret_cast<void*>(i + kShift));
  }

  for (int i = 1; i < 100; i += kInterval) {
    t.CheckFind(t.smi(i), reinterpret_cast<void*>(i + kShift));
  }

  for (int i = 1; i < 100; i += kInterval) {
    t.CheckGet(t.smi(i), reinterpret_cast<void*>(i + kShift));
  }

  for (int i = 1; i < 100; i++) {
    void** entry = t.map.Find(t.smi(i));
    if ((i % kInterval) != 1) {
      CHECK_NULL(entry);
    } else {
      CHECK_NOT_NULL(entry);
      CHECK_EQ(reinterpret_cast<void*>(i + kShift), *entry);
    }
  }
}


TEST(GetFind_num_1000) {
  const int kPrime = 137;
  IdentityMapTester t;
  int val1;
  int val2;

  for (int i = 0; i < 1000; i++) {
    t.TestGetFind(t.smi(i * kPrime), &val1, t.smi(i * kPrime + 1), &val2);
  }
}


TEST(GetFind_smi_gc) {
  const int kKey = 33;
  const int kShift = 1211;
  IdentityMapTester t;

  t.map.Set(t.smi(kKey), &t);
  t.SimulateGCByIncrementingSmisBy(kShift);
  t.CheckFind(t.smi(kKey + kShift), &t);
  t.CheckGet(t.smi(kKey + kShift), &t);
}


TEST(GetFind_smi_gc2) {
  int kKey1 = 1;
  int kKey2 = 33;
  const int kShift = 1211;
  IdentityMapTester t;

  t.map.Set(t.smi(kKey1), &kKey1);
  t.map.Set(t.smi(kKey2), &kKey2);
  t.SimulateGCByIncrementingSmisBy(kShift);
  t.CheckFind(t.smi(kKey1 + kShift), &kKey1);
  t.CheckGet(t.smi(kKey1 + kShift), &kKey1);
  t.CheckFind(t.smi(kKey2 + kShift), &kKey2);
  t.CheckGet(t.smi(kKey2 + kShift), &kKey2);
}


TEST(GetFind_smi_gc_n) {
  const int kShift = 12011;
  IdentityMapTester t;
  int keys[12] = {1,      2,      7,      8,      15,      23,
                  1 + 32, 2 + 32, 7 + 32, 8 + 32, 15 + 32, 23 + 32};
  // Initialize the map first.
  for (size_t i = 0; i < arraysize(keys); i += 2) {
    t.TestGetFind(t.smi(keys[i]), &keys[i], t.smi(keys[i + 1]), &keys[i + 1]);
  }
  // Check the above initialization.
  for (size_t i = 0; i < arraysize(keys); i++) {
    t.CheckFind(t.smi(keys[i]), &keys[i]);
  }
  // Simulate a GC by "moving" the smis in the internal keys array.
  t.SimulateGCByIncrementingSmisBy(kShift);
  // Check that searching for the incremented smis finds the same values.
  for (size_t i = 0; i < arraysize(keys); i++) {
    t.CheckFind(t.smi(keys[i] + kShift), &keys[i]);
  }
  // Check that searching for the incremented smis gets the same values.
  for (size_t i = 0; i < arraysize(keys); i++) {
    t.CheckGet(t.smi(keys[i] + kShift), &keys[i]);
  }
}


TEST(GetFind_smi_num_gc_n) {
  const int kShift = 12019;
  IdentityMapTester t;
  int smi_keys[] = {1, 2, 7, 15, 23};
  Handle<Object> num_keys[] = {t.num(1.1), t.num(2.2), t.num(3.3), t.num(4.4),
                               t.num(5.5), t.num(6.6), t.num(7.7), t.num(8.8),
                               t.num(9.9), t.num(10.1)};
  // Initialize the map first.
  for (size_t i = 0; i < arraysize(smi_keys); i++) {
    t.map.Set(t.smi(smi_keys[i]), &smi_keys[i]);
  }
  for (size_t i = 0; i < arraysize(num_keys); i++) {
    t.map.Set(num_keys[i], &num_keys[i]);
  }
  // Check the above initialization.
  for (size_t i = 0; i < arraysize(smi_keys); i++) {
    t.CheckFind(t.smi(smi_keys[i]), &smi_keys[i]);
  }
  for (size_t i = 0; i < arraysize(num_keys); i++) {
    t.CheckFind(num_keys[i], &num_keys[i]);
  }

  // Simulate a GC by moving SMIs.
  // Ironically the SMIs "move", but the heap numbers don't!
  t.SimulateGCByIncrementingSmisBy(kShift);

  // Check that searching for the incremented smis finds the same values.
  for (size_t i = 0; i < arraysize(smi_keys); i++) {
    t.CheckFind(t.smi(smi_keys[i] + kShift), &smi_keys[i]);
    t.CheckGet(t.smi(smi_keys[i] + kShift), &smi_keys[i]);
  }

  // Check that searching for the numbers finds the same values.
  for (size_t i = 0; i < arraysize(num_keys); i++) {
    t.CheckFind(num_keys[i], &num_keys[i]);
    t.CheckGet(num_keys[i], &num_keys[i]);
  }
}


void CollisionTest(int stride, bool rehash = false, bool resize = false) {
  for (int load = 15; load <= 120; load = load * 2) {
    IdentityMapTester t;

    {  // Add entries to the map.
      HandleScope scope(t.isolate());
      int next = 1;
      for (int i = 0; i < load; i++) {
        t.map.Set(t.smi(next), reinterpret_cast<void*>(next));
        t.CheckFind(t.smi(next), reinterpret_cast<void*>(next));
        next = next + stride;
      }
    }
    if (resize) t.Resize();  // Explicit resize (internal method).
    if (rehash) t.Rehash();  // Explicit rehash (internal method).
    {                        // Check find and get.
      HandleScope scope(t.isolate());
      int next = 1;
      for (int i = 0; i < load; i++) {
        t.CheckFind(t.smi(next), reinterpret_cast<void*>(next));
        t.CheckGet(t.smi(next), reinterpret_cast<void*>(next));
        next = next + stride;
      }
    }
  }
}


TEST(Collisions_1) { CollisionTest(1); }
TEST(Collisions_2) { CollisionTest(2); }
TEST(Collisions_3) { CollisionTest(3); }
TEST(Collisions_5) { CollisionTest(5); }
TEST(Collisions_7) { CollisionTest(7); }
TEST(Resize) { CollisionTest(9, false, true); }
TEST(Rehash) { CollisionTest(11, true, false); }


TEST(ExplicitGC) {
  IdentityMapTester t;
  Handle<Object> num_keys[] = {t.num(2.1), t.num(2.4), t.num(3.3), t.num(4.3),
                               t.num(7.5), t.num(6.4), t.num(7.3), t.num(8.3),
                               t.num(8.9), t.num(10.4)};

  // Insert some objects that should be in new space.
  for (size_t i = 0; i < arraysize(num_keys); i++) {
    t.map.Set(num_keys[i], &num_keys[i]);
  }

  // Do an explicit, real GC.
  t.heap()->CollectGarbage(i::NEW_SPACE, i::GarbageCollectionReason::kTesting);

  // Check that searching for the numbers finds the same values.
  for (size_t i = 0; i < arraysize(num_keys); i++) {
    t.CheckFind(num_keys[i], &num_keys[i]);
    t.CheckGet(num_keys[i], &num_keys[i]);
  }
}


TEST(CanonicalHandleScope) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  HandleScope outer(isolate);
  CanonicalHandleScope outer_canonical(isolate);

  // Deduplicate smi handles.
  List<Handle<Object> > smi_handles;
  for (int i = 0; i < 100; i++) {
    smi_handles.Add(Handle<Object>(Smi::FromInt(i), isolate));
  }
  Object** next_handle = isolate->handle_scope_data()->next;
  for (int i = 0; i < 100; i++) {
    Handle<Object> new_smi = Handle<Object>(Smi::FromInt(i), isolate);
    Handle<Object> old_smi = smi_handles[i];
    CHECK_EQ(new_smi.location(), old_smi.location());
  }
  // Check that no new handles have been allocated.
  CHECK_EQ(next_handle, isolate->handle_scope_data()->next);

  // Deduplicate root list items.
  Handle<String> empty_string(heap->empty_string());
  Handle<Map> free_space_map(heap->free_space_map());
  Handle<Symbol> uninitialized_symbol(heap->uninitialized_symbol());
  CHECK_EQ(isolate->factory()->empty_string().location(),
           empty_string.location());
  CHECK_EQ(isolate->factory()->free_space_map().location(),
           free_space_map.location());
  CHECK_EQ(isolate->factory()->uninitialized_symbol().location(),
           uninitialized_symbol.location());
  // Check that no new handles have been allocated.
  CHECK_EQ(next_handle, isolate->handle_scope_data()->next);

  // Test ordinary heap objects.
  Handle<HeapNumber> number1 = isolate->factory()->NewHeapNumber(3.3);
  Handle<String> string1 =
      isolate->factory()->NewStringFromAsciiChecked("test");
  next_handle = isolate->handle_scope_data()->next;
  Handle<HeapNumber> number2(*number1);
  Handle<String> string2(*string1);
  CHECK_EQ(number1.location(), number2.location());
  CHECK_EQ(string1.location(), string2.location());
  CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);
  Handle<HeapNumber> number3(*number2);
  Handle<String> string3(*string2);
  CHECK_EQ(number1.location(), number3.location());
  CHECK_EQ(string1.location(), string3.location());
  // Check that no new handles have been allocated.
  CHECK_EQ(next_handle, isolate->handle_scope_data()->next);

  // Inner handle scope do not create canonical handles.
  {
    HandleScope inner(isolate);
    Handle<HeapNumber> number4(*number1);
    Handle<String> string4(*string1);
    CHECK_NE(number1.location(), number4.location());
    CHECK_NE(string1.location(), string4.location());

    // Nested canonical scope does not conflict with outer canonical scope,
    // but does not canonicalize across scopes.
    CanonicalHandleScope inner_canonical(isolate);
    Handle<HeapNumber> number5(*number4);
    Handle<String> string5(*string4);
    CHECK_NE(number4.location(), number5.location());
    CHECK_NE(string4.location(), string5.location());
    CHECK_NE(number1.location(), number5.location());
    CHECK_NE(string1.location(), string5.location());

    Handle<HeapNumber> number6(*number1);
    Handle<String> string6(*string1);
    CHECK_NE(number4.location(), number6.location());
    CHECK_NE(string4.location(), string6.location());
    CHECK_NE(number1.location(), number6.location());
    CHECK_NE(string1.location(), string6.location());
    CHECK_EQ(number5.location(), number6.location());
    CHECK_EQ(string5.location(), string6.location());
  }
}

}  // namespace internal
}  // namespace v8
