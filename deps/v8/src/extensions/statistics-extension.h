// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXTENSIONS_STATISTICS_EXTENSION_H_
#define V8_EXTENSIONS_STATISTICS_EXTENSION_H_

#include "src/v8.h"

namespace v8 {
namespace internal {

class StatisticsExtension : public v8::Extension {
 public:
  StatisticsExtension() : v8::Extension("v8/statistics", kSource) {}
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name);
  static void GetCounters(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static const char* const kSource;
};

} }  // namespace v8::internal

#endif  // V8_EXTENSIONS_STATISTICS_EXTENSION_H_
