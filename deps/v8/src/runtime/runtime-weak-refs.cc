// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/api.h"
#include "src/arguments-inl.h"
#include "src/counters.h"
#include "src/execution.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_FinalizationGroupCleanupJob) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSFinalizationGroup, finalization_group, 0);
  finalization_group->set_scheduled_for_cleanup(false);

  JSFinalizationGroup::Cleanup(finalization_group, isolate);
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
