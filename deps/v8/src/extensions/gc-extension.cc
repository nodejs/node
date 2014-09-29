// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/gc-extension.h"

#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {


v8::Handle<v8::FunctionTemplate> GCExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate,
    v8::Handle<v8::String> str) {
  return v8::FunctionTemplate::New(isolate, GCExtension::GC);
}


void GCExtension::GC(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetIsolate()->RequestGarbageCollectionForTesting(
      args[0]->BooleanValue() ? v8::Isolate::kMinorGarbageCollection
                              : v8::Isolate::kFullGarbageCollection);
}

} }  // namespace v8::internal
