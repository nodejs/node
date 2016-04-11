// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_JS_H_
#define V8_WASM_JS_H_

#ifndef V8_SHARED
#include "src/allocation.h"
#include "src/hashmap.h"
#else
#include "include/v8.h"
#include "src/base/compiler-specific.h"
#endif  // !V8_SHARED

namespace v8 {
namespace internal {
// Exposes a WASM API to JavaScript through the V8 API.
class WasmJs {
 public:
  static void Install(Isolate* isolate, Handle<JSGlobalObject> global_object);
  static void InstallWasmFunctionMap(Isolate* isolate, Handle<Context> context);
};

}  // namespace internal
}  // namespace v8
#endif
