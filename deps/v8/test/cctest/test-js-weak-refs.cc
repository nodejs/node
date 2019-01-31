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

Handle<JSWeakFactory> ConstructJSWeakFactory(Isolate* isolate) {
  Factory* factory = isolate->factory();
  Handle<String> weak_factory_name = factory->WeakFactory_string();
  Handle<Object> global =
      handle(isolate->native_context()->global_object(), isolate);
  Handle<JSFunction> weak_factory_fun = Handle<JSFunction>::cast(
      Object::GetProperty(isolate, global, weak_factory_name)
          .ToHandleChecked());
  auto weak_factory = Handle<JSWeakFactory>::cast(
      JSObject::New(weak_factory_fun, weak_factory_fun,
                    Handle<AllocationSite>::null())
          .ToHandleChecked());
#ifdef VERIFY_HEAP
  weak_factory->JSWeakFactoryVerify(isolate);
#endif  // VERIFY_HEAP
  return weak_factory;
}

Handle<JSWeakRef> ConstructJSWeakRef(Isolate* isolate,
                                     Handle<JSReceiver> target) {
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

Handle<JSWeakCell> MakeCell(Isolate* isolate, Handle<JSObject> js_object,
                            Handle<JSWeakFactory> weak_factory) {
  Handle<Map> weak_cell_map(isolate->native_context()->js_weak_cell_map(),
                            isolate);
  Handle<JSWeakCell> weak_cell =
      Handle<JSWeakCell>::cast(isolate->factory()->NewJSObjectFromMap(
          weak_cell_map, TENURED, Handle<AllocationSite>::null()));
  weak_cell->set_target(*js_object);
  weak_factory->AddWeakCell(*weak_cell);
#ifdef VERIFY_HEAP
  weak_cell->JSWeakCellVerify(isolate);
#endif  // VERIFY_HEAP
  return weak_cell;
}

void NullifyWeakCell(Handle<JSWeakCell> weak_cell, Isolate* isolate) {
  auto empty_func = [](HeapObject object, ObjectSlot slot, Object target) {};
  weak_cell->Nullify(isolate, empty_func);
#ifdef VERIFY_HEAP
  weak_cell->JSWeakCellVerify(isolate);
#endif  // VERIFY_HEAP
}

void ClearWeakCell(Handle<JSWeakCell> weak_cell, Isolate* isolate) {
  weak_cell->Clear(isolate);
  CHECK(weak_cell->next()->IsUndefined(isolate));
  CHECK(weak_cell->prev()->IsUndefined(isolate));
#ifdef VERIFY_HEAP
  weak_cell->JSWeakCellVerify(isolate);
#endif  // VERIFY_HEAP
}

TEST(TestJSWeakCellCreation) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakFactory> weak_factory = ConstructJSWeakFactory(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Create JSWeakCell and verify internal data structures.
  Handle<JSWeakCell> weak_cell1 = MakeCell(isolate, js_object, weak_factory);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  CHECK_EQ(weak_factory->active_cells(), *weak_cell1);
  CHECK(weak_factory->cleared_cells()->IsUndefined(isolate));

  // Create another JSWeakCell and verify internal data structures.
  Handle<JSWeakCell> weak_cell2 = MakeCell(isolate, js_object, weak_factory);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell2->next(), *weak_cell1);
  CHECK_EQ(weak_cell1->prev(), *weak_cell2);
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  CHECK_EQ(weak_factory->active_cells(), *weak_cell2);
  CHECK(weak_factory->cleared_cells()->IsUndefined(isolate));
}

