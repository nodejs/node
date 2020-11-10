// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/prefinalizer-handler.h"

#include <algorithm>
#include <memory>

#include "src/base/platform/platform.h"
#include "src/heap/cppgc/heap.h"

namespace cppgc {
namespace internal {

// static
void PreFinalizerRegistrationDispatcher::RegisterPrefinalizer(
    cppgc::Heap* heap, PreFinalizer prefinalzier) {
  internal::Heap::From(heap)->prefinalizer_handler()->RegisterPrefinalizer(
      prefinalzier);
}

bool PreFinalizerRegistrationDispatcher::PreFinalizer::operator==(
    const PreFinalizer& other) {
  return (object_ == other.object_) && (callback_ == other.callback_);
}

PreFinalizerHandler::PreFinalizerHandler()
#ifdef DEBUG
    : creation_thread_id_(v8::base::OS::GetCurrentThreadId())
#endif
{
}

void PreFinalizerHandler::RegisterPrefinalizer(PreFinalizer prefinalizer) {
  DCHECK(CurrentThreadIsCreationThread());
  DCHECK_EQ(ordered_pre_finalizers_.end(),
            std::find(ordered_pre_finalizers_.begin(),
                      ordered_pre_finalizers_.end(), prefinalizer));
  ordered_pre_finalizers_.push_back(prefinalizer);
}

void PreFinalizerHandler::InvokePreFinalizers() {
  DCHECK(CurrentThreadIsCreationThread());
  LivenessBroker liveness_broker = LivenessBrokerFactory::Create();
  ordered_pre_finalizers_.erase(
      ordered_pre_finalizers_.begin(),
      std::remove_if(ordered_pre_finalizers_.rbegin(),
                     ordered_pre_finalizers_.rend(),
                     [liveness_broker](const PreFinalizer& pf) {
                       return (pf.callback_)(liveness_broker, pf.object_);
                     })
          .base());
  ordered_pre_finalizers_.shrink_to_fit();
}

bool PreFinalizerHandler::CurrentThreadIsCreationThread() {
#ifdef DEBUG
  return creation_thread_id_ == v8::base::OS::GetCurrentThreadId();
#else
  return true;
#endif
}

}  // namespace internal
}  // namespace cppgc
