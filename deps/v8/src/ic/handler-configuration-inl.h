// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_HANDLER_CONFIGURATION_INL_H_
#define V8_IC_HANDLER_CONFIGURATION_INL_H_

#include "src/builtins/builtins.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/ic/handler-configuration.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

inline Handle<Object> MakeCodeHandler(Isolate* isolate, Builtin builtin) {
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    Code code = isolate->builtins()->code(builtin);
    return handle(code.code_data_container(kAcquireLoad), isolate);
  } else {
    return isolate->builtins()->code_handle(builtin);
  }
}

OBJECT_CONSTRUCTORS_IMPL(LoadHandler, DataHandler)

CAST_ACCESSOR(LoadHandler)

// Decodes kind from Smi-handler.
LoadHandler::Kind LoadHandler::GetHandlerKind(Smi smi_handler) {
  return KindBits::decode(smi_handler.value());
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

Handle<Smi> LoadHandler::LoadSlow(Isolate* isolate) {
  int config = KindBits::encode(kSlow);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadField(Isolate* isolate, FieldIndex field_index) {
  int config = KindBits::encode(kField) |
               IsInobjectBits::encode(field_index.is_inobject()) |
               IsDoubleBits::encode(field_index.is_double()) |
               FieldIndexBits::encode(field_index.index());
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadWasmStructField(Isolate* isolate,
                                             WasmValueType type, int offset) {
  int config = KindBits::encode(kField) | IsWasmStructBits::encode(true) |
               WasmFieldTypeBits::encode(type) |
               WasmFieldOffsetBits::encode(offset);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> LoadHandler::LoadConstantFromPrototype(Isolate* isolate) {
  int config = KindBits::encode(kConstantFromPrototype);
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

Handle<Smi> LoadHandler::LoadWasmArrayElement(Isolate* isolate,
                                              WasmValueType type) {
  int config = KindBits::encode(kElement) | IsWasmArrayBits::encode(true) |
               WasmArrayTypeBits::encode(type);
  return handle(Smi::FromInt(config), isolate);
}

OBJECT_CONSTRUCTORS_IMPL(StoreHandler, DataHandler)

CAST_ACCESSOR(StoreHandler)

Handle<Smi> StoreHandler::StoreGlobalProxy(Isolate* isolate) {
  int config = KindBits::encode(kGlobalProxy);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreNormal(Isolate* isolate) {
  int config = KindBits::encode(kNormal);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreInterceptor(Isolate* isolate) {
  int config = KindBits::encode(kInterceptor);
  return handle(Smi::FromInt(config), isolate);
}

Builtin StoreHandler::StoreSloppyArgumentsBuiltin(KeyedAccessStoreMode mode) {
  switch (mode) {
    case STANDARD_STORE:
      return Builtin::kKeyedStoreIC_SloppyArguments_Standard;
    case STORE_AND_GROW_HANDLE_COW:
      return Builtin::kKeyedStoreIC_SloppyArguments_GrowNoTransitionHandleCOW;
    case STORE_IGNORE_OUT_OF_BOUNDS:
      return Builtin::kKeyedStoreIC_SloppyArguments_NoTransitionIgnoreOOB;
    case STORE_HANDLE_COW:
      return Builtin::kKeyedStoreIC_SloppyArguments_NoTransitionHandleCOW;
    default:
      UNREACHABLE();
  }
}

Builtin StoreHandler::StoreFastElementBuiltin(KeyedAccessStoreMode mode) {
  switch (mode) {
    case STANDARD_STORE:
      return Builtin::kStoreFastElementIC_Standard;
    case STORE_AND_GROW_HANDLE_COW:
      return Builtin::kStoreFastElementIC_GrowNoTransitionHandleCOW;
    case STORE_IGNORE_OUT_OF_BOUNDS:
      return Builtin::kStoreFastElementIC_NoTransitionIgnoreOOB;
    case STORE_HANDLE_COW:
      return Builtin::kStoreFastElementIC_NoTransitionHandleCOW;
    default:
      UNREACHABLE();
  }
}

Builtin StoreHandler::ElementsTransitionAndStoreBuiltin(
    KeyedAccessStoreMode mode) {
  switch (mode) {
    case STANDARD_STORE:
      return Builtin::kElementsTransitionAndStore_Standard;
    case STORE_AND_GROW_HANDLE_COW:
      return Builtin::kElementsTransitionAndStore_GrowNoTransitionHandleCOW;
    case STORE_IGNORE_OUT_OF_BOUNDS:
      return Builtin::kElementsTransitionAndStore_NoTransitionIgnoreOOB;
    case STORE_HANDLE_COW:
      return Builtin::kElementsTransitionAndStore_NoTransitionHandleCOW;
    default:
      UNREACHABLE();
  }
}

Handle<Smi> StoreHandler::StoreSlow(Isolate* isolate,
                                    KeyedAccessStoreMode store_mode) {
  int config =
      KindBits::encode(kSlow) | KeyedAccessStoreModeBits::encode(store_mode);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreProxy(Isolate* isolate) {
  int config = KindBits::encode(kProxy);
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreField(Isolate* isolate, Kind kind,
                                     int descriptor, FieldIndex field_index,
                                     Representation representation) {
  DCHECK(!representation.IsNone());
  DCHECK(kind == kField || kind == kConstField);

  int config = KindBits::encode(kind) |
               IsInobjectBits::encode(field_index.is_inobject()) |
               RepresentationBits::encode(representation.kind()) |
               DescriptorBits::encode(descriptor) |
               FieldIndexBits::encode(field_index.index());
  return handle(Smi::FromInt(config), isolate);
}

Handle<Smi> StoreHandler::StoreField(Isolate* isolate, int descriptor,
                                     FieldIndex field_index,
                                     PropertyConstness constness,
                                     Representation representation) {
  Kind kind = constness == PropertyConstness::kMutable ? kField : kConstField;
  return StoreField(isolate, kind, descriptor, field_index, representation);
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

inline const char* WasmValueType2String(WasmValueType type) {
  switch (type) {
    case WasmValueType::kI8:
      return "i8";
    case WasmValueType::kI16:
      return "i16";
    case WasmValueType::kI32:
      return "i32";
    case WasmValueType::kU32:
      return "u32";
    case WasmValueType::kI64:
      return "i64";
    case WasmValueType::kF32:
      return "f32";
    case WasmValueType::kF64:
      return "f64";
    case WasmValueType::kS128:
      return "s128";

    case WasmValueType::kRef:
      return "Ref";
    case WasmValueType::kOptRef:
      return "OptRef";

    case WasmValueType::kNumTypes:
      return "???";
  }
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_IC_HANDLER_CONFIGURATION_INL_H_
