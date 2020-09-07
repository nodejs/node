// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_OFF_THREAD_LOGGER_H_
#define V8_LOGGING_OFF_THREAD_LOGGER_H_

#include "src/base/logging.h"
#include "src/logging/log.h"

namespace v8 {
namespace internal {

// TODO(leszeks): Add support for logging from off-thread.
class OffThreadLogger {
 public:
  bool is_logging() const { return false; }
  bool is_listening_to_code_events() const { return false; }
  void ScriptEvent(Logger::ScriptEventType type, int script_id) {
    UNREACHABLE();
  }
  void ScriptDetails(Script script) { UNREACHABLE(); }
  void CodeLinePosInfoRecordEvent(Address code_start,
                                  ByteArray source_position_table) {
    UNREACHABLE();
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_OFF_THREAD_LOGGER_H_
