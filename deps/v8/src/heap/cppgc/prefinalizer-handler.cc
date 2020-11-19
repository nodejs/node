// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/prefinalizer-handler.h"

#include <algorithm>
#include <memory>

#include "src/base/platform/platform.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/liveness-broker.h"

namespace cppgc {
namespace internal {

// static
void PreFinalizerRegistrationDispatcher::RegisterPrefinalizer(
    PreFinalizer pre_finalizer) {
  BasePage::FromPayload(pre_finalizer.object)
      ->heap()
      ->prefinalizer_handler()
      ->RegisterPrefinalizer(pre_finalizer);
}

bool PreFinalizerRegistrationDispatcher::PreFinalizer::operator==(
    const PreFinalizer& other) const {
  return (object == other.object) && (callback == other.callback);
}

PreFinalizerHandler::PreFinalizerHandler()
#ifdef DEBUG
    : creation_thread_id_(v8::base::OS::GetCurrentThreadId())
#endif
{
}

void PreFinalizerHandler::RegisterPrefinalizer(PreFinalizer pre_finalizer) {
  DCHECK(CurrentThreadIsCreationThread());
  DCHECK_EQ(ordered_pre_finalizers_.end(),
            std::find(ordered_pre_finalizers_.begin(),
                      ordered_pre_finalizers_.end(), pre_finalizer));
  ordered_pre_finalizers_.push_back(pre_finalizer);
}

void PreFinalizerHandler::InvokePreFinalizers() {
  DCHECK(CurrentThreadIsCreationThread());
  LivenessBroker liveness_broker = LivenessBrokerFactory::Create();
  ordered_pre_finalizers_.erase(
      ordered_pre_finalizers_.begin(),
      std::remove_if(ordered_pre_finalizers_.rbegin(),
                     ordered_pre_finalizers_.rend(),
                     [liveness_broker](const PreFinalizer& pf) {
                       return (pf.callback)(liveness_broker, pf.object);
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
