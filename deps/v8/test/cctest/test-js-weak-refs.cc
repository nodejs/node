// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles-inl.h"
#include "src/heap/factory-inl.h"
#include "src/isolate.h"
#include "src/microtask-queue.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-weak-refs-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

namespace {

Handle<JSFinalizationGroup> ConstructJSFinalizationGroup(Isolate* isolate) {
  Factory* factory = isolate->factory();
  Handle<String> finalization_group_name =
      factory->NewStringFromStaticChars("FinalizationGroup");
  Handle<Object> global =
      handle(isolate->native_context()->global_object(), isolate);
  Handle<JSFunction> finalization_group_fun = Handle<JSFunction>::cast(
      Object::GetProperty(isolate, global, finalization_group_name)
          .ToHandleChecked());
  auto finalization_group = Handle<JSFinalizationGroup>::cast(
      JSObject::New(finalization_group_fun, finalization_group_fun,
                    Handle<AllocationSite>::null())
          .ToHandleChecked());
#ifdef VERIFY_HEAP
  finalization_group->JSFinalizationGroupVerify(isolate);
#endif  // VERIFY_HEAP
  return finalization_group;
}

Handle<JSWeakRef> ConstructJSWeakRef(Handle<JSReceiver> target,
                                     Isolate* isolate) {
  Factory* factory = isolate->factory();
  Handle<String> weak_ref_name = factory->WeakRef_string();
  Handle<Object> global =
      handle(isolate->native_context()->global_object(), isolate);
  Handle<JSFunction> weak_ref_fun = Handle<JSFunction>::cast(
      Object::GetProperty(isolate, global, weak_ref_name).ToHandleChecked());
  auto weak_ref = Handle<JSWeakRef>::cast(
      JSObject::New(weak_ref_fun, weak_ref_fun, Handle<AllocationSite>::null())
          .ToHandleChecked());
  weak_ref->set_target(*target);
#ifdef VERIFY_HEAP
  weak_ref->JSWeakRefVerify(isolate);
#endif  // VERIFY_HEAP
  return weak_ref;
}

Handle<JSObject> CreateKey(const char* key_prop_value, Isolate* isolate) {
  Factory* factory = isolate->factory();
  Handle<String> key_string = factory->NewStringFromStaticChars("key_string");
  Handle<JSObject> key =
      isolate->factory()->NewJSObject(isolate->object_function());
  JSObject::AddProperty(isolate, key, key_string,
                        factory->NewStringFromAsciiChecked(key_prop_value),
                        NONE);
  return key;
}

Handle<WeakCell> FinalizationGroupRegister(
    Handle<JSFinalizationGroup> finalization_group, Handle<JSObject> target,
    Handle<Object> holdings, Handle<Object> key, Isolate* isolate) {
  JSFinalizationGroup::Register(finalization_group, target, holdings, key,
                                isolate);
  CHECK(finalization_group->active_cells()->IsWeakCell());
  Handle<WeakCell> weak_cell =
      handle(WeakCell::cast(finalization_group->active_cells()), isolate);
#ifdef VERIFY_HEAP
  weak_cell->WeakCellVerify(isolate);
#endif  // VERIFY_HEAP
  return weak_cell;
}

Handle<WeakCell> FinalizationGroupRegister(
    Handle<JSFinalizationGroup> finalization_group, Handle<JSObject> target,
    Isolate* isolate) {
  Handle<Object> undefined =
      handle(ReadOnlyRoots(isolate).undefined_value(), isolate);
  return FinalizationGroupRegister(finalization_group, target, undefined,
                                   undefined, isolate);
}

void NullifyWeakCell(Handle<WeakCell> weak_cell, Isolate* isolate) {
  auto empty_func = [](HeapObject object, ObjectSlot slot, Object target) {};
  weak_cell->Nullify(isolate, empty_func);
#ifdef VERIFY_HEAP
  weak_cell->WeakCellVerify(isolate);
#endif  // VERIFY_HEAP
}

// Usage: VerifyWeakCellChain(isolate, list_head, n, cell1, cell2, ..., celln);
// verifies that list_head == cell1 and cell1, cell2, ..., celln. form a list.
void VerifyWeakCellChain(Isolate* isolate, Object list_head, int n_args, ...) {
  CHECK_GE(n_args, 0);

  va_list args;
  va_start(args, n_args);

  if (n_args == 0) {
    // Verify empty list
    CHECK(list_head->IsUndefined(isolate));
  } else {
    WeakCell current = WeakCell::cast(Object(va_arg(args, Address)));
    CHECK_EQ(current, list_head);
    CHECK(current->prev()->IsUndefined(isolate));

    for (int i = 1; i < n_args; i++) {
      WeakCell next = WeakCell::cast(Object(va_arg(args, Address)));
      CHECK_EQ(current->next(), next);
      CHECK_EQ(next->prev(), current);
      current = next;
    }
    CHECK(current->next()->IsUndefined(isolate));
  }
  va_end(args);
}

// Like VerifyWeakCellChain but verifies the chain created with key_list_prev
// and key_list_next instead of prev and next.
void VerifyWeakCellKeyChain(Isolate* isolate, Object list_head, int n_args,
                            ...) {
  CHECK_GE(n_args, 0);

  va_list args;
  va_start(args, n_args);

  if (n_args == 0) {
    // Verify empty list
    CHECK(list_head->IsTheHole(isolate));
  } else {
    WeakCell current = WeakCell::cast(Object(va_arg(args, Address)));
    CHECK_EQ(current, list_head);
    CHECK(current->key_list_prev()->IsUndefined(isolate));

    for (int i = 1; i < n_args; i++) {
      WeakCell next = WeakCell::cast(Object(va_arg(args, Address)));
      CHECK_EQ(current->key_list_next(), next);
      CHECK_EQ(next->key_list_prev(), current);
      current = next;
    }
    CHECK(current->key_list_next()->IsUndefined(isolate));
  }
  va_end(args);
}

}  // namespace

