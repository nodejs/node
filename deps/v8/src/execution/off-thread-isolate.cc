// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/off-thread-isolate.h"

#include "src/execution/isolate.h"
#include "src/execution/thread-id.h"
#include "src/logging/off-thread-logger.h"

namespace v8 {
namespace internal {

OffThreadIsolate::OffThreadIsolate(Isolate* isolate, Zone* zone)
    : HiddenOffThreadFactory(isolate),
      isolate_(isolate),
      logger_(new OffThreadLogger()),
      handle_zone_(zone) {}
OffThreadIsolate::~OffThreadIsolate() { delete logger_; }

int OffThreadIsolate::GetNextScriptId() { return isolate_->GetNextScriptId(); }

#if V8_SFI_HAS_UNIQUE_ID
int OffThreadIsolate::GetNextUniqueSharedFunctionInfoId() {
  return isolate_->GetNextUniqueSharedFunctionInfoId();
}
#endif  // V8_SFI_HAS_UNIQUE_ID

bool OffThreadIsolate::NeedsSourcePositionsForProfiling() {
  // TODO(leszeks): Figure out if it makes sense to check this asynchronously.
  return isolate_->NeedsSourcePositionsForProfiling();
}

bool OffThreadIsolate::is_collecting_type_profile() {
  // TODO(leszeks): Figure out if it makes sense to check this asynchronously.
  return isolate_->is_collecting_type_profile();
}

void OffThreadIsolate::PinToCurrentThread() {
  DCHECK(!thread_id_.IsValid());
  thread_id_ = ThreadId::Current();
}

}  // namespace internal
}  // namespace v8
