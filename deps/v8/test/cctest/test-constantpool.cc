// Copyright 2013 the V8 project authors. All rights reserved.

// Test constant pool array code.

#include "src/v8.h"

#include "src/factory.h"
#include "src/objects.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

static ConstantPoolArray::Type kTypes[] = { ConstantPoolArray::INT64,
                                            ConstantPoolArray::CODE_PTR,
                                            ConstantPoolArray::HEAP_PTR,
                                            ConstantPoolArray::INT32 };
static ConstantPoolArray::LayoutSection kSmall =
    ConstantPoolArray::SMALL_SECTION;
static ConstantPoolArray::LayoutSection kExtended =
    ConstantPoolArray::EXTENDED_SECTION;

Code* DummyCode(LocalContext* context) {
  CompileRun("function foo() {};");
  i::Handle<i::JSFunction> fun = v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(
          (*context)->Global()->Get(v8_str("foo"))));
  return fun->code();
}


TEST(ConstantPoolSmall) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());

  // Check construction.
  ConstantPoolArray::NumberOfEntries small(3, 1, 2, 1);
  Handle<ConstantPoolArray> array = factory->NewConstantPoolArray(small);

  int expected_counts[] = { 3, 1, 2, 1 };
  int expected_first_idx[] = { 0, 3, 4, 6 };
  int expected_last_idx[] = { 2, 3, 5, 6 };
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(expected_counts[i], array->number_of_entries(kTypes[i], kSmall));
    CHECK_EQ(expected_first_idx[i], array->first_index(kTypes[i], kSmall));
    CHECK_EQ(expected_last_idx[i], array->last_index(kTypes[i], kSmall));
  }
  CHECK(!array->is_extended_layout());

  // Check getters and setters.
  int64_t big_number = V8_2PART_UINT64_C(0x12345678, 9ABCDEF0);
  Handle<Object> object = factory->NewHeapNumber(4.0, IMMUTABLE, TENURED);
  Code* code = DummyCode(&context);
  array->set(0, big_number);
  array->set(1, 0.5);
  array->set(2, 3e-24);
  array->set(3, code->entry());
  array->set(4, code);
  array->set(5, *object);
  array->set(6, 50);
  CHECK_EQ(big_number, array->get_int64_entry(0));
  CHECK_EQ(0.5, array->get_int64_entry_as_double(1));
  CHECK_EQ(3e-24, array->get_int64_entry_as_double(2));
  CHECK_EQ(code->entry(), array->get_code_ptr_entry(3));
  CHECK_EQ(code, array->get_heap_ptr_entry(4));
  CHECK_EQ(*object, array->get_heap_ptr_entry(5));
  CHECK_EQ(50, array->get_int32_entry(6));
}


