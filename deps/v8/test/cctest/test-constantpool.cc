// Copyright 2013 the V8 project authors. All rights reserved.

// Test constant pool array code.

#include "v8.h"

#include "factory.h"
#include "objects.h"
#include "cctest.h"

using namespace v8::internal;


Code* DummyCode(LocalContext* context) {
  CompileRun("function foo() {};");
  i::Handle<i::JSFunction> fun = v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(
          (*context)->Global()->Get(v8_str("foo"))));
  return fun->code();
}


TEST(ConstantPool) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(context->GetIsolate());

  // Check construction.
  Handle<ConstantPoolArray> array = factory->NewConstantPoolArray(3, 1, 2, 1);
  CHECK_EQ(array->count_of_int64_entries(), 3);
  CHECK_EQ(array->count_of_code_ptr_entries(), 1);
  CHECK_EQ(array->count_of_heap_ptr_entries(), 2);
  CHECK_EQ(array->count_of_int32_entries(), 1);
  CHECK_EQ(array->length(), 7);
  CHECK_EQ(array->first_int64_index(), 0);
  CHECK_EQ(array->first_code_ptr_index(), 3);
  CHECK_EQ(array->first_heap_ptr_index(), 4);
  CHECK_EQ(array->first_int32_index(), 6);

  // Check getters and setters.
  int64_t big_number = V8_2PART_UINT64_C(0x12345678, 9ABCDEF0);
  Handle<Object> object = factory->NewHeapNumber(4.0);
  Code* code = DummyCode(&context);
  array->set(0, big_number);
  array->set(1, 0.5);
  array->set(2, 3e-24);
  array->set(3, code->entry());
  array->set(4, code);
  array->set(5, *object);
  array->set(6, 50);
  CHECK_EQ(array->get_int64_entry(0), big_number);
  CHECK_EQ(array->get_int64_entry_as_double(1), 0.5);
  CHECK_EQ(array->get_int64_entry_as_double(2), 3e-24);
  CHECK_EQ(array->get_code_ptr_entry(3), code->entry());
  CHECK_EQ(array->get_heap_ptr_entry(4), code);
  CHECK_EQ(array->get_heap_ptr_entry(5), *object);
  CHECK_EQ(array->get_int32_entry(6), 50);

  // Check pointers are updated on GC.
  Object* old_ptr = array->get_heap_ptr_entry(5);
  CHECK_EQ(*object, old_ptr);
  heap->CollectGarbage(NEW_SPACE);
  Object* new_ptr = array->get_heap_ptr_entry(5);
  CHECK_NE(*object, old_ptr);
  CHECK_EQ(*object, new_ptr);
}
