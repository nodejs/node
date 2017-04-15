// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/ic-compiler.h"

#include "src/ic/handler-compiler.h"
#include "src/ic/ic-inl.h"

namespace v8 {
namespace internal {

Handle<Object> PropertyICCompiler::ComputeKeyedStoreMonomorphicHandler(
    Handle<Map> receiver_map, KeyedAccessStoreMode store_mode) {
  Isolate* isolate = receiver_map->GetIsolate();

  DCHECK(store_mode == STANDARD_STORE ||
         store_mode == STORE_AND_GROW_NO_TRANSITION ||
         store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
         store_mode == STORE_NO_TRANSITION_HANDLE_COW);

  PropertyICCompiler compiler(isolate);
  Handle<Object> handler =
      compiler.CompileKeyedStoreMonomorphicHandler(receiver_map, store_mode);
  return handler;
}

void PropertyICCompiler::ComputeKeyedStorePolymorphicHandlers(
    MapHandleList* receiver_maps, MapHandleList* transitioned_maps,
    List<Handle<Object>>* handlers, KeyedAccessStoreMode store_mode) {
  Isolate* isolate = receiver_maps->at(0)->GetIsolate();
  DCHECK(store_mode == STANDARD_STORE ||
         store_mode == STORE_AND_GROW_NO_TRANSITION ||
         store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
         store_mode == STORE_NO_TRANSITION_HANDLE_COW);
  PropertyICCompiler compiler(isolate);
  compiler.CompileKeyedStorePolymorphicHandlers(
      receiver_maps, transitioned_maps, handlers, store_mode);
}

void PropertyICCompiler::CompileKeyedStorePolymorphicHandlers(
    MapHandleList* receiver_maps, MapHandleList* transitioned_maps,
    List<Handle<Object>>* handlers, KeyedAccessStoreMode store_mode) {
  for (int i = 0; i < receiver_maps->length(); ++i) {
    Handle<Map> receiver_map(receiver_maps->at(i));
    Handle<Object> handler;
    Handle<Map> transitioned_map;
    {
      Map* tmap = receiver_map->FindElementsKindTransitionedMap(receiver_maps);
      if (tmap != nullptr) transitioned_map = handle(tmap);
    }

    // TODO(mvstanton): The code below is doing pessimistic elements
    // transitions. I would like to stop doing that and rely on Allocation Site
    // Tracking to do a better job of ensuring the data types are what they need
    // to be. Not all the elements are in place yet, pessimistic elements
    // transitions are still important for performance.
    if (!transitioned_map.is_null()) {
      bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
      ElementsKind elements_kind = receiver_map->elements_kind();
      TRACE_HANDLER_STATS(isolate(),
                          KeyedStoreIC_ElementsTransitionAndStoreStub);
      Handle<Code> stub =
          ElementsTransitionAndStoreStub(isolate(), elements_kind,
                                         transitioned_map->elements_kind(),
                                         is_js_array, store_mode)
              .GetCode();
      Handle<Object> validity_cell =
          Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
      if (validity_cell.is_null()) {
        handler = stub;
      } else {
        handler = isolate()->factory()->NewTuple2(validity_cell, stub);
      }

    } else if (receiver_map->instance_type() < FIRST_JS_RECEIVER_TYPE) {
      // TODO(mvstanton): Consider embedding store_mode in the state of the slow
      // keyed store ic for uniformity.
      TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_SlowStub);
      handler = isolate()->builtins()->KeyedStoreIC_Slow();
    } else {
      handler = CompileKeyedStoreMonomorphicHandler(receiver_map, store_mode);
    }
    DCHECK(!handler.is_null());
    handlers->Add(handler);
    transitioned_maps->Add(transitioned_map);
  }
}


#define __ ACCESS_MASM(masm())

Handle<Object> PropertyICCompiler::CompileKeyedStoreMonomorphicHandler(
    Handle<Map> receiver_map, KeyedAccessStoreMode store_mode) {
  ElementsKind elements_kind = receiver_map->elements_kind();
  bool is_jsarray = receiver_map->instance_type() == JS_ARRAY_TYPE;
  Handle<Code> stub;
  if (receiver_map->has_sloppy_arguments_elements()) {
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_KeyedStoreSloppyArgumentsStub);
    stub = KeyedStoreSloppyArgumentsStub(isolate(), store_mode).GetCode();
  } else if (receiver_map->has_fast_elements() ||
             receiver_map->has_fixed_typed_array_elements()) {
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_StoreFastElementStub);
    stub = StoreFastElementStub(isolate(), is_jsarray, elements_kind,
                                store_mode).GetCode();
  } else {
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_StoreElementStub);
    stub = StoreElementStub(isolate(), elements_kind, store_mode).GetCode();
  }
  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
  if (validity_cell.is_null()) {
    return stub;
  }
  return isolate()->factory()->NewTuple2(validity_cell, stub);
}


#undef __
}  // namespace internal
}  // namespace v8
