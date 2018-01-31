// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_HANDLER_CONFIGURATION_INL_H_
#define V8_IC_HANDLER_CONFIGURATION_INL_H_

#include "src/ic/handler-configuration.h"

#include "src/field-index-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// Decodes kind from Smi-handler.
LoadHandler::Kind LoadHandler::GetHandlerKind(Smi* smi_handler) {
  return KindBits::decode(smi_handler->value());
}

Handle<Smi> LoadHandler::LoadNormal(Isolate* isolate) {
  int config = KindBits::encode(kNormal);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadGlobal(Isolate* isolate) {
  int config = KindBits::encode(kGlobal);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadInterceptor(Isolate* isolate) {
  int config = KindBits::encode(kInterceptor);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadField(Isolate* isolate, FieldIndex field_index) {
  int config = KindBits::encode(kField) |
               IsInobjectBits::encode(field_index.is_inobject()) |
               IsDoubleBits::encode(field_index.is_double()) |
               FieldIndexBits::encode(field_index.index());
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadConstant(Isolate* isolate, int descriptor) {
  int config = KindBits::encode(kConstant) | DescriptorBits::encode(descriptor);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadAccessor(Isolate* isolate, int descriptor) {
  int config = KindBits::encode(kAccessor) | DescriptorBits::encode(descriptor);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadProxy(Isolate* isolate) {
  int config = KindBits::encode(kProxy);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadNativeDataProperty(Isolate* isolate,
                                                int descriptor) {
  int config = KindBits::encode(kNativeDataProperty) |
               DescriptorBits::encode(descriptor);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadApiGetter(Isolate* isolate,
                                       bool holder_is_receiver) {
  int config = KindBits::encode(
      holder_is_receiver ? kApiGetter : kApiGetterHolderIsPrototype);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadModuleExport(Isolate* isolate, int index) {
  int config =
      KindBits::encode(kModuleExport) | ExportsIndexBits::encode(index);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::EnableAccessCheckOnReceiver(Isolate* isolate,
                                                     Handle<Smi> smi_handler) {
  int config = smi_handler->value();
#ifdef DEBUG
  Kind kind = KindBits::decode(config);
  DCHECK_NE(kElement, kind);
#endif
  config = DoAccessCheckOnReceiverBits::update(config, true);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::EnableLookupOnReceiver(Isolate* isolate,
                                                Handle<Smi> smi_handler) {
  int config = smi_handler->value();
#ifdef DEBUG
  Kind kind = KindBits::decode(config);
  DCHECK_NE(kElement, kind);
#endif
  config = LookupOnReceiverBits::update(config, true);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadNonExistent(Isolate* isolate) {
  int config = KindBits::encode(kNonExistent);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadElement(Isolate* isolate,
                                     ElementsKind elements_kind,
                                     bool convert_hole_to_undefined,
                                     bool is_js_array,
                                     KeyedAccessLoadMode load_mode) {
  int config =
      KindBits::encode(kElement) |
      AllowOutOfBoundsBits::encode(load_mode == LOAD_IGNORE_OUT_OF_BOUNDS) |
      ElementsKindBits::encode(elements_kind) |
      ConvertHoleBits::encode(convert_hole_to_undefined) |
      IsJsArrayBits::encode(is_js_array);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadIndexedString(Isolate* isolate,
                                           KeyedAccessLoadMode load_mode) {
  int config =
      KindBits::encode(kIndexedString) |
      AllowOutOfBoundsBits::encode(load_mode == LOAD_IGNORE_OUT_OF_BOUNDS);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreGlobalProxy(Isolate* isolate) {
  int config = KindBits::encode(kGlobalProxy);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreNormal(Isolate* isolate) {
  int config = KindBits::encode(kNormal);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreProxy(Isolate* isolate) {
  int config = KindBits::encode(kProxy);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::EnableAccessCheckOnReceiver(Isolate* isolate,
                                                      Handle<Smi> smi_handler) {
  int config = smi_handler->value();
  config = DoAccessCheckOnReceiverBits::update(config, true);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreField(Isolate* isolate, Kind kind,
                                     int descriptor, FieldIndex field_index,
                                     Representation representation,
                                     bool extend_storage) {
  FieldRepresentation field_rep;
  switch (representation.kind()) {
    case Representation::kSmi:
      field_rep = kSmi;
      break;
    case Representation::kDouble:
      field_rep = kDouble;
      break;
    case Representation::kHeapObject:
      field_rep = kHeapObject;
      break;
    case Representation::kTagged:
      field_rep = kTagged;
      break;
    default:
      UNREACHABLE();
  }

  DCHECK(kind == kField || kind == kTransitionToField ||
         (kind == kConstField && FLAG_track_constant_fields));
  DCHECK_IMPLIES(extend_storage, kind == kTransitionToField);
  DCHECK_IMPLIES(field_index.is_inobject(), !extend_storage);

  int config = KindBits::encode(kind) |
               ExtendStorageBits::encode(extend_storage) |
               IsInobjectBits::encode(field_index.is_inobject()) |
               FieldRepresentationBits::encode(field_rep) |
               DescriptorBits::encode(descriptor) |
               FieldIndexBits::encode(field_index.index());
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreField(Isolate* isolate, int descriptor,
                                     FieldIndex field_index,
                                     PropertyConstness constness,
                                     Representation representation) {
  DCHECK_IMPLIES(!FLAG_track_constant_fields, constness == kMutable);
  Kind kind = constness == kMutable ? kField : kConstField;
  return StoreField(isolate, kind, descriptor, field_index, representation,
                    false);
}

Handle<Smi> StoreHandler::TransitionToField(Isolate* isolate, int descriptor,
                                            FieldIndex field_index,
                                            Representation representation,
                                            bool extend_storage) {
  return StoreField(isolate, kTransitionToField, descriptor, field_index,
                    representation, extend_storage);
}

Handle<Smi> StoreHandler::TransitionToConstant(Isolate* isolate,
                                               int descriptor) {
  DCHECK(!FLAG_track_constant_fields);
  int config = KindBits::encode(kTransitionToConstant) |
               DescriptorBits::encode(descriptor);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreNativeDataProperty(Isolate* isolate,
                                                  int descriptor) {
  int config = KindBits::encode(kNativeDataProperty) |
               DescriptorBits::encode(descriptor);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreAccessor(Isolate* isolate, int descriptor) {
  int config = KindBits::encode(kAccessor) | DescriptorBits::encode(descriptor);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreApiSetter(Isolate* isolate,
                                         bool holder_is_receiver) {
  int config = KindBits::encode(
      holder_is_receiver ? kApiSetter : kApiSetterHolderIsPrototype);
  return handle(Smi::FromInt(config), isolate);
}

// static
WeakCell* StoreHandler::GetTransitionCell(Object* handler) {
  if (handler->IsTuple3()) {
    STATIC_ASSERT(kDataOffset == Tuple3::kValue1Offset);
    WeakCell* cell = WeakCell::cast(Tuple3::cast(handler)->value1());
    DCHECK(!cell->cleared());
    return cell;
  }

  DCHECK(handler->IsFixedArrayExact());
  WeakCell* cell = WeakCell::cast(FixedArray::cast(handler)->get(kDataIndex));
  DCHECK(!cell->cleared());
  return cell;
}

// static
bool StoreHandler::IsHandler(Object* maybe_handler) {
  return maybe_handler->IsFixedArrayExact() || maybe_handler->IsTuple3();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_HANDLER_CONFIGURATION_INL_H_
