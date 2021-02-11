// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_CPPGC_H_
#define INCLUDE_V8_CPPGC_H_

#include "cppgc/visitor.h"
#include "v8-internal.h"  // NOLINT(build/include_directory)
#include "v8.h"  // NOLINT(build/include_directory)

namespace v8 {

class JSVisitor : public cppgc::Visitor {
 public:
  explicit JSVisitor(cppgc::Visitor::Key key) : cppgc::Visitor(key) {}

  void Trace(const TracedReferenceBase& ref) {
    if (ref.IsEmptyThreadSafe()) return;
    Visit(ref);
  }

 protected:
  using cppgc::Visitor::Visit;

  virtual void Visit(const TracedReferenceBase& ref) {}
};

}  // namespace v8

namespace cppgc {

template <typename T>
struct TraceTrait<v8::TracedReference<T>> {
  static void Trace(Visitor* visitor, const v8::TracedReference<T>* self) {
    static_cast<v8::JSVisitor*>(visitor)->Trace(*self);
  }
};

}  // namespace cppgc

#endif  // INCLUDE_V8_CPPGC_H_
