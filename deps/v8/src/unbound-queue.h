// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNBOUND_QUEUE_
#define V8_UNBOUND_QUEUE_

#include "allocation.h"

namespace v8 {
namespace internal {


// Lock-free unbound queue for small records.  Intended for
// transferring small records between a Single producer and a Single
// consumer. Doesn't have restrictions on the number of queued
// elements, so producer never blocks.  Implemented after Herb
// Sutter's article:
// http://www.ddj.com/high-performance-computing/210604448
template<typename Record>
class UnboundQueue BASE_EMBEDDED {
 public:
  inline UnboundQueue();
  inline ~UnboundQueue();

  INLINE(bool Dequeue(Record* rec));
  INLINE(void Enqueue(const Record& rec));
  INLINE(bool IsEmpty() const);
  INLINE(Record* Peek() const);

 private:
  INLINE(void DeleteFirst());

  struct Node;

  Node* first_;
  AtomicWord divider_;  // Node*
  AtomicWord last_;     // Node*

  DISALLOW_COPY_AND_ASSIGN(UnboundQueue);
};


} }  // namespace v8::internal

#endif  // V8_UNBOUND_QUEUE_