TEST(TestRegister) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Register a weak reference and verify internal data structures.
  Handle<WeakCell> weak_cell1 =
      FinalizationGroupRegister(finalization_group, js_object, isolate);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 1,
                      *weak_cell1);
  CHECK(weak_cell1->key_list_prev()->IsUndefined(isolate));
  CHECK(weak_cell1->key_list_next()->IsUndefined(isolate));

  CHECK(finalization_group->cleared_cells()->IsUndefined(isolate));

  // No key was used during registration, key-based map stays uninitialized.
  CHECK(finalization_group->key_map()->IsUndefined(isolate));

  // Register another weak reference and verify internal data structures.
  Handle<WeakCell> weak_cell2 =
      FinalizationGroupRegister(finalization_group, js_object, isolate);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 2,
                      *weak_cell2, *weak_cell1);
  CHECK(weak_cell2->key_list_prev()->IsUndefined(isolate));
  CHECK(weak_cell2->key_list_next()->IsUndefined(isolate));

  CHECK(finalization_group->cleared_cells()->IsUndefined(isolate));
  CHECK(finalization_group->key_map()->IsUndefined(isolate));
}

TEST(TestRegisterWithKey) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSObject> key1 = CreateKey("key1", isolate);
  Handle<JSObject> key2 = CreateKey("key2", isolate);
  Handle<Object> undefined =
      handle(ReadOnlyRoots(isolate).undefined_value(), isolate);

  // Register a weak reference with a key and verify internal data structures.
  Handle<WeakCell> weak_cell1 = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key1, isolate);

  {
    CHECK(finalization_group->key_map()->IsObjectHashTable());
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 1, *weak_cell1);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key2), 0);
  }

  // Register another weak reference with a different key and verify internal
  // data structures.
  Handle<WeakCell> weak_cell2 = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key2, isolate);

  {
    CHECK(finalization_group->key_map()->IsObjectHashTable());
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 1, *weak_cell1);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key2), 1, *weak_cell2);
  }

  // Register another weak reference with key1 and verify internal data
  // structures.
  Handle<WeakCell> weak_cell3 = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key1, isolate);

  {
    CHECK(finalization_group->key_map()->IsObjectHashTable());
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 2, *weak_cell3,
                           *weak_cell1);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key2), 1, *weak_cell2);
  }
}

TEST(TestWeakCellNullify1) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<WeakCell> weak_cell1 =
      FinalizationGroupRegister(finalization_group, js_object, isolate);
  Handle<WeakCell> weak_cell2 =
      FinalizationGroupRegister(finalization_group, js_object, isolate);

  // Nullify the first WeakCell and verify internal data structures.
  NullifyWeakCell(weak_cell1, isolate);
  CHECK_EQ(finalization_group->active_cells(), *weak_cell2);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK(weak_cell2->next()->IsUndefined(isolate));
  CHECK_EQ(finalization_group->cleared_cells(), *weak_cell1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  // Nullify the second WeakCell and verify internal data structures.
  NullifyWeakCell(weak_cell2, isolate);
  CHECK(finalization_group->active_cells()->IsUndefined(isolate));
  CHECK_EQ(finalization_group->cleared_cells(), *weak_cell2);
  CHECK_EQ(weak_cell2->next(), *weak_cell1);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell1->prev(), *weak_cell2);
  CHECK(weak_cell1->next()->IsUndefined(isolate));
}

