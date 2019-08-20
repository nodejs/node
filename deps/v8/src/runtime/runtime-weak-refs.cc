// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/api/api.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/execution.h"
#include "src/handles/handles-inl.h"
#include "src/logging/counters.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/objects-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_FinalizationGroupCleanupJob) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSFinalizationGroup, finalization_group, 0);
  finalization_group->set_scheduled_for_cleanup(false);

  Handle<Object> cleanup(finalization_group->cleanup(), isolate);
  JSFinalizationGroup::Cleanup(isolate, finalization_group, cleanup);
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
