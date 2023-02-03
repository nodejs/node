// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_
#define V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_

#include "include/v8-internal.h"
#include "src/heap/base/stack.h"

namespace v8 {
namespace internal {

class RootVisitor;

class V8_EXPORT_PRIVATE ConservativeStackVisitor
    : public ::heap::base::StackVisitor {
 public:
  ConservativeStackVisitor(Isolate* isolate, RootVisitor* delegate);

  void VisitPointer(const void* pointer) final;

 private:
  void VisitConservativelyIfPointer(Address address);

  Isolate* isolate_ = nullptr;
  RootVisitor* delegate_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONSERVATIVE_STACK_VISITOR_H_
