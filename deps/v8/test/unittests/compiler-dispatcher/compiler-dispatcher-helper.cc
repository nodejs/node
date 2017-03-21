// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler-dispatcher/compiler-dispatcher-helper.h"

#include <memory>

#include "include/v8.h"
#include "src/api.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

Handle<Object> RunJS(v8::Isolate* isolate, const char* script) {
  return Utils::OpenHandle(
      *v8::Script::Compile(
           isolate->GetCurrentContext(),
           v8::String::NewFromUtf8(isolate, script, v8::NewStringType::kNormal)
               .ToLocalChecked())
           .ToLocalChecked()
           ->Run(isolate->GetCurrentContext())
           .ToLocalChecked());
}

}  // namespace internal
}  // namespace v8
