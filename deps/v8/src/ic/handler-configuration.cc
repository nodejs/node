// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/handler-configuration.h"

#include "src/ic/handler-configuration-inl.h"
#include "src/transitions.h"

namespace v8 {
namespace internal {

// |name| can be nullptr if no name/details check needs to be performed.
Object* StoreHandler::ValidTuple3HandlerOrNull(Object* handler, Name* name,
                                               Handle<Map>* out_transition) {
  DCHECK(handler->IsTuple3());
  // Step 1: Check validity cell.
  STATIC_ASSERT(kValidityCellOffset == Tuple3::kValue3Offset);
  Object* raw_validity_cell = Tuple3::cast(handler)->value3();
  Smi* valid = Smi::FromInt(Map::kPrototypeChainValid);
  // |raw_valitity_cell| can be Smi::kZero if no validity cell is required
  // (which counts as valid).
  if (raw_validity_cell->IsCell() &&
      Cell::cast(raw_validity_cell)->value() != valid) {
    return nullptr;
  }
  // Step 2 (optional): Check transition key.
  WeakCell* target_cell = StoreHandler::GetTuple3TransitionCell(handler);
  if (name != nullptr) {
    if (!TransitionsAccessor::IsMatchingMap(target_cell, name, kData, NONE)) {
      return nullptr;
    }
  }
  // Step 3: Check if the transition target is deprecated.
  Map* transition = Map::cast(target_cell->value());
  if (transition->is_deprecated()) return nullptr;
  *out_transition = handle(transition);
  return handler;
}

Object* StoreHandler::ValidFixedArrayHandlerOrNull(
    Object* raw_handler, Name* name, Handle<Map>* out_transition) {
  DCHECK(raw_handler->IsFixedArray());
  FixedArray* handler = FixedArray::cast(raw_handler);
  // Step 1: Check validity cell.
  Object* value = Cell::cast(handler->get(kValidityCellIndex))->value();
  if (value != Smi::FromInt(Map::kPrototypeChainValid)) return nullptr;
  // Step 2: Check transition key.
  WeakCell* target_cell = StoreHandler::GetArrayTransitionCell(handler);
  if (!TransitionsAccessor::IsMatchingMap(target_cell, name, kData, NONE)) {
    return nullptr;
  }
  // Step 3: Check prototypes.
  Heap* heap = handler->GetHeap();
  Isolate* isolate = heap->isolate();
  Handle<Name> name_handle(name, isolate);
  for (int i = kFirstPrototypeIndex; i < handler->length(); i++) {
    // This mirrors AccessorAssembler::CheckPrototype.
    WeakCell* prototype_cell = WeakCell::cast(handler->get(i));
    if (prototype_cell->cleared()) return nullptr;
    HeapObject* maybe_prototype = HeapObject::cast(prototype_cell->value());
    if (maybe_prototype->IsPropertyCell()) {
      Object* value = PropertyCell::cast(maybe_prototype)->value();
      if (value != heap->the_hole_value()) return nullptr;
    } else {
      DCHECK(maybe_prototype->map()->is_dictionary_map());
      // Do a negative dictionary lookup.
      NameDictionary* dict =
          JSObject::cast(maybe_prototype)->property_dictionary();
      int number = dict->FindEntry(isolate, name_handle);
      if (number != NameDictionary::kNotFound) return nullptr;
    }
  }
  // Step 4: Check if the transition target is deprecated.
  Map* transition = Map::cast(target_cell->value());
  if (transition->is_deprecated()) return nullptr;
  *out_transition = handle(transition);
  return handler;
}

}  // namespace internal
}  // namespace v8
