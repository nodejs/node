// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/local-isolate.h"

#include "src/bigint/bigint.h"
#include "src/execution/isolate.h"
#include "src/execution/thread-id.h"
#include "src/handles/handles-inl.h"
#include "src/logging/local-logger.h"
#include "src/logging/runtime-call-stats-scope.h"

namespace v8 {
namespace internal {

LocalIsolate::LocalIsolate(Isolate* isolate, ThreadKind kind)
    : HiddenLocalFactory(isolate),
      heap_(isolate->heap(), kind),
      isolate_(isolate),
      logger_(new LocalLogger(isolate)),
      thread_id_(ThreadId::Current()),
      stack_limit_(kind == ThreadKind::kMain
                       ? isolate->stack_guard()->real_climit()
                       : GetCurrentStackPosition() - v8_flags.stack_size * KB)
#ifdef V8_INTL_SUPPORT
      ,
      default_locale_(isolate->DefaultLocale())
#endif
{
#ifdef V8_RUNTIME_CALL_STATS
  if (kind == ThreadKind::kMain) {
    runtime_call_stats_ = isolate->counters()->runtime_call_stats();
  } else {
    rcs_scope_.emplace(isolate->counters()->worker_thread_runtime_call_stats());
    runtime_call_stats_ = rcs_scope_->Get();
  }
#endif
}

LocalIsolate::~LocalIsolate() {
  if (bigint_processor_) bigint_processor_->Destroy();
}

void LocalIsolate::RegisterDeserializerStarted() {
  return isolate_->RegisterDeserializerStarted();
}
void LocalIsolate::RegisterDeserializerFinished() {
  return isolate_->RegisterDeserializerFinished();
}
bool LocalIsolate::has_active_deserializer() const {
  return isolate_->has_active_deserializer();
}

int LocalIsolate::GetNextScriptId() { return isolate_->GetNextScriptId(); }

// Used for lazy initialization, based on an assumption that most
// LocalIsolates won't be used to parse any BigInt literals.
void LocalIsolate::InitializeBigIntProcessor() {
  bigint_processor_ = bigint::Processor::New(new bigint::Platform());
}

// static
bool StackLimitCheck::HasOverflowed(LocalIsolate* local_isolate) {
  return GetCurrentStackPosition() < local_isolate->stack_limit();
}

#ifdef V8_INTL_SUPPORT
// WARNING: This might be out-of-sync with the main-thread.
const std::string& LocalIsolate::DefaultLocale() {
  const std::string& res =
      is_main_thread() ? isolate_->DefaultLocale() : default_locale_;
  DCHECK(!res.empty());
  return res;
}
#endif

}  // namespace internal
}  // namespace v8
