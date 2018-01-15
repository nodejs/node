// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_API_H_
#define V8_WASM_API_H_

#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

// Like an ErrorThrower, but turns all pending exceptions into scheduled
// exceptions when going out of scope. Use this in API methods.
// Note that pending exceptions are not necessarily created by the ErrorThrower,
// but e.g. by the wasm start function. There might also be a scheduled
// exception, created by another API call (e.g. v8::Object::Get). But there
// should never be both pending and scheduled exceptions.
class V8_EXPORT_PRIVATE ScheduledErrorThrower : public ErrorThrower {
 public:
  ScheduledErrorThrower(v8::Isolate* isolate, const char* context)
      : ScheduledErrorThrower(reinterpret_cast<Isolate*>(isolate), context) {}

  ScheduledErrorThrower(Isolate* isolate, const char* context)
      : ErrorThrower(isolate, context) {}

  ~ScheduledErrorThrower();
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_API_H_
