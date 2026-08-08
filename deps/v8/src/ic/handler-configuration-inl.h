// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_HANDLER_CONFIGURATION_INL_H_
#define V8_IC_HANDLER_CONFIGURATION_INL_H_

#include "src/ic/handler-configuration.h"
// Include the non-inl header before the rest of the headers.

#include "src/builtins/builtins.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Decodes kind from Smi-handler.
LoadHandler::Kind LoadHandler::GetHandlerKind(Tagged<Smi> smi_handler) {
  return KindBits::decode(smi_handler.value());
}

Handle<Smi> LoadHandler::LoadNormal(Isolate* isolate, InternalIndex entry) {
  auto encoding = KindBits::encode(Kind::kNormal);
  encoding =
      entry.is_found() && entry.as_uint32() < DictionaryIndexBits::kMax
          ? DictionaryIndexBits::update(encoding, entry.as_uint32())
          : DictionaryIndexBits::update(encoding, DictionaryIndexBits::kMax);

  return handle(Smi::FromInt(encoding), isolate);
}

Handle<Smi> LoadHandler::LoadGlobal(Isolate* isolate) {
  int config = KindBits::encode(Kind::kGlobal);
  return handle(Smi::FromInt(config), isolate);
}

Tagged<Smi> LoadHandler::LoadInterceptor(bool non_masking) {
  int config = KindBits::encode(Kind::kInterceptor) |
               NonMaskingInterceptorBits::encode(non_masking);
  return Smi::FromInt(config);
}

