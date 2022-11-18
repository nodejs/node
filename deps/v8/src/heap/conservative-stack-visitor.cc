// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/conservative-stack-visitor.h"

#include "src/execution/isolate-inl.h"
#include "src/heap/mark-compact.h"
#include "src/objects/visitors.h"

#ifdef V8_COMPRESS_POINTERS
#include "src/common/ptr-compr-inl.h"
#endif  // V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

ConservativeStackVisitor::ConservativeStackVisitor(Isolate* isolate,
                                                   RootVisitor* delegate)
    : isolate_(isolate), delegate_(delegate) {}

void ConservativeStackVisitor::VisitPointer(const void* pointer) {
  auto address = reinterpret_cast<Address>(const_cast<void*>(pointer));
  VisitConservativelyIfPointer(address);
#ifdef V8_COMPRESS_POINTERS
  V8HeapCompressionScheme::ProcessIntermediatePointers(
      isolate_, address,
      [this](Address ptr) { VisitConservativelyIfPointer(ptr); });
#endif  // V8_COMPRESS_POINTERS
}

void ConservativeStackVisitor::VisitConservativelyIfPointer(Address address) {
  Address base_ptr;
#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_MB
  base_ptr = isolate_->heap()->mark_compact_collector()->FindBasePtrForMarking(
      address);
#else
#error "Some inner pointer resolution mechanism is needed"
#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_MB
  if (base_ptr == kNullAddress) return;
  HeapObject obj = HeapObject::FromAddress(base_ptr);
  Object root = obj;
  delegate_->VisitRootPointer(Root::kHandleScope, nullptr,
                              FullObjectSlot(&root));
  // Check that the delegate visitor did not modify the root slot.
  DCHECK_EQ(root, obj);
}

}  // namespace internal
}  // namespace v8
