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
  // Iterator for the elements of this BitVector.
  class Iterator BASE_EMBEDDED {
   public:
    explicit Iterator(BitVector* target)
        : target_(target),
          current_index_(0),
          current_value_(target->data_[0]),
          current_(-1) {
      ASSERT(target->data_length_ > 0);
      Advance();
    }
    ~Iterator() { }

    bool Done() const { return current_index_ >= target_->data_length_; }
    void Advance();

    int Current() const {
      ASSERT(!Done());
      return current_;
    }

   private:
    uint32_t SkipZeroBytes(uint32_t val) {
      while ((val & 0xFF) == 0) {
        val >>= 8;
        current_ += 8;
      }
      return val;
    }
    uint32_t SkipZeroBits(uint32_t val) {
      while ((val & 0x1) == 0) {
        val >>= 1;
        current_++;
      }
      return val;
    }

    BitVector* target_;
    int current_index_;
    uint32_t current_value_;
    int current_;

    friend class BitVector;
  };

  explicit BitVector(int length)
      : length_(length),
        data_length_(SizeFor(length)),
        data_(Zone::NewArray<uint32_t>(data_length_)) {
    ASSERT(length > 0);
    Clear();
  }

  BitVector(const BitVector& other)
      : length_(other.length()),
        data_length_(SizeFor(length_)),
        data_(Zone::NewArray<uint32_t>(data_length_)) {
    CopyFrom(other);
  }

  static int SizeFor(int length) {
    return 1 + ((length - 1) / 32);
  }

  BitVector& operator=(const BitVector& rhs) {
    if (this != &rhs) CopyFrom(rhs);
    return *this;
  }

  void CopyFrom(const BitVector& other) {
    ASSERT(other.length() <= length());
    for (int i = 0; i < other.data_length_; i++) {
      data_[i] = other.data_[i];
    }
    for (int i = other.data_length_; i < data_length_; i++) {
      data_[i] = 0;
    }
  }

  bool Contains(int i) const {
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

  bool UnionIsChanged(const BitVector& other) {
    ASSERT(other.length() == length());
    bool changed = false;
    for (int i = 0; i < data_length_; i++) {
      uint32_t old_data = data_[i];
      data_[i] |= other.data_[i];
      if (data_[i] != old_data) changed = true;
    }
    return changed;
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
  int length_;
  int data_length_;
  uint32_t* data_;
};


// An implementation of a sparse set whose elements are drawn from integers
// in the range [0..universe_size[.  It supports constant-time Contains,
// destructive Add, and destructuve Remove operations and linear-time (in
// the number of elements) destructive Union.
class SparseSet: public ZoneObject {
 public:
  // Iterator for sparse set elements.  Elements should not be added or
  // removed during iteration.
  class Iterator BASE_EMBEDDED {
   public:
    explicit Iterator(SparseSet* target) : target_(target), current_(0) {
      ASSERT(++target->iterator_count_ > 0);
    }
    ~Iterator() {
      ASSERT(target_->iterator_count_-- > 0);
    }
    bool Done() const { return current_ >= target_->dense_.length(); }
    void Advance() {
      ASSERT(!Done());
      ++current_;
    }
    int Current() {
      ASSERT(!Done());
      return target_->dense_[current_];
    }

   private:
    SparseSet* target_;
    int current_;

    friend class SparseSet;
  };

  explicit SparseSet(int universe_size)
      : dense_(4),
        sparse_(Zone::NewArray<int>(universe_size)) {
#ifdef DEBUG
    size_ = universe_size;
    iterator_count_ = 0;
#endif
  }

  bool Contains(int n) const {
    ASSERT(0 <= n && n < size_);
    int dense_index = sparse_[n];
    return (0 <= dense_index) &&
        (dense_index < dense_.length()) &&
        (dense_[dense_index] == n);
  }

  void Add(int n) {
    ASSERT(0 <= n && n < size_);
    ASSERT(iterator_count_ == 0);
    if (!Contains(n)) {
      sparse_[n] = dense_.length();
      dense_.Add(n);
    }
  }

  void Remove(int n) {
    ASSERT(0 <= n && n < size_);
    ASSERT(iterator_count_ == 0);
    if (Contains(n)) {
      int dense_index = sparse_[n];
      int last = dense_.RemoveLast();
      if (dense_index < dense_.length()) {
        dense_[dense_index] = last;
        sparse_[last] = dense_index;
      }
    }
  }

  void Union(const SparseSet& other) {
    for (int i = 0; i < other.dense_.length(); ++i) {
      Add(other.dense_[i]);
    }
  }

 private:
  // The set is implemented as a pair of a growable dense list and an
  // uninitialized sparse array.
  ZoneList<int> dense_;
  int* sparse_;
#ifdef DEBUG
  int size_;
  int iterator_count_;
#endif
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
  static bool Analyze(CompilationInfo* info);

 private:
  AssignedVariablesAnalyzer(CompilationInfo* info, int bits);
  bool Analyze();

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

  CompilationInfo* info_;

  // Accumulator for assigned variables set.
  BitVector av_;

  DISALLOW_COPY_AND_ASSIGN(AssignedVariablesAnalyzer);
};


} }  // namespace v8::internal


#endif  // V8_DATAFLOW_H_
