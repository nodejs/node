// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_
#define V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_

#include "src/heap/base/stack.h"
#include "src/heap/memory-chunk.h"

namespace v8 {
namespace internal {

class ConservativeStackVisitor : public ::heap::base::StackVisitor {
 public:
  ConservativeStackVisitor(Isolate* isolate, RootVisitor* delegate);

  void VisitPointer(const void* pointer) final;

 private:
  bool CheckPage(Address address, MemoryChunk* page);

  void VisitConservativelyIfPointer(const void* pointer);

  Isolate* isolate_ = nullptr;
  RootVisitor* delegate_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_
