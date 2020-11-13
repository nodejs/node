// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_HEAP_H_
#define INCLUDE_CPPGC_HEAP_H_

#include <memory>
#include <vector>

#include "cppgc/common.h"
#include "cppgc/custom-space.h"
#include "cppgc/platform.h"
#include "v8config.h"  // NOLINT(build/include_directory)

/**
 * cppgc - A C++ garbage collection library.
 */
namespace cppgc {

class AllocationHandle;

/**
 * Implementation details of cppgc. Those details are considered internal and
 * may change at any point in time without notice. Users should never rely on
 * the contents of this namespace.
 */
namespace internal {
class Heap;
}  // namespace internal

class V8_EXPORT Heap {
 public:
  /**
   * Specifies the stack state the embedder is in.
   */
  using StackState = EmbedderStackState;

  /**
   * Specifies whether conservative stack scanning is supported.
   */
  enum class StackSupport : uint8_t {
    /**
     * Conservative stack scan is supported.
     */
    kSupportsConservativeStackScan,
    /**
     * Conservative stack scan is not supported. Embedders may use this option
     * when using custom infrastructure that is unsupported by the library.
     */
    kNoConservativeStackScan,
  };

  /**
   * Constraints for a Heap setup.
   */
  struct ResourceConstraints {
    /**
     * Allows the heap to grow to some initial size in bytes before triggering
     * garbage collections. This is useful when it is known that applications
     * need a certain minimum heap to run to avoid repeatedly invoking the
     * garbage collector when growing the heap.
     */
    size_t initial_heap_size_bytes = 0;
  };

  /**
   * Options specifying Heap properties (e.g. custom spaces) when initializing a
   * heap through Heap::Create().
   */
  struct HeapOptions {
    /**
     * Creates reasonable defaults for instantiating a Heap.
     *
     * \returns the HeapOptions that can be passed to Heap::Create().
     */
    static HeapOptions Default() { return {}; }

    /**
     * Custom spaces added to heap are required to have indices forming a
     * numbered sequence starting at 0, i.e., their kSpaceIndex must correspond
     * to the index they reside in the vector.
     */
    std::vector<std::unique_ptr<CustomSpaceBase>> custom_spaces;

    /**
     * Specifies whether conservative stack scan is supported. When conservative
     * stack scan is not supported, the collector may try to invoke
     * garbage collections using non-nestable task, which are guaranteed to have
     * no interesting stack, through the provided Platform. If such tasks are
     * not supported by the Platform, the embedder must take care of invoking
     * the GC through ForceGarbageCollectionSlow().
     */
    StackSupport stack_support = StackSupport::kSupportsConservativeStackScan;

    /**
     * Resource constraints specifying various properties that the internal
     * GC scheduler follows.
     */
    ResourceConstraints resource_constraints;
  };

  /**
   * Creates a new heap that can be used for object allocation.
   *
   * \param platform implemented and provided by the embedder.
   * \param options HeapOptions specifying various properties for the Heap.
   * \returns a new Heap instance.
   */
  static std::unique_ptr<Heap> Create(
      std::shared_ptr<Platform> platform,
      HeapOptions options = HeapOptions::Default());

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

  AllocationHandle& GetAllocationHandle();

 private:
  Heap() = default;

  friend class internal::Heap;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_HEAP_H_