Handle<Smi> LoadHandler::LoadSlow(Isolate* isolate) {
  int config = KindBits::encode(Kind::kSlow);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadGeneric(Isolate* isolate) {
  return handle(LoadGeneric(), isolate);
}

Tagged<Smi> LoadHandler::LoadGeneric() {
  return Smi::FromInt(KindBits::encode(Kind::kGeneric));
}

Handle<Smi> LoadHandler::LoadField(Isolate* isolate, FieldIndex field_index,
                                   InternalIndex descriptor_index) {
  return LoadField(isolate, field_index.offset_in_words(),
                   field_index.is_inobject(), field_index.is_double(),
                   descriptor_index);
}

Handle<Smi> LoadHandler::LoadField(Isolate* isolate, int offset_in_words,
                                   bool is_in_object, bool is_double,
                                   InternalIndex descriptor_index) {
  return handle(
      LoadField(offset_in_words, is_in_object, is_double, descriptor_index),
      isolate);
}

Tagged<Smi> LoadHandler::LoadField(int offset_in_words, bool is_in_object,
                                   bool is_double,
                                   InternalIndex descriptor_index) {
  int config = KindBits::encode(Kind::kField) |
               IsInobjectBits::encode(is_in_object) |
               IsDoubleBits::encode(is_double) |
               StorageOffsetInWordsBits::encode(offset_in_words);
  if (descriptor_index.is_found()) {
    config |= DescriptorIndexBits::encode(descriptor_index.as_int());
  }
  return Smi::From31BitPattern(config);
}


Handle<Smi> LoadHandler::LoadConstantFromPrototype(Isolate* isolate) {
  int config = KindBits::encode(Kind::kConstantFromPrototype);
  return handle(Smi::FromInt(config), isolate);
}

DirectHandle<Smi> LoadHandler::LoadAccessorFromPrototype(Isolate* isolate) {
  int config = KindBits::encode(Kind::kAccessorFromPrototype);
  return direct_handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadProxy(Isolate* isolate) {
  int config = KindBits::encode(Kind::kProxy);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadNativeDataProperty(Isolate* isolate,
                                                InternalIndex descriptor) {
  int config = KindBits::encode(Kind::kNativeDataProperty) |
               DescriptorBits::encode(descriptor.as_int());
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadApiGetter(Isolate* isolate) {
  int config = KindBits::encode(Kind::kApiGetter);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadModuleExport(Isolate* isolate, int index) {
  int config =
      KindBits::encode(Kind::kModuleExport) | ExportsIndexBits::encode(index);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadNonExistent(Isolate* isolate) {
  int config = KindBits::encode(Kind::kNonExistent);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadElement(Isolate* isolate,
                                     ElementsKind elements_kind,
                                     bool is_js_array,
                                     KeyedAccessLoadMode load_mode) {
  DCHECK_IMPLIES(LoadModeHandlesHoles(load_mode),
                 IsHoleyElementsKind(elements_kind));
  int config = KindBits::encode(Kind::kElement) |
               AllowOutOfBoundsBits::encode(LoadModeHandlesOOB(load_mode)) |
               ElementsKindBits::encode(elements_kind) |
               AllowHandlingHole::encode(LoadModeHandlesHoles(load_mode)) |
               IsJsArrayBits::encode(is_js_array);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::TransitionAndLoadElement(
    Isolate* isolate, ElementsKind kind_after_transition,
    KeyedAccessLoadMode load_mode) {
  int config = KindBits::encode(Kind::kElementWithTransition) |
               AllowOutOfBoundsBits::encode(LoadModeHandlesOOB(load_mode)) |
               ElementsKindBits::encode(kind_after_transition) |
               AllowHandlingHole::encode(LoadModeHandlesHoles(load_mode)) |
               IsJsArrayBits::encode(true);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadIndexedString(Isolate* isolate,
                                           KeyedAccessLoadMode load_mode) {
  int config = KindBits::encode(Kind::kIndexedString) |
               AllowOutOfBoundsBits::encode(LoadModeHandlesOOB(load_mode));
  return handle(Smi::FromInt(config), isolate);
}


DirectHandle<Smi> StoreHandler::StoreGlobalProxy(Isolate* isolate) {
  int config = KindBits::encode(Kind::kGlobalProxy);
  return direct_handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreNormal(Isolate* isolate) {
  int config = KindBits::encode(Kind::kNormal);
  return handle(Smi::FromInt(config), isolate);
}

Tagged<Smi> StoreHandler::StoreInterceptor(bool non_masking) {
  int config = KindBits::encode(Kind::kInterceptor) |
               NonMaskingInterceptorBits::encode(non_masking);
  return Smi::FromInt(config);
}

Handle<Code> StoreHandler::StoreSloppyArgumentsBuiltin(
    Isolate* isolate, KeyedAccessStoreMode mode) {
  switch (mode) {
    case KeyedAccessStoreMode::kInBounds:
      return BUILTIN_CODE(isolate, KeyedStoreIC_SloppyArguments_InBounds);
    case KeyedAccessStoreMode::kGrowAndHandleCOW:
      return BUILTIN_CODE(
          isolate, KeyedStoreIC_SloppyArguments_NoTransitionGrowAndHandleCOW);
    case KeyedAccessStoreMode::kIgnoreTypedArrayOOB:
      return BUILTIN_CODE(
          isolate,
          KeyedStoreIC_SloppyArguments_NoTransitionIgnoreTypedArrayOOB);
    case KeyedAccessStoreMode::kHandleCOW:
      return BUILTIN_CODE(isolate,
                          KeyedStoreIC_SloppyArguments_NoTransitionHandleCOW);
    default:
      UNREACHABLE();
  }
}

Handle<Code> StoreHandler::StoreFastElementBuiltin(Isolate* isolate,
                                                   KeyedAccessStoreMode mode) {
  switch (mode) {
    case KeyedAccessStoreMode::kInBounds:
      return BUILTIN_CODE(isolate, StoreFastElementIC_InBounds);
    case KeyedAccessStoreMode::kGrowAndHandleCOW:
      return BUILTIN_CODE(isolate,
                          StoreFastElementIC_NoTransitionGrowAndHandleCOW);
    case KeyedAccessStoreMode::kIgnoreTypedArrayOOB:
      return BUILTIN_CODE(isolate,
                          StoreFastElementIC_NoTransitionIgnoreTypedArrayOOB);
    case KeyedAccessStoreMode::kHandleCOW:
      return BUILTIN_CODE(isolate, StoreFastElementIC_NoTransitionHandleCOW);
    default:
      UNREACHABLE();
  }
}

DirectHandle<Code> StoreHandler::ElementsTransitionAndStoreBuiltin(
    Isolate* isolate, KeyedAccessStoreMode mode) {
  switch (mode) {
    case KeyedAccessStoreMode::kInBounds:
      return BUILTIN_CODE(isolate, ElementsTransitionAndStore_InBounds);
    case KeyedAccessStoreMode::kGrowAndHandleCOW:
      return BUILTIN_CODE(
          isolate, ElementsTransitionAndStore_NoTransitionGrowAndHandleCOW);
    case KeyedAccessStoreMode::kIgnoreTypedArrayOOB:
      return BUILTIN_CODE(
          isolate, ElementsTransitionAndStore_NoTransitionIgnoreTypedArrayOOB);
    case KeyedAccessStoreMode::kHandleCOW:
      return BUILTIN_CODE(isolate,
                          ElementsTransitionAndStore_NoTransitionHandleCOW);
    default:
      UNREACHABLE();
  }
}

Tagged<Smi> StoreHandler::StoreSlow(KeyedAccessStoreMode store_mode) {
  int config = KindBits::encode(Kind::kSlow) |
               KeyedAccessStoreModeBits::encode(store_mode);
  return Smi::FromInt(config);
}

Handle<Smi> StoreHandler::StoreSlow(Isolate* isolate,
                                    KeyedAccessStoreMode store_mode) {
  return handle(StoreSlow(store_mode), isolate);
}

Handle<Smi> StoreHandler::StoreGeneric(Isolate* isolate) {
  return handle(StoreGeneric(), isolate);
}

Tagged<Smi> StoreHandler::StoreGeneric() {
  return Smi::FromInt(KindBits::encode(Kind::kGeneric));
}

Handle<Smi> StoreHandler::StoreProxy(Isolate* isolate) {
  return handle(StoreProxy(), isolate);
}

Tagged<Smi> StoreHandler::StoreProxy() {
  int config = KindBits::encode(Kind::kProxy);
  return Smi::FromInt(config);
}

Handle<Smi> StoreHandler::StoreField(Isolate* isolate, Kind kind,
                                     InternalIndex descriptor,
                                     FieldIndex field_index,
                                     Representation representation) {
  DCHECK(!representation.IsNone());
  DCHECK(kind == Kind::kField || kind == Kind::kConstField ||
         kind == Kind::kSharedStructField);

  int config = KindBits::encode(kind) |
               IsInobjectBits::encode(field_index.is_inobject()) |
               RepresentationBits::encode(representation.kind()) |
               DescriptorBits::encode(descriptor.as_int()) |
               StorageOffsetInWordsBits::encode(field_index.offset_in_words());
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreField(Isolate* isolate, InternalIndex descriptor,
                                     FieldIndex field_index,
                                     PropertyConstness constness,
                                     Representation representation) {
  Kind kind = constness == PropertyConstness::kMutable ? Kind::kField
                                                       : Kind::kConstField;
  return StoreField(isolate, kind, descriptor, field_index, representation);
}

Handle<Smi> StoreHandler::StoreSharedStructField(
    Isolate* isolate, InternalIndex descriptor, FieldIndex field_index,
    Representation representation) {
  DCHECK(representation.Equals(Representation::Tagged()));
  return StoreField(isolate, Kind::kSharedStructField, descriptor, field_index,
                    representation);
}

Handle<Smi> StoreHandler::StoreNativeDataProperty(Isolate* isolate,
                                                  InternalIndex descriptor) {
  int config = KindBits::encode(Kind::kNativeDataProperty) |
               DescriptorBits::encode(descriptor.as_int());
  return handle(Smi::FromInt(config), isolate);
}

DirectHandle<Smi> StoreHandler::StoreAccessorFromPrototype(Isolate* isolate) {
  int config = KindBits::encode(Kind::kAccessorFromPrototype);
  return direct_handle(Smi::FromInt(config), isolate);
}

DirectHandle<Smi> StoreHandler::StoreApiSetter(Isolate* isolate) {
  int config = KindBits::encode(Kind::kApiSetter);
  return direct_handle(Smi::FromInt(config), isolate);
}


}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_IC_HANDLER_CONFIGURATION_INL_H_
