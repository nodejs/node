// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_HEAP_H_
#define INCLUDE_CPPGC_HEAP_H_

#include <memory>
#include <vector>

#include "cppgc/common.h"
#include "cppgc/custom-space.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {
class Heap;
}  // namespace internal

class V8_EXPORT Heap {
 public:
  /**
   * Specifies the stack state the embedder is in.
   */
  using StackState = EmbedderStackState;

  struct HeapOptions {
    static HeapOptions Default() { return {}; }

    /**
     * Custom spaces added to heap are required to have indices forming a
     * numbered sequence starting at 0, i.e., their kSpaceIndex must correspond
     * to the index they reside in the vector.
     */
    std::vector<std::unique_ptr<CustomSpaceBase>> custom_spaces;
  };

  static std::unique_ptr<Heap> Create(HeapOptions = HeapOptions::Default());

  virtual ~Heap() = default;

  /**
   * Forces garbage collection.
   *
   * \param source String specifying the source (or caller) triggering a
   *   forced garbage collection.
   * \param reason String specifying the reason for the forced garbage
   *   collection.
   * \param stack_state The embedder stack state, see StackState.
   */
  void ForceGarbageCollectionSlow(
      const char* source, const char* reason,
      StackState stack_state = StackState::kMayContainHeapPointers);

 private:
  Heap() = default;

  friend class internal::Heap;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_HEAP_H_
