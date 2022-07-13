// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTENSIONS_STATISTICS_EXTENSION_H_
#define V8_EXTENSIONS_STATISTICS_EXTENSION_H_

#include "include/v8-extension.h"

namespace v8 {

template <typename T>
class FunctionCallbackInfo;

namespace internal {

class StatisticsExtension : public v8::Extension {
 public:
  StatisticsExtension() : v8::Extension("v8/statistics", kSource) {}
  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) override;
  static void GetCounters(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static const char* const kSource;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXTENSIONS_STATISTICS_EXTENSION_H_
