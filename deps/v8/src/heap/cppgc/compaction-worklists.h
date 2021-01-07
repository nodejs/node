// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_COMPACTION_WORKLISTS_H_
#define V8_HEAP_CPPGC_COMPACTION_WORKLISTS_H_

#include <unordered_set>

#include "src/heap/base/worklist.h"

namespace cppgc {
namespace internal {

class CompactionWorklists {
 public:
  using MovableReference = const void*;

  using MovableReferencesWorklist =
      heap::base::Worklist<MovableReference*, 256 /* local entries */>;

  MovableReferencesWorklist* movable_slots_worklist() {
    return &movable_slots_worklist_;
  }

  void ClearForTesting();

 private:
  MovableReferencesWorklist movable_slots_worklist_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_COMPACTION_WORKLISTS_H_
