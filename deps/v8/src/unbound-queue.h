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

#ifndef V8_UNBOUND_QUEUE_
#define V8_UNBOUND_QUEUE_

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

  INLINE(void Dequeue(Record* rec));
  INLINE(void Enqueue(const Record& rec));
  INLINE(bool IsEmpty()) { return divider_ == last_; }
  INLINE(Record* Peek());

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
