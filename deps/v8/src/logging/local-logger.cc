// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/logging/local-logger.h"

#include "src/execution/isolate.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

// TODO(leszeks): Add support for logging from off-thread.
LocalLogger::LocalLogger(Isolate* isolate)
    : logger_(isolate->logger()),
      is_logging_(isolate->logger()->is_logging()),
      is_listening_to_code_events_(
          isolate->logger()->is_listening_to_code_events()) {}

void LocalLogger::ScriptDetails(Script script) {
  logger_->ScriptDetails(script);
}
void LocalLogger::ScriptEvent(Logger::ScriptEventType type, int script_id) {
  logger_->ScriptEvent(type, script_id);
}
void LocalLogger::CodeLinePosInfoRecordEvent(Address code_start,
                                             ByteArray source_position_table,
                                             JitCodeEvent::CodeType code_type) {
  logger_->CodeLinePosInfoRecordEvent(code_start, source_position_table,
                                      code_type);
}

void LocalLogger::MapCreate(Map map) { logger_->MapCreate(map); }

void LocalLogger::MapDetails(Map map) { logger_->MapDetails(map); }

}  // namespace internal
}  // namespace v8
