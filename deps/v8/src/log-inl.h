// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOG_INL_H_
#define V8_LOG_INL_H_

#include "log.h"

namespace v8 {
namespace internal {

Logger::LogEventsAndTags Logger::ToNativeByScript(Logger::LogEventsAndTags tag,
                                                  Script* script) {
  if ((tag == FUNCTION_TAG || tag == LAZY_COMPILE_TAG || tag == SCRIPT_TAG)
      && script->type()->value() == Script::TYPE_NATIVE) {
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


} }  // namespace v8::internal

#endif  // V8_LOG_INL_H_
