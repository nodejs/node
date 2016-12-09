// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/ic-compiler.h"

#include "src/ic/handler-compiler.h"
#include "src/ic/ic-inl.h"

namespace v8 {
namespace internal {

Handle<Code> PropertyICCompiler::ComputeKeyedStoreMonomorphicHandler(
    Handle<Map> receiver_map, KeyedAccessStoreMode store_mode) {
  Isolate* isolate = receiver_map->GetIsolate();

  DCHECK(store_mode == STANDARD_STORE ||
         store_mode == STORE_AND_GROW_NO_TRANSITION ||
         store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
         store_mode == STORE_NO_TRANSITION_HANDLE_COW);

  PropertyICCompiler compiler(isolate);
  Handle<Code> code =
      compiler.CompileKeyedStoreMonomorphicHandler(receiver_map, store_mode);
  return code;
}

void PropertyICCompiler::ComputeKeyedStorePolymorphicHandlers(
    MapHandleList* receiver_maps, MapHandleList* transitioned_maps,
    CodeHandleList* handlers, KeyedAccessStoreMode store_mode) {
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
    CodeHandleList* handlers, KeyedAccessStoreMode store_mode) {
  for (int i = 0; i < receiver_maps->length(); ++i) {
    Handle<Map> receiver_map(receiver_maps->at(i));
    Handle<Code> cached_stub;
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
    bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
    ElementsKind elements_kind = receiver_map->elements_kind();
    if (!transitioned_map.is_null()) {
      cached_stub =
          ElementsTransitionAndStoreStub(isolate(), elements_kind,
                                         transitioned_map->elements_kind(),
                                         is_js_array, store_mode).GetCode();
    } else if (receiver_map->instance_type() < FIRST_JS_RECEIVER_TYPE) {
      // TODO(mvstanton): Consider embedding store_mode in the state of the slow
      // keyed store ic for uniformity.
      cached_stub = isolate()->builtins()->KeyedStoreIC_Slow();
    } else {
      if (IsSloppyArgumentsElements(elements_kind)) {
        cached_stub =
            KeyedStoreSloppyArgumentsStub(isolate(), store_mode).GetCode();
      } else if (receiver_map->has_fast_elements() ||
                 receiver_map->has_fixed_typed_array_elements()) {
        cached_stub = StoreFastElementStub(isolate(), is_js_array,
                                           elements_kind, store_mode).GetCode();
      } else {
        cached_stub =
            StoreElementStub(isolate(), elements_kind, store_mode).GetCode();
      }
    }
    DCHECK(!cached_stub.is_null());
    handlers->Add(cached_stub);
    transitioned_maps->Add(transitioned_map);
  }
}


#define __ ACCESS_MASM(masm())


Handle<Code> PropertyICCompiler::CompileKeyedStoreMonomorphicHandler(
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
  return stub;
}


#undef __
}  // namespace internal
}  // namespace v8
