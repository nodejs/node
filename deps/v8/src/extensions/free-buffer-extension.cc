// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/free-buffer-extension.h"

#include "src/base/platform/platform.h"
#include "src/v8.h"

namespace v8 {
namespace internal {


v8::Handle<v8::FunctionTemplate> FreeBufferExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate,
    v8::Handle<v8::String> str) {
  return v8::FunctionTemplate::New(isolate, FreeBufferExtension::FreeBuffer);
}


void FreeBufferExtension::FreeBuffer(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Handle<v8::ArrayBuffer> arrayBuffer = args[0].As<v8::ArrayBuffer>();
  v8::ArrayBuffer::Contents contents = arrayBuffer->Externalize();
  Isolate* isolate = reinterpret_cast<Isolate*>(args.GetIsolate());
  isolate->array_buffer_allocator()->Free(contents.Data(),
                                          contents.ByteLength());
}

}  // namespace internal
}  // namespace v8
