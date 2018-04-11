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

template <typename BitField>
Handle<Smi> SetBitFieldValue(Isolate* isolate, Handle<Smi> smi_handler,
                             typename BitField::FieldType value) {
  int config = smi_handler->value();
  config = BitField::update(config, true);
  return handle(Smi::FromInt(config), isolate);
}

// TODO(ishell): Remove templatezation once we move common bits from
// Load/StoreHandler to the base class.
template <typename ICHandler, bool fill_handler = true>
int InitPrototypeChecksImpl(Isolate* isolate, Handle<ICHandler> handler,
                            Handle<Smi>* smi_handler, Handle<Map> receiver_map,
                            Handle<JSReceiver> holder, Handle<Object> data1,
                            MaybeHandle<Object> maybe_data2) {
  int checks_count = 0;
  // Holder-is-receiver case itself does not add entries unless there is an
  // optional data2 value provided.

  if (receiver_map->IsPrimitiveMap() ||
      receiver_map->is_access_check_needed()) {
    DCHECK(!receiver_map->IsJSGlobalObjectMap());
    // The validity cell check for primitive and global proxy receivers does
    // not guarantee that certain native context ever had access to other
    // native context. However, a handler created for one native context could
    // be used in other native context through the megamorphic stub cache.
    // So we record the original native context to which this handler
    // corresponds.
    if (fill_handler) {
      Handle<Context> native_context = isolate->native_context();
      handler->set_data2(native_context->self_weak_cell());
    } else {
      // Enable access checks on receiver.
      typedef typename ICHandler::DoAccessCheckOnReceiverBits Bit;
      *smi_handler = SetBitFieldValue<Bit>(isolate, *smi_handler, true);
    }
    checks_count++;
  } else if (receiver_map->is_dictionary_map() &&
             !receiver_map->IsJSGlobalObjectMap()) {
    if (!fill_handler) {
      // Enable lookup on receiver.
      typedef typename ICHandler::LookupOnReceiverBits Bit;
      *smi_handler = SetBitFieldValue<Bit>(isolate, *smi_handler, true);
    }
  }
  if (fill_handler) {
    handler->set_data1(*data1);
  }
  Handle<Object> data2;
  if (maybe_data2.ToHandle(&data2)) {
    if (fill_handler) {
      // This value will go either to data2 or data3 slot depending on whether
      // data2 slot is already occupied by native context.
      if (checks_count == 0) {
        handler->set_data2(*data2);
      } else {
        DCHECK_EQ(1, checks_count);
        handler->set_data3(*data2);
      }
    }
    checks_count++;
  }
  return checks_count;
}

// Returns 0 if the validity cell check is enough to ensure that the
// prototype chain from |receiver_map| till |holder| did not change.
// If the |holder| is an empty handle then the full prototype chain is
// checked.
// Returns -1 if the handler has to be compiled or the number of prototype
// checks otherwise.
template <typename ICHandler>
int GetPrototypeCheckCount(
    Isolate* isolate, Handle<Smi>* smi_handler, Handle<Map> receiver_map,
    Handle<JSReceiver> holder, Handle<Object> data1,
    MaybeHandle<Object> maybe_data2 = MaybeHandle<Object>()) {
  DCHECK_NOT_NULL(smi_handler);
  return InitPrototypeChecksImpl<ICHandler, false>(isolate, Handle<ICHandler>(),
                                                   smi_handler, receiver_map,
                                                   holder, data1, maybe_data2);
}

template <typename ICHandler>
void InitPrototypeChecks(
    Isolate* isolate, Handle<ICHandler> handler, Handle<Map> receiver_map,
    Handle<JSReceiver> holder, Handle<Object> data1,
    MaybeHandle<Object> maybe_data2 = MaybeHandle<Object>()) {
  InitPrototypeChecksImpl<ICHandler, true>(
      isolate, handler, nullptr, receiver_map, holder, data1, maybe_data2);
}

}  // namespace

// static
Handle<Object> LoadHandler::LoadFromPrototype(Isolate* isolate,
                                              Handle<Map> receiver_map,
                                              Handle<JSReceiver> holder,
                                              Handle<Smi> smi_handler,
                                              MaybeHandle<Object> maybe_data1,
                                              MaybeHandle<Object> maybe_data2) {
  Handle<Object> data1;
  if (!maybe_data1.ToHandle(&data1)) {
    data1 = Map::GetOrCreatePrototypeWeakCell(holder, isolate);
  }

  int checks_count = GetPrototypeCheckCount<LoadHandler>(
      isolate, &smi_handler, receiver_map, holder, data1, maybe_data2);

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  if (validity_cell.is_null()) {
    // Although in case of kApiGetter we load from receiver we still have to
    // use the "prototype" shape of a handler in order to provide additional
    // data to the dispatcher.
    DCHECK_EQ(kApiGetter, GetHandlerKind(*smi_handler));
    validity_cell = handle(Smi::kZero, isolate);
  }

  int data_count = 1 + checks_count;
  Handle<LoadHandler> handler = isolate->factory()->NewLoadHandler(data_count);

  handler->set_smi_handler(*smi_handler);
  handler->set_validity_cell(*validity_cell);
  InitPrototypeChecks(isolate, handler, receiver_map, holder, data1,
                      maybe_data2);
  return handler;
}

