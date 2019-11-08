// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/handler-configuration.h"

#include "src/codegen/code-factory.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/transitions.h"

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
                            Handle<JSReceiver> holder, MaybeObjectHandle data1,
                            MaybeObjectHandle maybe_data2) {
  int data_size = 1;
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
      handler->set_data2(HeapObjectReference::Weak(*native_context));
    } else {
      // Enable access checks on receiver.
      using Bit = typename ICHandler::DoAccessCheckOnReceiverBits;
      *smi_handler = SetBitFieldValue<Bit>(isolate, *smi_handler, true);
    }
    data_size++;
  } else if (receiver_map->is_dictionary_map() &&
             !receiver_map->IsJSGlobalObjectMap()) {
    if (!fill_handler) {
      // Enable lookup on receiver.
      using Bit = typename ICHandler::LookupOnReceiverBits;
      *smi_handler = SetBitFieldValue<Bit>(isolate, *smi_handler, true);
    }
  }
  if (fill_handler) {
    handler->set_data1(*data1);
  }
  if (!maybe_data2.is_null()) {
    if (fill_handler) {
      // This value will go either to data2 or data3 slot depending on whether
      // data2 slot is already occupied by native context.
      if (data_size == 1) {
        handler->set_data2(*maybe_data2);
      } else {
        DCHECK_EQ(2, data_size);
        handler->set_data3(*maybe_data2);
      }
    }
    data_size++;
  }
  return data_size;
}

// Returns 0 if the validity cell check is enough to ensure that the
// prototype chain from |receiver_map| till |holder| did not change.
// If the |holder| is an empty handle then the full prototype chain is
// checked.
// Returns -1 if the handler has to be compiled or the number of prototype
// checks otherwise.
template <typename ICHandler>
int GetHandlerDataSize(Isolate* isolate, Handle<Smi>* smi_handler,
                       Handle<Map> receiver_map, Handle<JSReceiver> holder,
                       MaybeObjectHandle data1,
                       MaybeObjectHandle maybe_data2 = MaybeObjectHandle()) {
  DCHECK_NOT_NULL(smi_handler);
  return InitPrototypeChecksImpl<ICHandler, false>(isolate, Handle<ICHandler>(),
                                                   smi_handler, receiver_map,
                                                   holder, data1, maybe_data2);
}

template <typename ICHandler>
void InitPrototypeChecks(Isolate* isolate, Handle<ICHandler> handler,
                         Handle<Map> receiver_map, Handle<JSReceiver> holder,
                         MaybeObjectHandle data1,
                         MaybeObjectHandle maybe_data2 = MaybeObjectHandle()) {
  InitPrototypeChecksImpl<ICHandler, true>(
      isolate, handler, nullptr, receiver_map, holder, data1, maybe_data2);
}

}  // namespace

// static
Handle<Object> LoadHandler::LoadFromPrototype(Isolate* isolate,
                                              Handle<Map> receiver_map,
                                              Handle<JSReceiver> holder,
                                              Handle<Smi> smi_handler,
                                              MaybeObjectHandle maybe_data1,
                                              MaybeObjectHandle maybe_data2) {
  MaybeObjectHandle data1;
  if (maybe_data1.is_null()) {
    data1 = MaybeObjectHandle::Weak(holder);
  } else {
    data1 = maybe_data1;
  }

  int data_size = GetHandlerDataSize<LoadHandler>(
      isolate, &smi_handler, receiver_map, holder, data1, maybe_data2);

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);

  Handle<LoadHandler> handler = isolate->factory()->NewLoadHandler(data_size);

  handler->set_smi_handler(*smi_handler);
  handler->set_validity_cell(*validity_cell);
  InitPrototypeChecks(isolate, handler, receiver_map, holder, data1,
                      maybe_data2);
  return handler;
}

// static
Handle<Object> LoadHandler::LoadFullChain(Isolate* isolate,
                                          Handle<Map> receiver_map,
                                          const MaybeObjectHandle& holder,
                                          Handle<Smi> smi_handler) {
  Handle<JSReceiver> end;  // null handle, means full prototype chain lookup.
  MaybeObjectHandle data1 = holder;
  int data_size = GetHandlerDataSize<LoadHandler>(isolate, &smi_handler,
                                                  receiver_map, end, data1);

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  if (validity_cell->IsSmi()) {
    DCHECK_EQ(1, data_size);
    // Lookup on receiver isn't supported in case of a simple smi handler.
    if (!LookupOnReceiverBits::decode(smi_handler->value())) return smi_handler;
  }

  Handle<LoadHandler> handler = isolate->factory()->NewLoadHandler(data_size);

  handler->set_smi_handler(*smi_handler);
  handler->set_validity_cell(*validity_cell);
  InitPrototypeChecks(isolate, handler, receiver_map, end, data1);
  return handler;
}

