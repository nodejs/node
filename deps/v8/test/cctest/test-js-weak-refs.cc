// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles-inl.h"
#include "src/heap/factory-inl.h"
#include "src/isolate.h"
#include "test/cctest/cctest.h"

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
  auto empty_func = [](HeapObject* object, Object** slot, Object* target) {};
  weak_cell->Nullify(isolate, empty_func);
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
  JSWeakCell* cleared1 = weak_factory->PopClearedCell(isolate);
  CHECK_EQ(cleared1, *weak_cell3);
  CHECK(weak_cell3->prev()->IsUndefined(isolate));
  CHECK(weak_cell3->next()->IsUndefined(isolate));

  CHECK(weak_factory->NeedsCleanup());
  JSWeakCell* cleared2 = weak_factory->PopClearedCell(isolate);
  CHECK_EQ(cleared2, *weak_cell2);
  CHECK(weak_cell2->prev()->IsUndefined(isolate));
  CHECK(weak_cell2->next()->IsUndefined(isolate));

  CHECK(!weak_factory->NeedsCleanup());

  NullifyWeakCell(weak_cell1, isolate);

  CHECK(weak_factory->NeedsCleanup());
  JSWeakCell* cleared3 = weak_factory->PopClearedCell(isolate);
  CHECK_EQ(cleared3, *weak_cell1);
  CHECK(weak_cell1->prev()->IsUndefined(isolate));
  CHECK(weak_cell1->next()->IsUndefined(isolate));

  CHECK(!weak_factory->NeedsCleanup());
  CHECK(weak_factory->active_cells()->IsUndefined(isolate));
  CHECK(weak_factory->cleared_cells()->IsUndefined(isolate));
}

}  // namespace internal
}  // namespace v8
