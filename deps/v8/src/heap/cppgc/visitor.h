// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_VISITOR_H_
#define V8_HEAP_CPPGC_VISITOR_H_

#include "include/cppgc/visitor.h"
#include "src/heap/cppgc/heap-object-header.h"

namespace cppgc {
namespace internal {

class HeapBase;
class HeapObjectHeader;
class PageBackend;

class VisitorFactory {
 public:
  static constexpr Visitor::Key CreateKey() { return {}; }
};

// Base visitor that is allowed to create a public cppgc::Visitor object and
// use its internals.
class VisitorBase : public cppgc::Visitor {
 public:
  VisitorBase() : cppgc::Visitor(VisitorFactory::CreateKey()) {}
  ~VisitorBase() override = default;

  VisitorBase(const VisitorBase&) = delete;
  VisitorBase& operator=(const VisitorBase&) = delete;

  template <typename Persistent>
  void TraceRootForTesting(const Persistent& p, const SourceLocation& loc) {
    TraceRoot(p, loc);
  }
};

// Regular visitor that additionally allows for conservative tracing.
class ConservativeTracingVisitor {
 public:
  ConservativeTracingVisitor(HeapBase&, PageBackend&, cppgc::Visitor&);
  virtual ~ConservativeTracingVisitor() = default;

  ConservativeTracingVisitor(const ConservativeTracingVisitor&) = delete;
  ConservativeTracingVisitor& operator=(const ConservativeTracingVisitor&) =
      delete;

  void TraceConservativelyIfNeeded(const void*);
  void TraceConservativelyIfNeeded(HeapObjectHeader&);

 protected:
  using TraceConservativelyCallback = void(ConservativeTracingVisitor*,
                                           const HeapObjectHeader&);
  virtual void V8_EXPORT_PRIVATE
  VisitFullyConstructedConservatively(HeapObjectHeader&);
  virtual void VisitInConstructionConservatively(HeapObjectHeader&,
                                                 TraceConservativelyCallback) {}

  HeapBase& heap_;
  PageBackend& page_backend_;
  cppgc::Visitor& visitor_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_VISITOR_H_
