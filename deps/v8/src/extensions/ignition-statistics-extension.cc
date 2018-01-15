// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/ignition-statistics-extension.h"

#include "src/base/logging.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

v8::Local<v8::FunctionTemplate>
IgnitionStatisticsExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate, v8::Local<v8::String> name) {
  DCHECK_EQ(strcmp(*v8::String::Utf8Value(isolate, name),
                   "getIgnitionDispatchCounters"),
            0);
  return v8::FunctionTemplate::New(
      isolate, IgnitionStatisticsExtension::GetIgnitionDispatchCounters);
}

const char* const IgnitionStatisticsExtension::kSource =
    "native function getIgnitionDispatchCounters();";

void IgnitionStatisticsExtension::GetIgnitionDispatchCounters(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(FLAG_trace_ignition_dispatches);
  args.GetReturnValue().Set(reinterpret_cast<Isolate*>(args.GetIsolate())
                                ->interpreter()
                                ->GetDispatchCountersObject());
}

}  // namespace internal
}  // namespace v8
