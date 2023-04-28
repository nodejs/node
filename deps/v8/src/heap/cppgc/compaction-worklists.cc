
// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/compaction-worklists.h"

namespace cppgc {
namespace internal {

void CompactionWorklists::ClearForTesting() { movable_slots_worklist_.Clear(); }

}  // namespace internal
}  // namespace cppgc