// static
KeyedAccessLoadMode LoadHandler::GetKeyedAccessLoadMode(MaybeObject handler) {
  DisallowHeapAllocation no_gc;
  if (handler->IsSmi()) {
    int const raw_handler = handler.ToSmi().value();
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
  Handle<Code> stub =
      CodeFactory::ElementsTransitionAndStore(isolate, store_mode).code();
  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);
  Handle<StoreHandler> handler = isolate->factory()->NewStoreHandler(1);
  handler->set_smi_handler(*stub);
  handler->set_validity_cell(*validity_cell);
  handler->set_data1(HeapObjectReference::Weak(*transition));
  return handler;
}

MaybeObjectHandle StoreHandler::StoreTransition(Isolate* isolate,
                                                Handle<Map> transition_map) {
  bool is_dictionary_map = transition_map->is_dictionary_map();
#ifdef DEBUG
  if (!is_dictionary_map) {
    InternalIndex descriptor = transition_map->LastAdded();
    Handle<DescriptorArray> descriptors(transition_map->instance_descriptors(),
                                        isolate);
    PropertyDetails details = descriptors->GetDetails(descriptor);
    if (descriptors->GetKey(descriptor).IsPrivate()) {
      DCHECK_EQ(DONT_ENUM, details.attributes());
    } else {
      DCHECK_EQ(NONE, details.attributes());
    }
    Representation representation = details.representation();
    DCHECK(!representation.IsNone());
  }
#endif
  // Declarative handlers don't support access checks.
  DCHECK(!transition_map->is_access_check_needed());

  // Get validity cell value if it is necessary for the handler.
  Handle<Object> validity_cell;
  if (is_dictionary_map || !transition_map->IsPrototypeValidityCellValid()) {
    validity_cell =
        Map::GetOrCreatePrototypeChainValidityCell(transition_map, isolate);
  }

  if (is_dictionary_map) {
    DCHECK(!transition_map->IsJSGlobalObjectMap());
    Handle<StoreHandler> handler = isolate->factory()->NewStoreHandler(0);
    // Store normal with enabled lookup on receiver.
    int config = KindBits::encode(kNormal) | LookupOnReceiverBits::encode(true);
    handler->set_smi_handler(Smi::FromInt(config));
    handler->set_validity_cell(*validity_cell);
    return MaybeObjectHandle(handler);

  } else {
    // Ensure the transition map contains a valid prototype validity cell.
    if (!validity_cell.is_null()) {
      transition_map->set_prototype_validity_cell(*validity_cell);
    }
    return MaybeObjectHandle::Weak(transition_map);
  }
}

// static
Handle<Object> StoreHandler::StoreThroughPrototype(
    Isolate* isolate, Handle<Map> receiver_map, Handle<JSReceiver> holder,
    Handle<Smi> smi_handler, MaybeObjectHandle maybe_data1,
    MaybeObjectHandle maybe_data2) {
  MaybeObjectHandle data1;
  if (maybe_data1.is_null()) {
    data1 = MaybeObjectHandle::Weak(holder);
  } else {
    data1 = maybe_data1;
  }

  int data_size = GetHandlerDataSize<StoreHandler>(
      isolate, &smi_handler, receiver_map, holder, data1, maybe_data2);

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate);

  Handle<StoreHandler> handler = isolate->factory()->NewStoreHandler(data_size);

  handler->set_smi_handler(*smi_handler);
  handler->set_validity_cell(*validity_cell);
  InitPrototypeChecks(isolate, handler, receiver_map, holder, data1,
                      maybe_data2);
  return handler;
}

// static
MaybeObjectHandle StoreHandler::StoreGlobal(Handle<PropertyCell> cell) {
  return MaybeObjectHandle::Weak(cell);
}

// static
Handle<Object> StoreHandler::StoreProxy(Isolate* isolate,
                                        Handle<Map> receiver_map,
                                        Handle<JSProxy> proxy,
                                        Handle<JSReceiver> receiver) {
  Handle<Smi> smi_handler = StoreProxy(isolate);
  if (receiver.is_identical_to(proxy)) return smi_handler;
  return StoreThroughPrototype(isolate, receiver_map, proxy, smi_handler,
                               MaybeObjectHandle::Weak(proxy));
}

}  // namespace internal
}  // namespace v8
