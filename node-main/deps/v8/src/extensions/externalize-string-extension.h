// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTENSIONS_EXTERNALIZE_STRING_EXTENSION_H_
#define V8_EXTENSIONS_EXTERNALIZE_STRING_EXTENSION_H_

#include "include/v8-extension.h"

namespace v8 {

template <typename T>
class FunctionCallbackInfo;

namespace internal {

class ExternalizeStringExtension : public v8::Extension {
 public:
  ExternalizeStringExtension()
      : v8::Extension("v8/externalize", BuildSource(buffer_, sizeof(buffer_))) {
  }
  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) override;
  static void Externalize(const v8::FunctionCallbackInfo<v8::Value>& info);
  static void CreateExternalizableString(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void CreateExternalizableTwoByteString(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void IsOneByte(const v8::FunctionCallbackInfo<v8::Value>& info);

 private:
  static const char* BuildSource(char* buf, size_t size);
  char buffer_[400];
  static const char* const kSource;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXTENSIONS_EXTERNALIZE_STRING_EXTENSION_H_
