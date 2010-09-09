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

#ifndef V8_DATAFLOW_H_
#define V8_DATAFLOW_H_

#include "v8.h"

#include "ast.h"
#include "compiler.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Node;

class BitVector: public ZoneObject {
 public:
  BitVector() : length_(0), data_length_(0), data_(NULL) { }

  explicit BitVector(int length) {
    ExpandTo(length);
  }

  BitVector(const BitVector& other)
      : length_(other.length()),
        data_length_(SizeFor(length_)),
        data_(Zone::NewArray<uint32_t>(data_length_)) {
    CopyFrom(other);
  }

  void ExpandTo(int length) {
    ASSERT(length > 0);
    length_ = length;
    data_length_ = SizeFor(length);
    data_ = Zone::NewArray<uint32_t>(data_length_);
    Clear();
  }

  BitVector& operator=(const BitVector& rhs) {
    if (this != &rhs) CopyFrom(rhs);
    return *this;
  }

  void CopyFrom(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] = other.data_[i];
    }
  }

  bool Contains(int i) {
    ASSERT(i >= 0 && i < length());
    uint32_t block = data_[i / 32];
    return (block & (1U << (i % 32))) != 0;
  }

  void Add(int i) {
    ASSERT(i >= 0 && i < length());
    data_[i / 32] |= (1U << (i % 32));
  }

  void Remove(int i) {
    ASSERT(i >= 0 && i < length());
    data_[i / 32] &= ~(1U << (i % 32));
  }

  void Union(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] |= other.data_[i];
    }
  }

  void Intersect(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] &= other.data_[i];
    }
  }

  void Subtract(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] &= ~other.data_[i];
    }
  }

  void Clear() {
    for (int i = 0; i < data_length_; i++) {
      data_[i] = 0;
    }
  }

  bool IsEmpty() const {
    for (int i = 0; i < data_length_; i++) {
      if (data_[i] != 0) return false;
    }
    return true;
  }

  bool Equals(const BitVector& other) {
    for (int i = 0; i < data_length_; i++) {
      if (data_[i] != other.data_[i]) return false;
    }
    return true;
  }

  int length() const { return length_; }

#ifdef DEBUG
  void Print();
#endif

 private:
  static int SizeFor(int length) {
    return 1 + ((length - 1) / 32);
  }

  int length_;
  int data_length_;
  uint32_t* data_;
};


// Simple fixed-capacity list-based worklist (managed as a queue) of
// pointers to T.
template<typename T>
class WorkList BASE_EMBEDDED {
 public:
  // The worklist cannot grow bigger than size.  We keep one item empty to
  // distinguish between empty and full.
  explicit WorkList(int size)
      : capacity_(size + 1), head_(0), tail_(0), queue_(capacity_) {
    for (int i = 0; i < capacity_; i++) queue_.Add(NULL);
  }

  bool is_empty() { return head_ == tail_; }

  bool is_full() {
    // The worklist is full if head is at 0 and tail is at capacity - 1:
    //   head == 0 && tail == capacity-1 ==> tail - head == capacity - 1
    // or if tail is immediately to the left of head:
    //   tail+1 == head  ==> tail - head == -1
    int diff = tail_ - head_;
    return (diff == -1 || diff == capacity_ - 1);
  }

  void Insert(T* item) {
    ASSERT(!is_full());
    queue_[tail_++] = item;
    if (tail_ == capacity_) tail_ = 0;
  }

  T* Remove() {
    ASSERT(!is_empty());
    T* item = queue_[head_++];
    if (head_ == capacity_) head_ = 0;
    return item;
  }

 private:
  int capacity_;  // Including one empty slot.
  int head_;      // Where the first item is.
  int tail_;      // Where the next inserted item will go.
  List<T*> queue_;
};


// Computes the set of assigned variables and annotates variables proxies
// that are trivial sub-expressions and for-loops where the loop variable
// is guaranteed to be a smi.
class AssignedVariablesAnalyzer : public AstVisitor {
 public:
  explicit AssignedVariablesAnalyzer(FunctionLiteral* fun) : fun_(fun) { }
  bool Analyze();

 private:
  Variable* FindSmiLoopVariable(ForStatement* stmt);

  int BitIndex(Variable* var);

  void RecordAssignedVar(Variable* var);

  void MarkIfTrivial(Expression* expr);

  // Visits an expression saving the accumulator before, clearing
  // it before visting and restoring it after visiting.
  void ProcessExpression(Expression* expr);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  FunctionLiteral* fun_;

  // Accumulator for assigned variables set.
  BitVector av_;

  DISALLOW_COPY_AND_ASSIGN(AssignedVariablesAnalyzer);
};


} }  // namespace v8::internal


#endif  // V8_DATAFLOW_H_
