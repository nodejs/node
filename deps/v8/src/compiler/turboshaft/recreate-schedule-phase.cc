// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/recreate-schedule-phase.h"

namespace v8::internal::compiler::turboshaft {

RecreateScheduleResult RecreateSchedulePhase::Run(Zone* temp_zone,
                                                  Linkage* linkage) {
  return RecreateSchedule(linkage->GetIncomingDescriptor(), temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
