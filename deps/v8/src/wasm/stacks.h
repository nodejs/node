// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_STACKS_H_
#define V8_WASM_STACKS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <optional>

#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/objects/visitors.h"
#include "src/utils/allocation.h"
#include "src/wasm/value-type.h"

namespace v8 {
class Isolate;
}

namespace v8::internal::wasm {

class StackMemory;

struct JumpBuffer {
  Address sp;
  Address fp;
  Address pc;
  void* stack_limit;
  StackMemory* parent = nullptr;

  // We track the state below to prevent stack corruptions under the sandbox
  // security model.
  // Assuming that the external pointer to the jump buffer has been corrupted
  // and replaced with a different jump buffer, we check its state before
  // resuming it to verify that it is not Active or Retired.
  // The distinction between Suspended and Inactive may not be strictly
  // necessary since we currently always pass a single JS value in the return
  // register across stacks (either the Promise, the result of the Promise, or
  // the result of the export). However adding a state does not cost anything
  // and is more robust against potential changes in the calling conventions.
  enum StackState : int32_t {
    Active,     // The (unique) active stack. The jump buffer is invalid in that
                // state.
    Suspended,  // A stack suspended by WasmSuspend.
    Inactive,   // A parent/ancestor of the active stack. In other words, a
                // stack that either called or resumed a suspendable stack.
    Retired     // A finished stack. The jump buffer is invalid in that state.
  };
  StackState state;
};

class StackMemory {
 public:
  static std::unique_ptr<StackMemory> New() {
    return std::unique_ptr<StackMemory>(new StackMemory());
  }

  // Returns a non-owning view of the central stack. This may be
  // the simulator's stack when running on the simulator.
  static StackMemory* GetCentralStackView(Isolate* isolate);

  ~StackMemory();
  void* jslimit() const;
  Address base() const {
    Address memory_limit = active_segment_
                               ? active_segment_->base()
                               : reinterpret_cast<Address>(limit_ + size_);
#ifdef USE_SIMULATOR
    // To perform runtime calls with different signatures, some simulators
    // prepare a fixed number of arguments which is an upper bound of the actual
    // parameter count. The extra stack slots contain arbitrary data and are
    // never used, but with stack-switching this can happen close to the stack
    // start, so we need to reserve a safety gap to ensure that the addresses
    // are at least mapped to prevent a crash.
    constexpr int kStackBaseSafetyOffset = 20 * kSystemPointerSize;
#else
    constexpr int kStackBaseSafetyOffset = 0;
#endif
    return memory_limit - kStackBaseSafetyOffset;
  }
  bool IsValidContinuation(Tagged<WasmContinuationObject> cont);
  JumpBuffer* jmpbuf() { return &jmpbuf_; }
  bool Contains(Address addr) {
    if (!owned_) {
      return reinterpret_cast<Address>(jslimit()) <= addr && addr < base();
    }
    for (auto segment = first_segment_; segment;
         segment = segment->next_segment_) {
      if (reinterpret_cast<Address>(segment->limit_) <= addr &&
          addr < segment->base()) {
        return true;
      }
      if (segment == active_segment_) break;
    }
    return false;
  }
  int id() { return id_; }
  bool IsActive() { return jmpbuf_.state == JumpBuffer::Active; }
  void set_index(size_t index) { index_ = index; }
  size_t index() { return index_; }
  size_t allocated_size() {
    size_t size = 0;
    auto segment = first_segment_;
    while (segment) {
      size += segment->size_;
      segment = segment->next_segment_;
    }
    return size;
  }
  void FillWith(uint8_t value) {
    auto segment = first_segment_;
    while (segment) {
      memset(segment->limit_, value, segment->size_);
      segment = segment->next_segment_;
    }
  }
  void Iterate(v8::internal::RootVisitor* v, Isolate* isolate);

  Address old_fp() { return active_segment_->old_fp; }
  bool Grow(Address current_fp, size_t min_size);
  Address Shrink();
  void ShrinkTo(Address stack_address);
  void Reset();

  class StackSegment {
   public:
    Address base() const { return reinterpret_cast<Address>(limit_ + size_); }

   private:
    explicit StackSegment(size_t size);
    ~StackSegment();
    uint8_t* limit_;
    size_t size_;

    // References to segments of segmented stack
    StackSegment* next_segment_ = nullptr;
    StackSegment* prev_segment_ = nullptr;
    Address old_fp = 0;