TEST(ConstantPoolExtended) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());

  // Check construction.
  ConstantPoolArray::NumberOfEntries small(1, 2, 3, 4);
  ConstantPoolArray::NumberOfEntries extended(5, 6, 7, 8);
  Handle<ConstantPoolArray> array =
      factory->NewExtendedConstantPoolArray(small, extended);

  // Check small section.
  int small_counts[] = { 1, 2, 3, 4 };
  int small_first_idx[] = { 0, 1, 3, 6 };
  int small_last_idx[] = { 0, 2, 5, 9 };
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(small_counts[i], array->number_of_entries(kTypes[i], kSmall));
    CHECK_EQ(small_first_idx[i], array->first_index(kTypes[i], kSmall));
    CHECK_EQ(small_last_idx[i], array->last_index(kTypes[i], kSmall));
  }

  // Check extended layout.
  CHECK(array->is_extended_layout());
  int extended_counts[] = { 5, 6, 7, 8 };
  int extended_first_idx[] = { 10, 15, 21, 28 };
  int extended_last_idx[] = { 14, 20, 27, 35 };
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(extended_counts[i],
             array->number_of_entries(kTypes[i], kExtended));
    CHECK_EQ(extended_first_idx[i], array->first_index(kTypes[i], kExtended));
    CHECK_EQ(extended_last_idx[i], array->last_index(kTypes[i], kExtended));
  }

  // Check small and large section's don't overlap.
  int64_t small_section_int64 = V8_2PART_UINT64_C(0x56781234, DEF09ABC);
  Code* small_section_code_ptr = DummyCode(&context);
  Handle<Object> small_section_heap_ptr =
      factory->NewHeapNumber(4.0, IMMUTABLE, TENURED);
  int32_t small_section_int32 = 0xab12cd45;

  int64_t extended_section_int64 = V8_2PART_UINT64_C(0x12345678, 9ABCDEF0);
  Code* extended_section_code_ptr = DummyCode(&context);
  Handle<Object> extended_section_heap_ptr =
      factory->NewHeapNumber(5.0, IMMUTABLE, TENURED);
  int32_t extended_section_int32 = 0xef67ab89;

  for (int i = array->first_index(ConstantPoolArray::INT64, kSmall);
       i <= array->last_index(ConstantPoolArray::INT32, kSmall); i++) {
    if (i <= array->last_index(ConstantPoolArray::INT64, kSmall)) {
      array->set(i, small_section_int64);
    } else if (i <= array->last_index(ConstantPoolArray::CODE_PTR, kSmall)) {
      array->set(i, small_section_code_ptr->entry());
    } else if (i <= array->last_index(ConstantPoolArray::HEAP_PTR, kSmall)) {
      array->set(i, *small_section_heap_ptr);
    } else {
      CHECK(i <= array->last_index(ConstantPoolArray::INT32, kSmall));
      array->set(i, small_section_int32);
    }
  }
  for (int i = array->first_index(ConstantPoolArray::INT64, kExtended);
       i <= array->last_index(ConstantPoolArray::INT32, kExtended); i++) {
    if (i <= array->last_index(ConstantPoolArray::INT64, kExtended)) {
      array->set(i, extended_section_int64);
    } else if (i <= array->last_index(ConstantPoolArray::CODE_PTR, kExtended)) {
      array->set(i, extended_section_code_ptr->entry());
    } else if (i <= array->last_index(ConstantPoolArray::HEAP_PTR, kExtended)) {
      array->set(i, *extended_section_heap_ptr);
    } else {
      CHECK(i <= array->last_index(ConstantPoolArray::INT32, kExtended));
      array->set(i, extended_section_int32);
    }
  }

  for (int i = array->first_index(ConstantPoolArray::INT64, kSmall);
       i <= array->last_index(ConstantPoolArray::INT32, kSmall); i++) {
    if (i <= array->last_index(ConstantPoolArray::INT64, kSmall)) {
      CHECK_EQ(small_section_int64, array->get_int64_entry(i));
    } else if (i <= array->last_index(ConstantPoolArray::CODE_PTR, kSmall)) {
      CHECK_EQ(small_section_code_ptr->entry(), array->get_code_ptr_entry(i));
    } else if (i <= array->last_index(ConstantPoolArray::HEAP_PTR, kSmall)) {
      CHECK_EQ(*small_section_heap_ptr, array->get_heap_ptr_entry(i));
    } else {
      CHECK(i <= array->last_index(ConstantPoolArray::INT32, kSmall));
      CHECK_EQ(small_section_int32, array->get_int32_entry(i));
    }
  }
  for (int i = array->first_index(ConstantPoolArray::INT64, kExtended);
       i <= array->last_index(ConstantPoolArray::INT32, kExtended); i++) {
    if (i <= array->last_index(ConstantPoolArray::INT64, kExtended)) {
      CHECK_EQ(extended_section_int64, array->get_int64_entry(i));
    } else if (i <= array->last_index(ConstantPoolArray::CODE_PTR, kExtended)) {
      CHECK_EQ(extended_section_code_ptr->entry(),
               array->get_code_ptr_entry(i));
    } else if (i <= array->last_index(ConstantPoolArray::HEAP_PTR, kExtended)) {
      CHECK_EQ(*extended_section_heap_ptr, array->get_heap_ptr_entry(i));
    } else {
      CHECK(i <= array->last_index(ConstantPoolArray::INT32, kExtended));
      CHECK_EQ(extended_section_int32, array->get_int32_entry(i));
    }
  }
}


static void CheckIterator(Handle<ConstantPoolArray> array,
                          ConstantPoolArray::Type type,
                          int expected_indexes[],
                          int count) {
  int i = 0;
  ConstantPoolArray::Iterator iter(*array, type);
  while (!iter.is_finished()) {
    CHECK_EQ(expected_indexes[i++], iter.next_index());
  }
  CHECK_EQ(count, i);
}


TEST(ConstantPoolIteratorSmall) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());

  ConstantPoolArray::NumberOfEntries small(1, 5, 2, 0);
  Handle<ConstantPoolArray> array = factory->NewConstantPoolArray(small);

  int expected_int64_indexs[] = { 0 };
  CheckIterator(array, ConstantPoolArray::INT64, expected_int64_indexs, 1);
  int expected_code_indexs[] = { 1, 2, 3, 4, 5 };
  CheckIterator(array, ConstantPoolArray::CODE_PTR, expected_code_indexs, 5);
  int expected_heap_indexs[] = { 6, 7 };
  CheckIterator(array, ConstantPoolArray::HEAP_PTR, expected_heap_indexs, 2);
  int expected_int32_indexs[1];
  CheckIterator(array, ConstantPoolArray::INT32, expected_int32_indexs, 0);
}


