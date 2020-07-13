// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_VISITOR_H_
#define V8_HEAP_CPPGC_VISITOR_H_

#include "include/cppgc/visitor.h"

namespace cppgc {
namespace internal {

// Base visitor that is allowed to create a public cppgc::Visitor object and
// use its internals.
class VisitorBase : public cppgc::Visitor {
 public:
  VisitorBase() = default;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_VISITOR_H_
