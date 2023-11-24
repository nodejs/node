// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_LOG_INL_H_
#define V8_LOGGING_LOG_INL_H_

#include "src/execution/isolate.h"
#include "src/logging/log.h"
#include "src/objects/objects-inl.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

LogEventListener::CodeTag V8FileLogger::ToNativeByScript(
    LogEventListener::CodeTag tag, Tagged<Script> script) {
  if (script->type() != Script::Type::kNative) return tag;
  switch (tag) {
    case LogEventListener::CodeTag::kFunction:
      return LogEventListener::CodeTag::kNativeFunction;
    case LogEventListener::CodeTag::kScript:
      return LogEventListener::CodeTag::kNativeScript;
    default:
      return tag;
  }
}

template <class TimerEvent>
void TimerEventScope<TimerEvent>::LogTimerEvent(v8::LogEventStatus se) {
  V8FileLogger::CallEventLogger(isolate_, TimerEvent::name(), se,
                                TimerEvent::expose_to_api());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_LOG_INL_H_
