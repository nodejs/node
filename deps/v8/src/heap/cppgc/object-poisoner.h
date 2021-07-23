// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_OBJECT_POISONER_H_
#define V8_HEAP_CPPGC_OBJECT_POISONER_H_

#include "src/base/sanitizer/asan.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/object-view.h"

namespace cppgc {
namespace internal {

#ifdef V8_USE_ADDRESS_SANITIZER

// Poisons the payload of unmarked objects.
class UnmarkedObjectsPoisoner : public HeapVisitor<UnmarkedObjectsPoisoner> {
  friend class HeapVisitor<UnmarkedObjectsPoisoner>;

 private:
  bool VisitHeapObjectHeader(HeapObjectHeader& header) {
    if (header.IsFree() || header.IsMarked()) return true;

    ASAN_POISON_MEMORY_REGION(header.ObjectStart(), ObjectView(header).Size());
    return true;
  }
};

#endif  // V8_USE_ADDRESS_SANITIZER

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_OBJECT_POISONER_H_
