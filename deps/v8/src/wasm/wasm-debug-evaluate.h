// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_DEBUG_EVALUATE_H_
#define V8_WASM_WASM_DEBUG_EVALUATE_H_

#include "src/base/macros.h"
#include "src/handles/maybe-handles.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

MaybeHandle<String> V8_EXPORT_PRIVATE DebugEvaluate(
    Vector<const byte> snippet, Handle<WasmInstanceObject> debuggee_instance,
    WasmInterpreter::FramePtr frame);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_DEBUG_EVALUATE_H_
