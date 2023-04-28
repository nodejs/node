// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTENSIONS_GC_EXTENSION_H_
#define V8_EXTENSIONS_GC_EXTENSION_H_

#include "include/v8-extension.h"
#include "include/v8-local-handle.h"
#include "src/base/strings.h"

namespace v8 {

template <typename T>
class FunctionCallbackInfo;

namespace internal {

// Provides garbage collection on invoking |fun_name|(options), where
// - options is a dictionary like object. See supported properties below.
// - no parameter refers to options:
//   {type: 'major', execution: 'sync'}.
// - truthy parameter that is not setting any options:
//   {type: 'minor', execution: 'sync'}.
//
// Supported options:
// - type: 'major' or 'minor' for full GC and Scavenge, respectively.
// - execution: 'sync' or 'async' for synchronous and asynchronous execution,
// respectively.
// - Defaults to {type: 'major', execution: 'sync'}.
//
// Returns a Promise that resolves when GC is done when asynchronous execution
// is requested, and undefined otherwise.
class GCExtension : public v8::Extension {
 public:
  explicit GCExtension(const char* fun_name)
      : v8::Extension("v8/gc",
                      BuildSource(buffer_, sizeof(buffer_), fun_name)) {}
  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) override;
  static void GC(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static const char* BuildSource(char* buf, size_t size, const char* fun_name) {
    base::SNPrintF(base::Vector<char>(buf, static_cast<int>(size)),
                   "native function %s();", fun_name);
    return buf;
  }

  char buffer_[50];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXTENSIONS_GC_EXTENSION_H_