    friend class StackMemory;
  };

  struct StackSwitchInfo {
    // Source FP and target SP of the frame that switched to the central stack.
    // The source FP is in the secondary stack, the target SP is in the central
    // stack.
    // The stack cannot be suspended while it is on the central stack, so there
    // can be at most one switch for a given stack.
    Address source_fp = kNullAddress;
    Address target_sp = kNullAddress;
    bool has_value() const { return source_fp != kNullAddress; }
  };
  const StackSwitchInfo& stack_switch_info() const {
    return stack_switch_info_;
  }
  void set_stack_switch_info(Address fp, Address sp) {
    stack_switch_info_ = {fp, sp};
  }
  void clear_stack_switch_info() {
    stack_switch_info_.source_fp = kNullAddress;
  }

  static int JSStackLimitMarginKB() {
    if (v8_flags.experimental_wasm_growable_stacks) {
      // The limiting factor for this margin is the stack space used by outgoing
      // stack parameters in wasm. They can take up to 16KB (1000 simd
      // parameters, minus register parameters) and are not taken into account
      // by stack checks.
      // TODO(42204615): look into changing the stack check to take outgoing
      // stack parameters into account.
      static_assert(kMaxValueTypeSize == 16);
      static_assert(kV8MaxWasmFunctionParams == 1000);
      return 20;
    } else {
      return DEBUG_BOOL ? 80 : 40;
    }
  }

  friend class StackPool;

  constexpr static uint32_t stack_switch_source_fp_offset() {
    return OFFSET_OF(StackMemory, stack_switch_info_) +
           OFFSET_OF(StackMemory::StackSwitchInfo, source_fp);
  }
  constexpr static uint32_t stack_switch_target_sp_offset() {
    return OFFSET_OF(StackMemory, stack_switch_info_) +
           OFFSET_OF(StackMemory::StackSwitchInfo, target_sp);
  }
  constexpr static uint32_t jmpbuf_offset() {
    return OFFSET_OF(StackMemory, jmpbuf_);
  }

 private:
  // This constructor allocates a new stack segment.
  StackMemory();

  // Overload to represent a view of the libc stack.
  StackMemory(uint8_t* limit, size_t size);

  uint8_t* limit_;
  size_t size_;
  bool owned_;
  JumpBuffer jmpbuf_;
  // Stable ID.
  int id_;
  // Index of this stack in the global Isolate::wasm_stacks() vector. This
  // allows us to add and remove from the vector in constant time (see
  // return_switch()).
  size_t index_;
  StackSwitchInfo stack_switch_info_;
  StackSegment* first_segment_ = nullptr;
  StackSegment* active_segment_ = nullptr;
  Tagged<WasmContinuationObject> current_cont_ = {};
};

constexpr int kStackSpOffset =
    wasm::StackMemory::jmpbuf_offset() + offsetof(JumpBuffer, sp);
constexpr int kStackFpOffset =
    wasm::StackMemory::jmpbuf_offset() + offsetof(JumpBuffer, fp);
constexpr int kStackPcOffset =
    wasm::StackMemory::jmpbuf_offset() + offsetof(JumpBuffer, pc);
constexpr int kStackLimitOffset =
    wasm::StackMemory::jmpbuf_offset() + offsetof(JumpBuffer, stack_limit);
constexpr int kStackParentOffset =
    wasm::StackMemory::jmpbuf_offset() + offsetof(JumpBuffer, parent);
constexpr int kStackStateOffset =
    wasm::StackMemory::jmpbuf_offset() + offsetof(JumpBuffer, state);

// A pool of "finished" stacks, i.e. stacks whose last frame have returned and
// whose memory can be reused for new suspendable computations.
class StackPool {
 public:
  // Gets a stack from the free list if one exists, else allocates it.
  std::unique_ptr<StackMemory> GetOrAllocate();
  // Adds a finished stack to the free list.
  void Add(std::unique_ptr<StackMemory> stack);
  // Decommit the stack memories and empty the freelist.
  void ReleaseFinishedStacks();
  size_t Size() const;

 private:
  std::vector<std::unique_ptr<StackMemory>> freelist_;
  size_t size_ = 0;
  // If the next finished stack would move the total size above this limit, the
  // stack is freed instead of being added to the free list.
  static constexpr int kMaxSize = 4 * MB;
};

}  // namespace v8::internal::wasm

#endif  // V8_WASM_STACKS_H_