TEST(TestJSWeakCellNullify1) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakFactory> weak_factory = ConstructJSWeakFactory(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSWeakCell> weak_cell1 = MakeCell(isolate, js_object, weak_factory);
  Handle<JSWeakCell> weak_cell2 = MakeCell(isolate, js_object, weak_factory);

  // Nullify the first JSWeakCell and verify internal data structures.
  NullifyWeakCell(weak_cell1, isolate);
  CHECK_EQ(weak_factory->active_cells(), *weak_cell2);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK(weak_cell2->next()->IsUndefined(isolate));
  CHECK_EQ(weak_factory->cleared_cells(), *weak_cell1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  // Nullify the second JSWeakCell and verify internal data structures.
  NullifyWeakCell(weak_cell2, isolate);
  CHECK(weak_factory->active_cells()->IsUndefined(isolate));
  CHECK_EQ(weak_factory->cleared_cells(), *weak_cell2);
  CHECK_EQ(weak_cell2->next(), *weak_cell1);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell1->prev(), *weak_cell2);
  CHECK(weak_cell1->next()->IsUndefined(isolate));
}

TEST(TestJSWeakCellNullify2) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakFactory> weak_factory = ConstructJSWeakFactory(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSWeakCell> weak_cell1 = MakeCell(isolate, js_object, weak_factory);
  Handle<JSWeakCell> weak_cell2 = MakeCell(isolate, js_object, weak_factory);

  // Like TestJSWeakCellNullify1 but clear the JSWeakCells in opposite order.
  NullifyWeakCell(weak_cell2, isolate);
  CHECK_EQ(weak_factory->active_cells(), *weak_cell1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));
  CHECK_EQ(weak_factory->cleared_cells(), *weak_cell2);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK(weak_cell2->next()->IsUndefined(isolate));

  NullifyWeakCell(weak_cell1, isolate);
  CHECK(weak_factory->active_cells()->IsUndefined(isolate));
  CHECK_EQ(weak_factory->cleared_cells(), *weak_cell1);
  CHECK_EQ(weak_cell1->next(), *weak_cell2);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell2->prev(), *weak_cell1);
  CHECK(weak_cell2->next()->IsUndefined(isolate));
}

