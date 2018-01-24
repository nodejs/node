// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/handler-configuration.h"

#include "src/code-stubs.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/transitions.h"

namespace v8 {
namespace internal {

namespace {

template <bool fill_array = true>
int InitPrototypeChecks(Isolate* isolate, Handle<Map> receiver_map,
                        Handle<JSReceiver> holder, Handle<Name> name,
                        Handle<FixedArray> array, int first_index) {
  if (!holder.is_null() && holder->map() == *receiver_map) return 0;

  HandleScope scope(isolate);
  int checks_count = 0;

  if (receiver_map->IsPrimitiveMap() || receiver_map->IsJSGlobalProxyMap()) {
    // The validity cell check for primitive and global proxy receivers does
    // not guarantee that certain native context ever had access to other
    // native context. However, a handler created for one native context could
    // be used in other native context through the megamorphic stub cache.
    // So we record the original native context to which this handler
    // corresponds.
    if (fill_array) {
      Handle<Context> native_context = isolate->native_context();
      array->set(first_index + checks_count, native_context->self_weak_cell());
    }
    checks_count++;

  } else if (receiver_map->IsJSGlobalObjectMap()) {
    // If we are creating a handler for [Load/Store]GlobalIC then we need to
    // check that the property did not appear in the global object.
    if (fill_array) {
      Handle<JSGlobalObject> global = isolate->global_object();
      Handle<PropertyCell> cell = JSGlobalObject::EnsureEmptyPropertyCell(
          global, name, PropertyCellType::kInvalidated);
      DCHECK(cell->value()->IsTheHole(isolate));
      Handle<WeakCell> weak_cell = isolate->factory()->NewWeakCell(cell);
      array->set(first_index + checks_count, *weak_cell);
    }
    checks_count++;
  }

  // Create/count entries for each global or dictionary prototype appeared in
  // the prototype chain contains from receiver till holder.
  PrototypeIterator::WhereToEnd end = name->IsPrivate()
                                          ? PrototypeIterator::END_AT_NON_HIDDEN
                                          : PrototypeIterator::END_AT_NULL;
  for (PrototypeIterator iter(receiver_map, end); !iter.IsAtEnd();
       iter.Advance()) {
    Handle<JSReceiver> current =
        PrototypeIterator::GetCurrent<JSReceiver>(iter);
    if (holder.is_identical_to(current)) break;
    Handle<Map> current_map(current->map(), isolate);

    if (current_map->IsJSGlobalObjectMap()) {
      if (fill_array) {
        Handle<JSGlobalObject> global = Handle<JSGlobalObject>::cast(current);
        Handle<PropertyCell> cell = JSGlobalObject::EnsureEmptyPropertyCell(
            global, name, PropertyCellType::kInvalidated);
        DCHECK(cell->value()->IsTheHole(isolate));
        Handle<WeakCell> weak_cell = isolate->factory()->NewWeakCell(cell);
        array->set(first_index + checks_count, *weak_cell);
      }
      checks_count++;

    } else if (current_map->is_dictionary_map()) {
      DCHECK(!current_map->IsJSGlobalProxyMap());  // Proxy maps are fast.
      if (fill_array) {
        DCHECK_EQ(NameDictionary::kNotFound,
                  current->property_dictionary()->FindEntry(name));
        Handle<WeakCell> weak_cell =
            Map::GetOrCreatePrototypeWeakCell(current, isolate);
        array->set(first_index + checks_count, *weak_cell);
      }
      checks_count++;
    }
  }
  return checks_count;
}

// Returns 0 if the validity cell check is enough to ensure that the
// prototype chain from |receiver_map| till |holder| did not change.
// If the |holder| is an empty handle then the full prototype chain is
// checked.
// Returns -1 if the handler has to be compiled or the number of prototype
// checks otherwise.
int GetPrototypeCheckCount(Isolate* isolate, Handle<Map> receiver_map,
                           Handle<JSReceiver> holder, Handle<Name> name) {
  return InitPrototypeChecks<false>(isolate, receiver_map, holder, name,
                                    Handle<FixedArray>(), 0);
}

}  // namespace

// static
Handle<Object> LoadHandler::LoadFromPrototype(Isolate* isolate,
                                              Handle<Map> receiver_map,
                                              Handle<JSReceiver> holder,
                                              Handle<Name> name,
                                              Handle<Smi> smi_handler,
                                              MaybeHandle<Object> maybe_data) {
  int checks_count =
      GetPrototypeCheckCount(isolate, receiver_map, holder, name);
  DCHECK_LE(0, checks_count);

  if (receiver_map->IsPrimitiveMap() ||
      receiver_map->is_access_check_needed()) {
    DCHECK(!receiver_map->is_dictionary_map());
    DCHECK_LE(1, checks_count);  // For native context.
    smi_handler = EnableAccessCheckOnReceiver(isolate, smi_handler);
  } else if (receiver_map->is_dictionary_map() &&
             !receiver_map->IsJSGlobalObjectMap()) {
    smi_handler = EnableLookupOnReceiver(isolate, smi_handler);
  }

  Handle<Cell> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  DCHECK(!validity_cell.is_null());

  Handle<Object> data;
  if (!maybe_data.ToHandle(&data)) {
    data = Map::GetOrCreatePrototypeWeakCell(holder, isolate);
  }

  if (checks_count == 0) {
    return isolate->factory()->NewTuple3(data, smi_handler, validity_cell,
                                         TENURED);
  }
  Handle<FixedArray> handler_array(isolate->factory()->NewFixedArray(
      kFirstPrototypeIndex + checks_count, TENURED));
  handler_array->set(kSmiHandlerIndex, *smi_handler);
  handler_array->set(kValidityCellIndex, *validity_cell);
  handler_array->set(kDataIndex, *data);
  InitPrototypeChecks(isolate, receiver_map, holder, name, handler_array,
                      kFirstPrototypeIndex);
  return handler_array;
}

// static
Handle<Object> LoadHandler::LoadFullChain(Isolate* isolate,
                                          Handle<Map> receiver_map,
                                          Handle<Object> holder,
                                          Handle<Name> name,
                                          Handle<Smi> smi_handler) {
  Handle<JSReceiver> end;  // null handle
  int checks_count = GetPrototypeCheckCount(isolate, receiver_map, end, name);
  DCHECK_LE(0, checks_count);

  if (receiver_map->IsPrimitiveMap() ||
      receiver_map->is_access_check_needed()) {
    DCHECK(!receiver_map->is_dictionary_map());
    DCHECK_LE(1, checks_count);  // For native context.
    smi_handler = EnableAccessCheckOnReceiver(isolate, smi_handler);
  } else if (receiver_map->is_dictionary_map() &&
             !receiver_map->IsJSGlobalObjectMap()) {
    smi_handler = EnableLookupOnReceiver(isolate, smi_handler);
  }

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  if (validity_cell.is_null()) {
    DCHECK_EQ(0, checks_count);
    // Lookup on receiver isn't supported in case of a simple smi handler.
    if (!LookupOnReceiverBits::decode(smi_handler->value())) return smi_handler;
    validity_cell = handle(Smi::kZero, isolate);
  }

  Factory* factory = isolate->factory();
  if (checks_count == 0) {
    return factory->NewTuple3(holder, smi_handler, validity_cell, TENURED);
  }
  Handle<FixedArray> handler_array(factory->NewFixedArray(
      LoadHandler::kFirstPrototypeIndex + checks_count, TENURED));
  handler_array->set(kSmiHandlerIndex, *smi_handler);
  handler_array->set(kValidityCellIndex, *validity_cell);
  handler_array->set(kDataIndex, *holder);
  InitPrototypeChecks(isolate, receiver_map, end, name, handler_array,
                      kFirstPrototypeIndex);
  return handler_array;
}

// static
KeyedAccessLoadMode LoadHandler::GetKeyedAccessLoadMode(Object* handler) {
  DisallowHeapAllocation no_gc;
  if (handler->IsSmi()) {
    int const raw_handler = Smi::cast(handler)->value();
    Kind const kind = KindBits::decode(raw_handler);
    if ((kind == kElement || kind == kIndexedString) &&
        AllowOutOfBoundsBits::decode(raw_handler)) {
      return LOAD_IGNORE_OUT_OF_BOUNDS;
    }
  }
  return STANDARD_LOAD;
}

// static
Handle<Object> StoreHandler::StoreElementTransition(
    Isolate* isolate, Handle<Map> receiver_map, Handle<Map> transition,
    KeyedAccessStoreMode store_mode) {
  bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
  ElementsKind elements_kind = receiver_map->elements_kind();
  Handle<Code> stub = ElementsTransitionAndStoreStub(
                          isolate, elements_kind, transition->elements_kind(),
                          is_js_array, store_mode)
                          .GetCode();
  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  if (validity_cell.is_null()) {
    validity_cell = handle(Smi::kZero, isolate);
  }
  Handle<WeakCell> cell = Map::WeakCellForMap(transition);
  return isolate->factory()->NewTuple3(cell, stub, validity_cell, TENURED);
}

Handle<Smi> StoreHandler::StoreTransition(Isolate* isolate,
                                          Handle<Map> transition_map) {
  int descriptor = transition_map->LastAdded();
  Handle<DescriptorArray> descriptors(transition_map->instance_descriptors());
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  DCHECK(!representation.IsNone());

  // Declarative handlers don't support access checks.
  DCHECK(!transition_map->is_access_check_needed());

  DCHECK_EQ(kData, details.kind());
  if (details.location() == PropertyLocation::kDescriptor) {
    return TransitionToConstant(isolate, descriptor);
  }
  DCHECK_EQ(PropertyLocation::kField, details.location());
  bool extend_storage =
      Map::cast(transition_map->GetBackPointer())->UnusedPropertyFields() == 0;

  FieldIndex index = FieldIndex::ForDescriptor(*transition_map, descriptor);
  return TransitionToField(isolate, descriptor, index, representation,
                           extend_storage);
}

// static
Handle<Object> StoreHandler::StoreThroughPrototype(
    Isolate* isolate, Handle<Map> receiver_map, Handle<JSReceiver> holder,
    Handle<Name> name, Handle<Smi> smi_handler,
    MaybeHandle<Object> maybe_data) {
  int checks_count =
      GetPrototypeCheckCount(isolate, receiver_map, holder, name);

  DCHECK_LE(0, checks_count);

  if (receiver_map->is_access_check_needed()) {
    DCHECK(!receiver_map->is_dictionary_map());
    DCHECK_LE(1, checks_count);  // For native context.
    smi_handler = EnableAccessCheckOnReceiver(isolate, smi_handler);
  }

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  if (validity_cell.is_null()) {
    DCHECK_EQ(0, checks_count);
    validity_cell = handle(Smi::kZero, isolate);
  }

  Handle<Object> data;
  if (!maybe_data.ToHandle(&data)) {
    data = Map::GetOrCreatePrototypeWeakCell(holder, isolate);
  }

  Factory* factory = isolate->factory();
  if (checks_count == 0) {
    return factory->NewTuple3(data, smi_handler, validity_cell, TENURED);
  }
  Handle<FixedArray> handler_array(
      factory->NewFixedArray(kFirstPrototypeIndex + checks_count, TENURED));
  handler_array->set(kSmiHandlerIndex, *smi_handler);
  handler_array->set(kValidityCellIndex, *validity_cell);
  handler_array->set(kDataIndex, *data);
  InitPrototypeChecks(isolate, receiver_map, holder, name, handler_array,
                      kFirstPrototypeIndex);
  return handler_array;
}

// static
Handle<Object> StoreHandler::StoreGlobal(Isolate* isolate,
                                         Handle<PropertyCell> cell) {
  return isolate->factory()->NewWeakCell(cell);
}

// static
Handle<Object> StoreHandler::StoreProxy(Isolate* isolate,
                                        Handle<Map> receiver_map,
                                        Handle<JSProxy> proxy,
                                        Handle<JSReceiver> receiver,
                                        Handle<Name> name) {
  Handle<Smi> smi_handler = StoreProxy(isolate);
  if (receiver.is_identical_to(proxy)) return smi_handler;
  Handle<WeakCell> holder_cell = isolate->factory()->NewWeakCell(proxy);
  return StoreThroughPrototype(isolate, receiver_map, proxy, name, smi_handler,
                               holder_cell);
}

Object* StoreHandler::ValidHandlerOrNull(Object* raw_handler, Name* name,
                                         Handle<Map>* out_transition) {
  STATIC_ASSERT(kValidityCellOffset == Tuple3::kValue3Offset);

  Smi* valid = Smi::FromInt(Map::kPrototypeChainValid);

  if (raw_handler->IsTuple3()) {
    // Check validity cell.
    Tuple3* handler = Tuple3::cast(raw_handler);

    Object* raw_validity_cell = handler->value3();
    // |raw_valitity_cell| can be Smi::kZero if no validity cell is required
    // (which counts as valid).
    if (raw_validity_cell->IsCell() &&
        Cell::cast(raw_validity_cell)->value() != valid) {
      return nullptr;
    }

  } else {
    DCHECK(raw_handler->IsFixedArrayExact());
    FixedArray* handler = FixedArray::cast(raw_handler);

    // Check validity cell.
    Object* value = Cell::cast(handler->get(kValidityCellIndex))->value();
    if (value != valid) return nullptr;

    // Check prototypes.
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
        if (number != NameDictionary::kNotFound) {
          PropertyDetails details = dict->DetailsAt(number);
          if (details.IsReadOnly()) return nullptr;
          if (details.kind() == PropertyKind::kAccessor) return nullptr;
          break;
        }
      }
    }
  }

  // Check if the transition target is deprecated.
  WeakCell* target_cell = GetTransitionCell(raw_handler);
  Map* transition = Map::cast(target_cell->value());
  if (transition->is_deprecated()) return nullptr;
  *out_transition = handle(transition);
  return raw_handler;
}

}  // namespace internal
}  // namespace v8
