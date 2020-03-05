// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug-helper-internal.h"
#include "torque-generated/class-debug-readers-tq.h"

namespace di = v8_debug_helper_internal;

extern "C" {
V8_DEBUG_HELPER_EXPORT const d::ClassList*
_v8_debug_helper_ListObjectClasses() {
  return &di::kObjectClassList;
}
}
