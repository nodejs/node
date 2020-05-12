// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap.h"

#include <memory>

#include "src/heap/cppgc/heap-object-header.h"

namespace cppgc {

std::unique_ptr<Heap> Heap::Create() {
  return std::make_unique<internal::Heap>();
}

namespace internal {

void Heap::CollectGarbage() {
  for (HeapObjectHeader* header : objects_) {
    header->Finalize();
    free(header);
  }
  objects_.clear();
}

}  // namespace internal
}  // namespace cppgc
