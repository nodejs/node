// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOG_INL_H_
#define V8_LOG_INL_H_

#include "src/log.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

CodeEventListener::LogEventsAndTags Logger::ToNativeByScript(
    CodeEventListener::LogEventsAndTags tag, Script* script) {
  if (script->type() != Script::TYPE_NATIVE) return tag;
  switch (tag) {
    case CodeEventListener::FUNCTION_TAG:
      return CodeEventListener::NATIVE_FUNCTION_TAG;
    case CodeEventListener::LAZY_COMPILE_TAG:
      return CodeEventListener::NATIVE_LAZY_COMPILE_TAG;
    case CodeEventListener::SCRIPT_TAG:
      return CodeEventListener::NATIVE_SCRIPT_TAG;
    default:
      return tag;
  }
}

void Logger::CallEventLogger(Isolate* isolate, const char* name, StartEnd se,
                             bool expose_to_api) {
  if (isolate->event_logger()) {
    if (isolate->event_logger() == DefaultEventLoggerSentinel) {
      LOG(isolate, TimerEvent(se, name));
    } else if (expose_to_api) {
      isolate->event_logger()(name, se);
    }
  }
}

template <class TimerEvent>
void TimerEventScope<TimerEvent>::LogTimerEvent(Logger::StartEnd se) {
  Logger::CallEventLogger(isolate_, TimerEvent::name(), se,
                          TimerEvent::expose_to_api());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_LOG_INL_H_
