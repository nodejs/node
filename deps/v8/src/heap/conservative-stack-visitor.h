// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_
#define V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_

#include "include/v8-internal.h"
#include "src/common/globals.h"
#include "src/heap/base/stack.h"

namespace v8 {
namespace internal {

class MemoryAllocator;
class RootVisitor;

class V8_EXPORT_PRIVATE ConservativeStackVisitor
    : public ::heap::base::StackVisitor {
 public:
  ConservativeStackVisitor(Isolate* isolate, RootVisitor* delegate);

  void VisitPointer(const void* pointer) final;

  // This method finds an object header based on a `maybe_inner_ptr`. It returns
  // `kNullAddress` if the parameter does not point to (the interior of) a valid
  // heap object. The allocator_ field is used to provide the set of valid heap
  // pages. The collector_ field is used to determine which kind of heap objects
  // we are interested in. For MARK_COMPACTOR all heap objects are considered,
  // whereas for young generation collectors we only consider objects in the
  // young generation.
  Address FindBasePtr(Address maybe_inner_ptr) const;

  static ConservativeStackVisitor ForTesting(Isolate* isolate,
                                             GarbageCollector collector) {
    return ConservativeStackVisitor(isolate, collector);
  }

 private:
  ConservativeStackVisitor(Isolate* isolate, GarbageCollector collector);

  template <bool is_known_to_be_in_cage>
  void VisitConservativelyIfPointer(Address address);

  const PtrComprCageBase cage_base_;
  RootVisitor* const delegate_;
  MemoryAllocator* const allocator_;
  const GarbageCollector collector_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_
