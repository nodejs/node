// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_WASM_SUPPORT_H_
#define V8_DEBUG_DEBUG_WASM_SUPPORT_H_

#include <memory>

namespace v8 {
namespace debug {
class ScopeIterator;
}  // namespace debug

namespace internal {

template <typename T>
class Handle;
class JSObject;
class WasmFrame;

Handle<JSObject> GetWasmDebugProxy(WasmFrame* frame);

std::unique_ptr<debug::ScopeIterator> GetWasmScopeIterator(WasmFrame* frame);

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_WASM_SUPPORT_H_