// static
Handle<Object> LoadHandler::LoadFullChain(Isolate* isolate,
                                          Handle<Map> receiver_map,
                                          Handle<Object> holder,
                                          Handle<Smi> smi_handler) {
  Handle<JSReceiver> end;  // null handle, means full prototype chain lookup.
  Handle<Object> data1 = holder;
  int checks_count = GetPrototypeCheckCount<LoadHandler>(
      isolate, &smi_handler, receiver_map, end, data1);

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  if (validity_cell.is_null()) {
    DCHECK_EQ(0, checks_count);
    // Lookup on receiver isn't supported in case of a simple smi handler.
    if (!LookupOnReceiverBits::decode(smi_handler->value())) return smi_handler;
    validity_cell = handle(Smi::kZero, isolate);
  }

  int data_count = 1 + checks_count;
  Handle<LoadHandler> handler = isolate->factory()->NewLoadHandler(data_count);

  handler->set_smi_handler(*smi_handler);
  handler->set_validity_cell(*validity_cell);
  InitPrototypeChecks(isolate, handler, receiver_map, end, data1);
  return handler;
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
  Handle<StoreHandler> handler = isolate->factory()->NewStoreHandler(1);
  handler->set_smi_handler(*stub);
  handler->set_validity_cell(*validity_cell);
  handler->set_data1(*cell);
  return handler;
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
    Handle<Smi> smi_handler, MaybeHandle<Object> maybe_data1,
    MaybeHandle<Object> maybe_data2) {
  Handle<Object> data1;
  if (!maybe_data1.ToHandle(&data1)) {
    data1 = Map::GetOrCreatePrototypeWeakCell(holder, isolate);
  }

  int checks_count = GetPrototypeCheckCount<StoreHandler>(
      isolate, &smi_handler, receiver_map, holder, data1, maybe_data2);

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  if (validity_cell.is_null()) {
    DCHECK_EQ(0, checks_count);
    validity_cell = handle(Smi::kZero, isolate);
  }

  int data_count = 1 + checks_count;
  Handle<StoreHandler> handler =
      isolate->factory()->NewStoreHandler(data_count);

  handler->set_smi_handler(*smi_handler);
  handler->set_validity_cell(*validity_cell);
  InitPrototypeChecks(isolate, handler, receiver_map, holder, data1,
                      maybe_data2);
  return handler;
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
                                        Handle<JSReceiver> receiver) {
  Handle<Smi> smi_handler = StoreProxy(isolate);
  if (receiver.is_identical_to(proxy)) return smi_handler;
  Handle<WeakCell> holder_cell = isolate->factory()->NewWeakCell(proxy);
  return StoreThroughPrototype(isolate, receiver_map, proxy, smi_handler,
                               holder_cell);
}

Object* StoreHandler::ValidHandlerOrNull(Object* raw_handler, Name* name,
                                         Handle<Map>* out_transition) {
  Smi* valid = Smi::FromInt(Map::kPrototypeChainValid);

  DCHECK(raw_handler->IsStoreHandler());

  // Check validity cell.
  StoreHandler* handler = StoreHandler::cast(raw_handler);

  Object* raw_validity_cell = handler->validity_cell();
  // |raw_valitity_cell| can be Smi::kZero if no validity cell is required
  // (which counts as valid).
  if (raw_validity_cell->IsCell() &&
      Cell::cast(raw_validity_cell)->value() != valid) {
    return nullptr;
  }
  // We use this ValidHandlerOrNull() function only for transitioning store
  // handlers which are not applicable to receivers that require access checks.
  DCHECK(handler->smi_handler()->IsSmi());
  DCHECK(
      !DoAccessCheckOnReceiverBits::decode(Smi::ToInt(handler->smi_handler())));

  // Check if the transition target is deprecated.
  WeakCell* target_cell = GetTransitionCell(raw_handler);
  Map* transition = Map::cast(target_cell->value());
  if (transition->is_deprecated()) return nullptr;
  *out_transition = handle(transition);
  return raw_handler;
}

}  // namespace internal
}  // namespace v8
