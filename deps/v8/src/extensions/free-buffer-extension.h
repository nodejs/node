// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTENSIONS_FREE_BUFFER_EXTENSION_H_
#define V8_EXTENSIONS_FREE_BUFFER_EXTENSION_H_

#include "include/v8.h"

namespace v8 {
namespace internal {

class FreeBufferExtension : public v8::Extension {
 public:
  FreeBufferExtension()
      : v8::Extension("v8/free-buffer", "native function freeBuffer();") {}
  virtual v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name);
  static void FreeBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXTENSIONS_FREE_BUFFER_EXTENSION_H_
