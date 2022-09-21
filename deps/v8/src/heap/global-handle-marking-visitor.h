// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GLOBAL_HANDLE_MARKING_VISITOR_H_
#define V8_HEAP_GLOBAL_HANDLE_MARKING_VISITOR_H_

#include "src/handles/global-handles.h"
#include "src/heap/base/stack.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact.h"

namespace v8 {
namespace internal {

// Root marking visitor for conservatively marking traced global handles.
// The visitor assumes that on-stack pointers may point into global handle nodes
// which requires them to be kept alive.
class GlobalHandleMarkingVisitor final : public ::heap::base::StackVisitor {
 public:
  GlobalHandleMarkingVisitor(Heap&, MarkingState&, MarkingWorklists::Local&);
  ~GlobalHandleMarkingVisitor() override = default;

  void VisitPointer(const void*) override;

 private:
  Heap& heap_;
  MarkingState& marking_state_;
  MarkingWorklists::Local& local_marking_worklist_;
  GlobalHandles::NodeBounds traced_node_bounds_;
};

#endif  // V8_HEAP_GLOBAL_HANDLE_MARKING_VISITOR_H_

}  // namespace internal
}  // namespace v8
