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

Logger::LogEventsAndTags Logger::ToNativeByScript(Logger::LogEventsAndTags tag,
                                                  Script* script) {
  if ((tag == FUNCTION_TAG || tag == LAZY_COMPILE_TAG || tag == SCRIPT_TAG) &&
      script->type() == Script::TYPE_NATIVE) {
    switch (tag) {
      case FUNCTION_TAG: return NATIVE_FUNCTION_TAG;
      case LAZY_COMPILE_TAG: return NATIVE_LAZY_COMPILE_TAG;
      case SCRIPT_TAG: return NATIVE_SCRIPT_TAG;
      default: return tag;
    }
  } else {
    return tag;
  }
}


void Logger::CallEventLogger(Isolate* isolate, const char* name, StartEnd se,
                             bool expose_to_api) {
  if (isolate->event_logger() != NULL) {
    if (isolate->event_logger() == DefaultEventLoggerSentinel) {
      LOG(isolate, TimerEvent(se, name));
    } else if (expose_to_api) {
      isolate->event_logger()(name, se);
    }
  }
}
}  // namespace internal
}  // namespace v8

#endif  // V8_LOG_INL_H_
