// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTENSIONS_EXTERNALIZE_STRING_EXTENSION_H_
#define V8_EXTENSIONS_EXTERNALIZE_STRING_EXTENSION_H_

#include "src/v8.h"

namespace v8 {
namespace internal {

class ExternalizeStringExtension : public v8::Extension {
 public:
  ExternalizeStringExtension() : v8::Extension("v8/externalize", kSource) {}
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name);
  static void Externalize(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsAscii(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static const char* const kSource;
};

} }  // namespace v8::internal

#endif  // V8_EXTENSIONS_EXTERNALIZE_STRING_EXTENSION_H_
