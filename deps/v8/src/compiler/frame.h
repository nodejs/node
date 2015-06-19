// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FRAME_H_
#define V8_COMPILER_FRAME_H_

#include "src/bit-vector.h"

namespace v8 {
namespace internal {
namespace compiler {

// Collects the spill slot requirements and the allocated general and double
// registers for a compiled function. Frames are usually populated by the
// register allocator and are used by Linkage to generate code for the prologue
// and epilogue to compiled code.
class Frame : public ZoneObject {
 public:
  Frame()
      : register_save_area_size_(0),
        spill_slot_count_(0),
        osr_stack_slot_count_(0),
        allocated_registers_(NULL),
        allocated_double_registers_(NULL) {}

  inline int GetSpillSlotCount() { return spill_slot_count_; }

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

  // OSR stack slots, including locals and expression stack slots.
  void SetOsrStackSlotCount(int slots) {
    DCHECK(slots >= 0);
    osr_stack_slot_count_ = slots;
  }

  int GetOsrStackSlotCount() { return osr_stack_slot_count_; }

  int AllocateSpillSlot(int width) {
    DCHECK(width == 4 || width == 8);
    // Skip one slot if necessary.
    if (width > kPointerSize) {
      DCHECK(width == kPointerSize * 2);
      spill_slot_count_++;
      spill_slot_count_ |= 1;
    }
    return spill_slot_count_++;
  }

  void ReserveSpillSlots(size_t slot_count) {
    DCHECK_EQ(0, spill_slot_count_);  // can only reserve before allocation.
    spill_slot_count_ = static_cast<int>(slot_count);
  }

 private:
  int register_save_area_size_;
  int spill_slot_count_;
  int osr_stack_slot_count_;
  BitVector* allocated_registers_;
  BitVector* allocated_double_registers_;

  DISALLOW_COPY_AND_ASSIGN(Frame);
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
