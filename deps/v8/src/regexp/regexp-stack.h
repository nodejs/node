// Copyright 2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_STACK_H_
#define V8_REGEXP_REGEXP_STACK_H_

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class RegExpStack;

// Maintains a per-v8thread stack area that can be used by irregexp
// implementation for its backtracking stack.
// Since there is only one stack area, the Irregexp implementation is not
// re-entrant. I.e., no regular expressions may be executed in the same thread
// during a preempted Irregexp execution.
class V8_NODISCARD RegExpStackScope {
 public:
  // Create and delete an instance to control the life-time of a growing stack.

  // Initializes the stack memory area if necessary.
  explicit RegExpStackScope(Isolate* isolate);
  ~RegExpStackScope();  // Releases the stack if it has grown.
  RegExpStackScope(const RegExpStackScope&) = delete;
  RegExpStackScope& operator=(const RegExpStackScope&) = delete;

  RegExpStack* stack() const { return regexp_stack_; }

 private:
  RegExpStack* regexp_stack_;
};

class RegExpStack {
 public:
  RegExpStack();
  ~RegExpStack();
  RegExpStack(const RegExpStack&) = delete;
  RegExpStack& operator=(const RegExpStack&) = delete;

  // Number of allocated locations on the stack below the limit.
  // No sequence of pushes must be longer that this without doing a stack-limit
  // check.
  static constexpr int kStackLimitSlack = 32;

  // Gives the top of the memory used as stack.
  Address stack_base() {
    DCHECK_NE(0, thread_local_.memory_size_);
    DCHECK_EQ(thread_local_.memory_top_,
              thread_local_.memory_ + thread_local_.memory_size_);
    return reinterpret_cast<Address>(thread_local_.memory_top_);
  }

  // The total size of the memory allocated for the stack.
  size_t stack_capacity() { return thread_local_.memory_size_; }

  // If the stack pointer gets below the limit, we should react and
  // either grow the stack or report an out-of-stack exception.
  // There is only a limited number of locations below the stack limit,
  // so users of the stack should check the stack limit during any
  // sequence of pushes longer that this.
  Address* limit_address_address() { return &(thread_local_.limit_); }

  // Ensures that there is a memory area with at least the specified size.
  // If passing zero, the default/minimum size buffer is allocated.
  Address EnsureCapacity(size_t size);

  bool is_in_use() const { return thread_local_.is_in_use_; }
  void set_is_in_use(bool v) { thread_local_.is_in_use_ = v; }

  // Thread local archiving.
  static constexpr int ArchiveSpacePerThread() {
    return static_cast<int>(kThreadLocalSize);
  }
  char* ArchiveStack(char* to);
  char* RestoreStack(char* from);
  void FreeThreadResources() { thread_local_.ResetToStaticStack(this); }

  // Maximal size of allocated stack area.
  static constexpr size_t kMaximumStackSize = 64 * MB;

 private:
  // Artificial limit used when the thread-local state has been destroyed.
  static const Address kMemoryTop =
      static_cast<Address>(static_cast<uintptr_t>(-1));

  // Minimal size of dynamically-allocated stack area.
  static constexpr size_t kMinimumDynamicStackSize = 1 * KB;

  // In addition to dynamically-allocated, variable-sized stacks, we also have
  // a statically allocated and sized area that is used whenever no dynamic
  // stack is allocated. This guarantees that a stack is always available and
  // we can skip availability-checks later on.
  // It's double the slack size to ensure that we have a bit of breathing room
  // before NativeRegExpMacroAssembler::GrowStack must be called.
  static constexpr size_t kStaticStackSize =
      2 * kStackLimitSlack * kSystemPointerSize;
  byte static_stack_[kStaticStackSize] = {0};

  STATIC_ASSERT(kStaticStackSize <= kMaximumStackSize);

  // Structure holding the allocated memory, size and limit.
  struct ThreadLocal {
    explicit ThreadLocal(RegExpStack* regexp_stack) {
      ResetToStaticStack(regexp_stack);
    }

    // If memory_size_ > 0 then memory_ and memory_top_ must be non-nullptr
    // and memory_top_ = memory_ + memory_size_
    byte* memory_ = nullptr;
    byte* memory_top_ = nullptr;
    size_t memory_size_ = 0;
    Address limit_ = kNullAddress;
    bool owns_memory_ = false;  // Whether memory_ is owned and must be freed.
    bool is_in_use_ = false;    // To guard against reentrancy.

    void ResetToStaticStack(RegExpStack* regexp_stack);
    void FreeAndInvalidate();
  };
  static constexpr size_t kThreadLocalSize = sizeof(ThreadLocal);

  // Address of top of memory used as stack.
  Address memory_top_address_address() {
    return reinterpret_cast<Address>(&thread_local_.memory_top_);
  }

  // Resets the buffer if it has grown beyond the default/minimum size.
  // After this, the buffer is either the default size, or it is empty, so
  // you have to call EnsureCapacity before using it again.
  void Reset();

  ThreadLocal thread_local_;
  Isolate* isolate_;

  friend class ExternalReference;
  friend class Isolate;
  friend class RegExpStackScope;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_STACK_H_
