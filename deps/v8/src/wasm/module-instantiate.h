// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_MODULE_INSTANTIATE_H_
#define V8_WASM_MODULE_INSTANTIATE_H_

#include <stdint.h>

#include "src/base/optional.h"
#include "src/common/message-template.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {

class Isolate;
class JSArrayBuffer;
class JSReceiver;
class WasmModuleObject;
class WasmInstanceObject;
class Zone;

template <typename T>
class Handle;
template <typename T>
class MaybeHandle;

namespace wasm {
class ErrorThrower;

MaybeHandle<WasmInstanceObject> InstantiateToInstanceObject(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory);

// Initializes a segment at index {segment_index} of the segment array of
// {instance}. If successful, returns the empty {Optional}, otherwise an
// {Optional} that contains the error message. Exits early if the segment is
// already initialized.
base::Optional<MessageTemplate> InitializeElementSegment(
    Zone* zone, Isolate* isolate, Handle<WasmInstanceObject> instance,
    uint32_t segment_index);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_INSTANTIATE_H_
