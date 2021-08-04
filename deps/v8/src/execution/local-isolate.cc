// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/local-isolate.h"

#include "src/execution/isolate.h"
#include "src/execution/thread-id.h"
#include "src/handles/handles-inl.h"
#include "src/logging/local-logger.h"

namespace v8 {
namespace internal {

LocalIsolate::LocalIsolate(Isolate* isolate, ThreadKind kind,
                           RuntimeCallStats* runtime_call_stats)
    : HiddenLocalFactory(isolate),
      heap_(isolate->heap(), kind),
      isolate_(isolate),
      logger_(new LocalLogger(isolate)),
      thread_id_(ThreadId::Current()),
      stack_limit_(kind == ThreadKind::kMain
                       ? isolate->stack_guard()->real_climit()
                       : GetCurrentStackPosition() - FLAG_stack_size * KB),
      runtime_call_stats_(runtime_call_stats) {}

LocalIsolate::~LocalIsolate() = default;

int LocalIsolate::GetNextScriptId() { return isolate_->GetNextScriptId(); }

#if V8_SFI_HAS_UNIQUE_ID
int LocalIsolate::GetNextUniqueSharedFunctionInfoId() {
  return isolate_->GetNextUniqueSharedFunctionInfoId();
}
#endif  // V8_SFI_HAS_UNIQUE_ID

bool LocalIsolate::is_collecting_type_profile() const {
  // TODO(leszeks): Figure out if it makes sense to check this asynchronously.
  return isolate_->is_collecting_type_profile();
}

// static
bool StackLimitCheck::HasOverflowed(LocalIsolate* local_isolate) {
  return GetCurrentStackPosition() < local_isolate->stack_limit();
}

}  // namespace internal
}  // namespace v8