TEST(ConstantPoolIteratorExtended) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());

  ConstantPoolArray::NumberOfEntries small(1, 0, 0, 4);
  ConstantPoolArray::NumberOfEntries extended(5, 0, 3, 0);
  Handle<ConstantPoolArray> array =
      factory->NewExtendedConstantPoolArray(small, extended);

  int expected_int64_indexs[] = { 0, 5, 6, 7, 8, 9 };
  CheckIterator(array, ConstantPoolArray::INT64, expected_int64_indexs, 6);
  int expected_code_indexs[1];
  CheckIterator(array, ConstantPoolArray::CODE_PTR, expected_code_indexs, 0);
  int expected_heap_indexs[] = { 10, 11, 12 };
  CheckIterator(array, ConstantPoolArray::HEAP_PTR, expected_heap_indexs, 3);
  int expected_int32_indexs[] = { 1, 2, 3, 4 };
  CheckIterator(array, ConstantPoolArray::INT32, expected_int32_indexs, 4);
}


TEST(ConstantPoolPreciseGC) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());

  ConstantPoolArray::NumberOfEntries small(1, 0, 0, 1);
  Handle<ConstantPoolArray> array = factory->NewConstantPoolArray(small);

  // Check that the store buffer knows which entries are pointers and which are
  // not.  To do this, make non-pointer entries which look like new space
  // pointers but are actually invalid and ensure the GC doesn't try to move
  // them.
  Handle<HeapObject> object = factory->NewHeapNumber(4.0);
  Object* raw_ptr = *object;
  // If interpreted as a pointer, this should be right inside the heap number
  // which will cause a crash when trying to lookup the 'map' pointer.
  intptr_t invalid_ptr = reinterpret_cast<intptr_t>(raw_ptr) + kInt32Size;
  int32_t invalid_ptr_int32 = static_cast<int32_t>(invalid_ptr);
  int64_t invalid_ptr_int64 = static_cast<int64_t>(invalid_ptr);
  array->set(0, invalid_ptr_int64);
  array->set(1, invalid_ptr_int32);

  // Ensure we perform a scan on scavenge for the constant pool's page.
  MemoryChunk::FromAddress(array->address())->set_scan_on_scavenge(true);
  heap->CollectGarbage(NEW_SPACE);

  // Check the object was moved by GC.
  CHECK_NE(*object, raw_ptr);

  // Check the non-pointer entries weren't changed.
  CHECK_EQ(invalid_ptr_int64, array->get_int64_entry(0));
  CHECK_EQ(invalid_ptr_int32, array->get_int32_entry(1));
}


TEST(ConstantPoolCompacting) {
  if (i::FLAG_never_compact) return;
  i::FLAG_always_compact = true;
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());

  ConstantPoolArray::NumberOfEntries small(0, 0, 1, 0);
  ConstantPoolArray::NumberOfEntries extended(0, 0, 1, 0);
  Handle<ConstantPoolArray> array =
      factory->NewExtendedConstantPoolArray(small, extended);

  // Start a second old-space page so that the heap pointer added to the
  // constant pool array ends up on the an evacuation candidate page.
  Page* first_page = heap->old_data_space()->anchor()->next_page();
  {
    HandleScope scope(isolate);
    Handle<HeapObject> temp =
        factory->NewFixedDoubleArray(900 * KB / kDoubleSize, TENURED);
    CHECK(heap->InOldDataSpace(temp->address()));
    Handle<HeapObject> heap_ptr =
        factory->NewHeapNumber(5.0, IMMUTABLE, TENURED);
    CHECK(heap->InOldDataSpace(heap_ptr->address()));
    CHECK(!first_page->Contains(heap_ptr->address()));
    array->set(0, *heap_ptr);
    array->set(1, *heap_ptr);
  }

  // Check heap pointers are correctly updated on GC.
  Object* old_ptr = array->get_heap_ptr_entry(0);
  Handle<Object> object(old_ptr, isolate);
  CHECK_EQ(old_ptr, *object);
  CHECK_EQ(old_ptr, array->get_heap_ptr_entry(1));

  // Force compacting garbage collection.
  CHECK(FLAG_always_compact);
  heap->CollectAllGarbage(Heap::kNoGCFlags);

  CHECK_NE(old_ptr, *object);
  CHECK_EQ(*object, array->get_heap_ptr_entry(0));
  CHECK_EQ(*object, array->get_heap_ptr_entry(1));
}
