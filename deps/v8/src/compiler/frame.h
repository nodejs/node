// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FRAME_H_
#define V8_COMPILER_FRAME_H_

#include "src/v8.h"

#include "src/data-flow.h"

namespace v8 {
namespace internal {
namespace compiler {

// Collects the spill slot requirements and the allocated general and double
// registers for a compiled function. Frames are usually populated by the
// register allocator and are used by Linkage to generate code for the prologue
// and epilogue to compiled code.
class Frame {
 public:
  Frame()
      : register_save_area_size_(0),
        spill_slot_count_(0),
        double_spill_slot_count_(0),
        allocated_registers_(NULL),
        allocated_double_registers_(NULL) {}

  inline int GetSpillSlotCount() { return spill_slot_count_; }
  inline int GetDoubleSpillSlotCount() { return double_spill_slot_count_; }

  void SetAllocatedRegisters(BitVector* regs) {
    DCHECK(allocated_registers_ == NULL);
    allocated_registers_ = regs;
  }

  void SetAllocatedDoubleRegisters(BitVector* regs) {
    DCHECK(allocated_double_registers_ == NULL);
    allocated_double_registers_ = regs;
  }

  bool DidAllocateDoubleRegisters() {
    return !allocated_double_registers_->IsEmpty();
  }

  void SetRegisterSaveAreaSize(int size) {
    DCHECK(IsAligned(size, kPointerSize));
    register_save_area_size_ = size;
  }

  int GetRegisterSaveAreaSize() { return register_save_area_size_; }

  int AllocateSpillSlot(bool is_double) {
    // If 32-bit, skip one if the new slot is a double.
    if (is_double) {
      if (kDoubleSize > kPointerSize) {
        DCHECK(kDoubleSize == kPointerSize * 2);
        spill_slot_count_++;
        spill_slot_count_ |= 1;
      }
      double_spill_slot_count_++;
    }
    return spill_slot_count_++;
  }

 private:
  int register_save_area_size_;
  int spill_slot_count_;
  int double_spill_slot_count_;
  BitVector* allocated_registers_;
  BitVector* allocated_double_registers_;
};


// Represents an offset from either the stack pointer or frame pointer.
class FrameOffset {
 public:
  inline bool from_stack_pointer() { return (offset_ & 1) == kFromSp; }
  inline bool from_frame_pointer() { return (offset_ & 1) == kFromFp; }
  inline int offset() { return offset_ & ~1; }

  inline static FrameOffset FromStackPointer(int offset) {
    DCHECK((offset & 1) == 0);
    return FrameOffset(offset | kFromSp);
  }

  inline static FrameOffset FromFramePointer(int offset) {
    DCHECK((offset & 1) == 0);
    return FrameOffset(offset | kFromFp);
  }

 private:
  explicit FrameOffset(int offset) : offset_(offset) {}

  int offset_;  // Encodes SP or FP in the low order bit.

  static const int kFromSp = 1;
  static const int kFromFp = 0;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_FRAME_H_