TEST(TestWeakCellNullify2) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<WeakCell> weak_cell1 =
      FinalizationGroupRegister(finalization_group, js_object, isolate);
  Handle<WeakCell> weak_cell2 =
      FinalizationGroupRegister(finalization_group, js_object, isolate);

  // Like TestWeakCellNullify1 but nullify the WeakCells in opposite order.
  NullifyWeakCell(weak_cell2, isolate);
  CHECK_EQ(finalization_group->active_cells(), *weak_cell1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));
  CHECK_EQ(finalization_group->cleared_cells(), *weak_cell2);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK(weak_cell2->next()->IsUndefined(isolate));

  NullifyWeakCell(weak_cell1, isolate);
  CHECK(finalization_group->active_cells()->IsUndefined(isolate));
  CHECK_EQ(finalization_group->cleared_cells(), *weak_cell1);
  CHECK_EQ(weak_cell1->next(), *weak_cell2);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell2->prev(), *weak_cell1);
  CHECK(weak_cell2->next()->IsUndefined(isolate));
}

TEST(TestJSFinalizationGroupPopClearedCellHoldings1) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());
  Handle<Object> undefined =
      handle(ReadOnlyRoots(isolate).undefined_value(), isolate);

  Handle<Object> holdings1 = factory->NewStringFromAsciiChecked("holdings1");
  Handle<WeakCell> weak_cell1 = FinalizationGroupRegister(
      finalization_group, js_object, holdings1, undefined, isolate);
  Handle<Object> holdings2 = factory->NewStringFromAsciiChecked("holdings2");
  Handle<WeakCell> weak_cell2 = FinalizationGroupRegister(
      finalization_group, js_object, holdings2, undefined, isolate);
  Handle<Object> holdings3 = factory->NewStringFromAsciiChecked("holdings3");
  Handle<WeakCell> weak_cell3 = FinalizationGroupRegister(
      finalization_group, js_object, holdings3, undefined, isolate);

  NullifyWeakCell(weak_cell2, isolate);
  NullifyWeakCell(weak_cell3, isolate);

  CHECK(finalization_group->NeedsCleanup());
  Object cleared1 =
      JSFinalizationGroup::PopClearedCellHoldings(finalization_group, isolate);
  CHECK_EQ(cleared1, *holdings3);
  CHECK(weak_cell3->prev()->IsUndefined(isolate));
  CHECK(weak_cell3->next()->IsUndefined(isolate));

  CHECK(finalization_group->NeedsCleanup());
  Object cleared2 =
      JSFinalizationGroup::PopClearedCellHoldings(finalization_group, isolate);
  CHECK_EQ(cleared2, *holdings2);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK(weak_cell2->next()->IsUndefined(isolate));

  CHECK(!finalization_group->NeedsCleanup());

  NullifyWeakCell(weak_cell1, isolate);

  CHECK(finalization_group->NeedsCleanup());
  Object cleared3 =
      JSFinalizationGroup::PopClearedCellHoldings(finalization_group, isolate);
  CHECK_EQ(cleared3, *holdings1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  CHECK(!finalization_group->NeedsCleanup());
  CHECK(finalization_group->active_cells()->IsUndefined(isolate));
  CHECK(finalization_group->cleared_cells()->IsUndefined(isolate));
}

TEST(TestJSFinalizationGroupPopClearedCellHoldings2) {
  // Test that when all WeakCells for a key are popped, the key is removed from
  // the key map.
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());
  Handle<JSObject> key1 = CreateKey("key1", isolate);

  Handle<Object> holdings1 = factory->NewStringFromAsciiChecked("holdings1");
  Handle<WeakCell> weak_cell1 = FinalizationGroupRegister(
      finalization_group, js_object, holdings1, key1, isolate);
  Handle<Object> holdings2 = factory->NewStringFromAsciiChecked("holdings2");
  Handle<WeakCell> weak_cell2 = FinalizationGroupRegister(
      finalization_group, js_object, holdings2, key1, isolate);

  NullifyWeakCell(weak_cell1, isolate);
  NullifyWeakCell(weak_cell2, isolate);

  // Nullifying doesn't affect the key chains (just moves WeakCells from
  // active_cells to cleared_cells).
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 2, *weak_cell2,
                           *weak_cell1);
  }

  Object cleared1 =
      JSFinalizationGroup::PopClearedCellHoldings(finalization_group, isolate);
  CHECK_EQ(cleared1, *holdings2);

  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 1, *weak_cell1);
  }

  Object cleared2 =
      JSFinalizationGroup::PopClearedCellHoldings(finalization_group, isolate);
  CHECK_EQ(cleared2, *holdings1);

  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 0);
  }
}

