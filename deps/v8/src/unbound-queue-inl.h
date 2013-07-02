// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_UNBOUND_QUEUE_INL_H_
#define V8_UNBOUND_QUEUE_INL_H_

#include "unbound-queue.h"

#include "atomicops.h"

namespace v8 {
namespace internal {

template<typename Record>
struct UnboundQueue<Record>::Node: public Malloced {
  explicit Node(const Record& value)
      : value(value), next(NULL) {
  }

  Record value;
  Node* next;
};


template<typename Record>
UnboundQueue<Record>::UnboundQueue() {
  first_ = new Node(Record());
  divider_ = last_ = reinterpret_cast<AtomicWord>(first_);
}


template<typename Record>
UnboundQueue<Record>::~UnboundQueue() {
  while (first_ != NULL) DeleteFirst();
}


template<typename Record>
void UnboundQueue<Record>::DeleteFirst() {
  Node* tmp = first_;
  first_ = tmp->next;
  delete tmp;
}


template<typename Record>
bool UnboundQueue<Record>::Dequeue(Record* rec) {
  if (divider_ == Acquire_Load(&last_)) return false;
  Node* next = reinterpret_cast<Node*>(divider_)->next;
  *rec = next->value;
  Release_Store(&divider_, reinterpret_cast<AtomicWord>(next));
  return true;
}


template<typename Record>
void UnboundQueue<Record>::Enqueue(const Record& rec) {
  Node*& next = reinterpret_cast<Node*>(last_)->next;
  next = new Node(rec);
  Release_Store(&last_, reinterpret_cast<AtomicWord>(next));

  while (first_ != reinterpret_cast<Node*>(Acquire_Load(&divider_))) {
    DeleteFirst();
  }
}


template<typename Record>
bool UnboundQueue<Record>::IsEmpty() const {
  return NoBarrier_Load(&divider_) == NoBarrier_Load(&last_);
}


template<typename Record>
Record* UnboundQueue<Record>::Peek() const {
  if (divider_ == Acquire_Load(&last_)) return NULL;
  Node* next = reinterpret_cast<Node*>(divider_)->next;
  return &next->value;
}

} }  // namespace v8::internal

#endif  // V8_UNBOUND_QUEUE_INL_H_
