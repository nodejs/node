// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTENSIONS_GC_EXTENSION_H_
#define V8_EXTENSIONS_GC_EXTENSION_H_

#include "v8.h"

namespace v8 {
namespace internal {

class GCExtension : public v8::Extension {
 public:
  explicit GCExtension(const char* fun_name)
      : v8::Extension("v8/gc",
                      BuildSource(buffer_, sizeof(buffer_), fun_name)) {}
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name);
  static void GC(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static const char* BuildSource(char* buf, size_t size, const char* fun_name) {
    OS::SNPrintF(Vector<char>(buf, static_cast<int>(size)),
                 "native function %s();", fun_name);
    return buf;
  }

  char buffer_[50];
};

} }  // namespace v8::internal

#endif  // V8_EXTENSIONS_GC_EXTENSION_H_