TEST(TestUnregisterActiveCells) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSObject> key1 = CreateKey("key1", isolate);
  Handle<JSObject> key2 = CreateKey("key2", isolate);
  Handle<Object> undefined =
      handle(ReadOnlyRoots(isolate).undefined_value(), isolate);

  Handle<WeakCell> weak_cell1a = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key1, isolate);
  Handle<WeakCell> weak_cell1b = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key1, isolate);

  Handle<WeakCell> weak_cell2a = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key2, isolate);
  Handle<WeakCell> weak_cell2b = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key2, isolate);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 4,
                      *weak_cell2b, *weak_cell2a, *weak_cell1b, *weak_cell1a);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 0);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 2, *weak_cell1b,
                           *weak_cell1a);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key2), 2, *weak_cell2b,
                           *weak_cell2a);
  }

  JSFinalizationGroup::Unregister(finalization_group, key1, isolate);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 0);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key2), 2, *weak_cell2b,
                           *weak_cell2a);
  }

  // Both weak_cell1a and weak_cell1b removed from active_cells.
  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 2,
                      *weak_cell2b, *weak_cell2a);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 0);
}

TEST(TestUnregisterActiveAndClearedCells) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSObject> key1 = CreateKey("key1", isolate);
  Handle<JSObject> key2 = CreateKey("key2", isolate);
  Handle<Object> undefined =
      handle(ReadOnlyRoots(isolate).undefined_value(), isolate);

  Handle<WeakCell> weak_cell1a = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key1, isolate);
  Handle<WeakCell> weak_cell1b = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key1, isolate);

  Handle<WeakCell> weak_cell2a = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key2, isolate);
  Handle<WeakCell> weak_cell2b = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key2, isolate);

  NullifyWeakCell(weak_cell2a, isolate);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 3,
                      *weak_cell2b, *weak_cell1b, *weak_cell1a);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 1,
                      *weak_cell2a);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 2, *weak_cell1b,
                           *weak_cell1a);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key2), 2, *weak_cell2b,
                           *weak_cell2a);
  }

  JSFinalizationGroup::Unregister(finalization_group, key2, isolate);

  // Both weak_cell2a and weak_cell2b removed.
  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 2,
                      *weak_cell1b, *weak_cell1a);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 0);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 2, *weak_cell1b,
                           *weak_cell1a);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key2), 0);
  }
}

TEST(TestWeakCellUnregisterTwice) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSObject> key1 = CreateKey("key1", isolate);
  Handle<Object> undefined =
      handle(ReadOnlyRoots(isolate).undefined_value(), isolate);

  Handle<WeakCell> weak_cell1 = FinalizationGroupRegister(
      finalization_group, js_object, undefined, key1, isolate);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 1,
                      *weak_cell1);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 0);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 1, *weak_cell1);
  }

  JSFinalizationGroup::Unregister(finalization_group, key1, isolate);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 0);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 0);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 0);
  }

  JSFinalizationGroup::Unregister(finalization_group, key1, isolate);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 0);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 0);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 0);
  }
}

TEST(TestWeakCellUnregisterPopped) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());
  Handle<JSObject> key1 = CreateKey("key1", isolate);
  Handle<Object> holdings1 = factory->NewStringFromAsciiChecked("holdings1");
  Handle<WeakCell> weak_cell1 = FinalizationGroupRegister(
      finalization_group, js_object, holdings1, key1, isolate);

  NullifyWeakCell(weak_cell1, isolate);

  CHECK(finalization_group->NeedsCleanup());
  Object cleared1 =
      JSFinalizationGroup::PopClearedCellHoldings(finalization_group, isolate);
  CHECK_EQ(cleared1, *holdings1);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 0);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 0);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 0);
  }

  JSFinalizationGroup::Unregister(finalization_group, key1, isolate);

  VerifyWeakCellChain(isolate, finalization_group->active_cells(), 0);
  VerifyWeakCellChain(isolate, finalization_group->cleared_cells(), 0);
  {
    Handle<ObjectHashTable> key_map =
        handle(ObjectHashTable::cast(finalization_group->key_map()), isolate);
    VerifyWeakCellKeyChain(isolate, key_map->Lookup(key1), 0);
  }
}

