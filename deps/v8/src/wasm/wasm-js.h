// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_JS_H_
#define V8_WASM_WASM_JS_H_

#include <memory>

#include "src/common/globals.h"

namespace v8 {
class WasmStreaming;
}  // namespace v8

namespace v8::internal {

namespace wasm {
class CompilationResultResolver;
class StreamingDecoder;

V8_EXPORT_PRIVATE std::unique_ptr<WasmStreaming> StartStreamingForTesting(
    Isolate*, std::shared_ptr<wasm::CompilationResultResolver>);
}  // namespace wasm

// Exposes a WebAssembly API to JavaScript through the V8 API.
class WasmJs {
 public:
  V8_EXPORT_PRIVATE static void Install(Isolate* isolate,
                                        bool exposed_on_global_object);

  V8_EXPORT_PRIVATE static void InstallConditionalFeatures(
      Isolate* isolate, Handle<NativeContext> context);
};

}  // namespace v8::internal

#endif  // V8_WASM_WASM_JS_H_