TEST(TestJSWeakFactoryPopClearedCell) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakFactory> weak_factory = ConstructJSWeakFactory(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSWeakCell> weak_cell1 = MakeCell(isolate, js_object, weak_factory);
  Handle<JSWeakCell> weak_cell2 = MakeCell(isolate, js_object, weak_factory);
  Handle<JSWeakCell> weak_cell3 = MakeCell(isolate, js_object, weak_factory);

  NullifyWeakCell(weak_cell2, isolate);
  NullifyWeakCell(weak_cell3, isolate);

  CHECK(weak_factory->NeedsCleanup());
  JSWeakCell cleared1 = weak_factory->PopClearedCell(isolate);
  CHECK_EQ(cleared1, *weak_cell3);
  CHECK(weak_cell3->prev()->IsUndefined(isolate));
  CHECK(weak_cell3->next()->IsUndefined(isolate));

  CHECK(weak_factory->NeedsCleanup());
  JSWeakCell cleared2 = weak_factory->PopClearedCell(isolate);
  CHECK_EQ(cleared2, *weak_cell2);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK(weak_cell2->next()->IsUndefined(isolate));

  CHECK(!weak_factory->NeedsCleanup());

  NullifyWeakCell(weak_cell1, isolate);

  CHECK(weak_factory->NeedsCleanup());
  JSWeakCell cleared3 = weak_factory->PopClearedCell(isolate);
  CHECK_EQ(cleared3, *weak_cell1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  CHECK(!weak_factory->NeedsCleanup());
  CHECK(weak_factory->active_cells()->IsUndefined(isolate));
  CHECK(weak_factory->cleared_cells()->IsUndefined(isolate));
}

TEST(TestJSWeakCellClearActiveCells) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakFactory> weak_factory = ConstructJSWeakFactory(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSWeakCell> weak_cell1 = MakeCell(isolate, js_object, weak_factory);
  Handle<JSWeakCell> weak_cell2 = MakeCell(isolate, js_object, weak_factory);
  Handle<JSWeakCell> weak_cell3 = MakeCell(isolate, js_object, weak_factory);

  CHECK_EQ(weak_factory->active_cells(), *weak_cell3);
  CHECK(weak_cell3->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell3->next(), *weak_cell2);
  CHECK_EQ(weak_cell2->prev(), *weak_cell3);
  CHECK_EQ(weak_cell2->next(), *weak_cell1);
  CHECK_EQ(weak_cell1->prev(), *weak_cell2);
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  // Clear all JSWeakCells in active_cells and verify the consistency of the
  // active_cells list in all stages.
  ClearWeakCell(weak_cell2, isolate);
  CHECK_EQ(weak_factory->active_cells(), *weak_cell3);
  CHECK(weak_cell3->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell3->next(), *weak_cell1);
  CHECK_EQ(weak_cell1->prev(), *weak_cell3);
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  ClearWeakCell(weak_cell3, isolate);
  CHECK_EQ(weak_factory->active_cells(), *weak_cell1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  ClearWeakCell(weak_cell1, isolate);
  CHECK(weak_factory->active_cells()->IsUndefined(isolate));
}

TEST(TestJSWeakCellClearClearedCells) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakFactory> weak_factory = ConstructJSWeakFactory(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSWeakCell> weak_cell1 = MakeCell(isolate, js_object, weak_factory);
  Handle<JSWeakCell> weak_cell2 = MakeCell(isolate, js_object, weak_factory);
  Handle<JSWeakCell> weak_cell3 = MakeCell(isolate, js_object, weak_factory);

  NullifyWeakCell(weak_cell1, isolate);
  NullifyWeakCell(weak_cell2, isolate);
  NullifyWeakCell(weak_cell3, isolate);

  CHECK_EQ(weak_factory->cleared_cells(), *weak_cell3);
  CHECK(weak_cell3->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell3->next(), *weak_cell2);
  CHECK_EQ(weak_cell2->prev(), *weak_cell3);
  CHECK_EQ(weak_cell2->next(), *weak_cell1);
  CHECK_EQ(weak_cell1->prev(), *weak_cell2);
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  // Clear all JSWeakCells in cleared_cells and verify the consistency of the
  // cleared_cells list in all stages.
  ClearWeakCell(weak_cell2, isolate);
  CHECK_EQ(weak_factory->cleared_cells(), *weak_cell3);
  CHECK(weak_cell3->prev()->IsUndefined(isolate));
  CHECK_EQ(weak_cell3->next(), *weak_cell1);
  CHECK_EQ(weak_cell1->prev(), *weak_cell3);
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  ClearWeakCell(weak_cell3, isolate);
  CHECK_EQ(weak_factory->cleared_cells(), *weak_cell1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  ClearWeakCell(weak_cell1, isolate);
  CHECK(weak_factory->cleared_cells()->IsUndefined(isolate));
}

TEST(TestJSWeakCellClearTwice) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakFactory> weak_factory = ConstructJSWeakFactory(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSWeakCell> weak_cell1 = MakeCell(isolate, js_object, weak_factory);

  ClearWeakCell(weak_cell1, isolate);
  ClearWeakCell(weak_cell1, isolate);
}

TEST(TestJSWeakCellClearPopped) {
  FLAG_harmony_weak_refs = true;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope outer_scope(isolate);
  Handle<JSWeakFactory> weak_factory = ConstructJSWeakFactory(isolate);
  Handle<JSObject> js_object =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<JSWeakCell> weak_cell1 = MakeCell(isolate, js_object, weak_factory);
  NullifyWeakCell(weak_cell1, isolate);
  JSWeakCell cleared1 = weak_factory->PopClearedCell(isolate);
  CHECK_EQ(cleared1, *weak_cell1);

  ClearWeakCell(weak_cell1, isolate);
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
    Handle<JSWeakRef> inner_weak_ref = ConstructJSWeakRef(isolate, js_object);

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
    Handle<JSWeakRef> inner_weak_ref = ConstructJSWeakRef(isolate, js_object);

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
    Handle<JSWeakRef> inner_weak_ref = ConstructJSWeakRef(isolate, js_object);
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
    Handle<JSWeakRef> inner_weak_ref = ConstructJSWeakRef(isolate, js_object);
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