TEST(TestWeakCellUnregisterNonexistentKey) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group =
      ConstructJSFinalizationGroup(isolate);
  Handle<JSObject> key1 = CreateKey("key1", isolate);

  JSFinalizationGroup::Unregister(finalization_group, key1, isolate);
}

TEST(TestJSWeakRef) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakRef> weak_ref;
  {
    HandleScope inner_scope(isolate);

    Handle<JSObject> js_object =
        isolate->factory()->NewJSObject(isolate->object_function());
    // This doesn't add the target into the KeepDuringJob set.
    Handle<JSWeakRef> inner_weak_ref = ConstructJSWeakRef(js_object, isolate);

    CcTest::CollectAllGarbage();
    CHECK(!inner_weak_ref->target()->IsUndefined(isolate));

    weak_ref = inner_scope.CloseAndEscape(inner_weak_ref);
  }

  CHECK(!weak_ref->target()->IsUndefined(isolate));

  CcTest::CollectAllGarbage();

  CHECK(weak_ref->target()->IsUndefined(isolate));
}

TEST(TestJSWeakRefIncrementalMarking) {
  FLAG_harmony_weak_refs = true;
  if (!FLAG_incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext context;

  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);
  Handle<JSWeakRef> weak_ref;
  {
    HandleScope inner_scope(isolate);

    Handle<JSObject> js_object =
        isolate->factory()->NewJSObject(isolate->object_function());
    // This doesn't add the target into the KeepDuringJob set.
    Handle<JSWeakRef> inner_weak_ref = ConstructJSWeakRef(js_object, isolate);

    heap::SimulateIncrementalMarking(heap, true);
    CcTest::CollectAllGarbage();
    CHECK(!inner_weak_ref->target()->IsUndefined(isolate));

    weak_ref = inner_scope.CloseAndEscape(inner_weak_ref);
  }

  CHECK(!weak_ref->target()->IsUndefined(isolate));

  heap::SimulateIncrementalMarking(heap, true);
  CcTest::CollectAllGarbage();

  CHECK(weak_ref->target()->IsUndefined(isolate));
}

TEST(TestJSWeakRefKeepDuringJob) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;

  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);
  Handle<JSWeakRef> weak_ref;
  {
    HandleScope inner_scope(isolate);

    Handle<JSObject> js_object =
        isolate->factory()->NewJSObject(isolate->object_function());
    Handle<JSWeakRef> inner_weak_ref = ConstructJSWeakRef(js_object, isolate);
    heap->AddKeepDuringJobTarget(js_object);

    weak_ref = inner_scope.CloseAndEscape(inner_weak_ref);
  }

  CHECK(!weak_ref->target()->IsUndefined(isolate));

  CcTest::CollectAllGarbage();

  CHECK(!weak_ref->target()->IsUndefined(isolate));

  // Clears the KeepDuringJob set.
  isolate->default_microtask_queue()->RunMicrotasks(isolate);
  CcTest::CollectAllGarbage();

  CHECK(weak_ref->target()->IsUndefined(isolate));
}

TEST(TestJSWeakRefKeepDuringJobIncrementalMarking) {
  FLAG_harmony_weak_refs = true;
  if (!FLAG_incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext context;

  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);
  Handle<JSWeakRef> weak_ref;
  {
    HandleScope inner_scope(isolate);

    Handle<JSObject> js_object =
        isolate->factory()->NewJSObject(isolate->object_function());
    Handle<JSWeakRef> inner_weak_ref = ConstructJSWeakRef(js_object, isolate);
    heap->AddKeepDuringJobTarget(js_object);

    weak_ref = inner_scope.CloseAndEscape(inner_weak_ref);
  }

  CHECK(!weak_ref->target()->IsUndefined(isolate));

  heap::SimulateIncrementalMarking(heap, true);
  CcTest::CollectAllGarbage();

  CHECK(!weak_ref->target()->IsUndefined(isolate));

  // Clears the KeepDuringJob set.
  isolate->default_microtask_queue()->RunMicrotasks(isolate);
  heap::SimulateIncrementalMarking(heap, true);
  CcTest::CollectAllGarbage();

  CHECK(weak_ref->target()->IsUndefined(isolate));
}

}  // namespace internal
}  // namespace v8
