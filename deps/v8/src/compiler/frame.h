// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FRAME_H_
#define V8_COMPILER_FRAME_H_

#include "src/bit-vector.h"
#include "src/frame-constants.h"

namespace v8 {
namespace internal {
namespace compiler {

class CallDescriptor;

// Collects the spill slot and other frame slot requirements for a compiled
// function. Frames are usually populated by the register allocator and are used
// by Linkage to generate code for the prologue and epilogue to compiled
// code. Frame objects must be considered immutable once they've been
// instantiated and the basic information about the frame has been collected
// into them. Mutable state associated with the frame is stored separately in
// FrameAccessState.
//
// Frames are divided up into four regions.
// - The first is the fixed header, which always has a constant size and can be
//   predicted before code generation begins depending on the type of code being
//   generated.
// - The second is the region for spill slots, which is immediately below the
//   fixed header and grows as the register allocator needs to spill to the
//   stack and asks the frame for more space.
// - The third region, which contains the callee-saved registers must be
//   reserved after register allocation, since its size can only be precisely
//   determined after register allocation once the number of used callee-saved
//   register is certain.
// - The fourth region is a scratch area for return values from other functions
//   called, if multiple returns cannot all be passed in registers. This region
//   Must be last in a stack frame, so that it is positioned immediately below
//   the stack frame of a callee to store to.
//
// The frame region immediately below the fixed header contains spill slots
// starting at slot 4 for JSFunctions.  The callee-saved frame region below that
// starts at 4+spill_slot_count_.  Callee stack slots correspond to
// parameters that are accessible through negative slot ids.
//
// Every slot of a caller or callee frame is accessible by the register
// allocator and gap resolver with a SpillSlotOperand containing its
// corresponding slot id.
//
// Below an example JSFunction Frame with slot ids, frame regions and contents:
//
//  slot      JS frame
//       +-----------------+--------------------------------
//  -n-1 |   parameter 0   |                            ^
//       |- - - - - - - - -|                            |
//  -n   |                 |                          Caller
//  ...  |       ...       |                       frame slots
//  -2   |  parameter n-1  |                       (slot < 0)
//       |- - - - - - - - -|                            |
//  -1   |   parameter n   |                            v
//  -----+-----------------+--------------------------------
//   0   |   return addr   |   ^                        ^
//       |- - - - - - - - -|   |                        |
//   1   | saved frame ptr | Fixed                      |
//       |- - - - - - - - -| Header <-- frame ptr       |
//   2   |Context/Frm. Type|   |                        |
//       |- - - - - - - - -|   |                        |
//   3   |   [JSFunction]  |   v                        |
//       +-----------------+----                        |
//   4   |    spill 1      |   ^                      Callee
//       |- - - - - - - - -|   |                   frame slots
//  ...  |      ...        | Spill slots           (slot >= 0)
//       |- - - - - - - - -|   |                        |
//  m+3  |    spill m      |   v                        |
//       +-----------------+----                        |
//  m+4  |  callee-saved 1 |   ^                        |
//       |- - - - - - - - -|   |                        |
//       |      ...        | Callee-saved               |
//       |- - - - - - - - -|   |                        |
// m+r+3 |  callee-saved r |   v                        |
//       +-----------------+----                        |
// m+r+4 |    return 0     |   ^                        |
//       |- - - - - - - - -|   |                        |
//       |      ...        | Return                     |
//       |- - - - - - - - -|   |                        |
//       |    return q-1   |   v                        v
//  -----+-----------------+----- <-- stack ptr -------------
//
class Frame : public ZoneObject {
 public:
  explicit Frame(int fixed_frame_size_in_slots);

  inline int GetTotalFrameSlotCount() const { return frame_slot_count_; }
  inline int GetFixedSlotCount() const { return fixed_slot_count_; }
  inline int GetSpillSlotCount() const { return spill_slot_count_; }
  inline int GetReturnSlotCount() const { return return_slot_count_; }

  void SetAllocatedRegisters(BitVector* regs) {
    DCHECK_NULL(allocated_registers_);
    allocated_registers_ = regs;
  }

  void SetAllocatedDoubleRegisters(BitVector* regs) {
    DCHECK_NULL(allocated_double_registers_);
    allocated_double_registers_ = regs;
  }

  bool DidAllocateDoubleRegisters() const {
    return !allocated_double_registers_->IsEmpty();
  }

  void AlignSavedCalleeRegisterSlots(int alignment = kDoubleSize) {
    int alignment_slots = alignment / kSystemPointerSize;
    int delta = alignment_slots - (frame_slot_count_ & (alignment_slots - 1));
    if (delta != alignment_slots) {
      frame_slot_count_ += delta;
    }
    spill_slot_count_ += delta;
  }

  void AllocateSavedCalleeRegisterSlots(int count) {
    frame_slot_count_ += count;
  }

  int AllocateSpillSlot(int width, int alignment = 0) {
    DCHECK_EQ(frame_slot_count_,
              fixed_slot_count_ + spill_slot_count_ + return_slot_count_);
    int frame_slot_count_before = frame_slot_count_;
    if (alignment > kSystemPointerSize) {
      // Slots are pointer sized, so alignment greater than a pointer size
      // requires allocating additional slots.
      width += alignment - kSystemPointerSize;
    }
    AllocateAlignedFrameSlots(width);
    spill_slot_count_ += frame_slot_count_ - frame_slot_count_before;
    return frame_slot_count_ - return_slot_count_ - 1;
  }

  void EnsureReturnSlots(int count) {
    if (count > return_slot_count_) {
      count -= return_slot_count_;
      frame_slot_count_ += count;
      return_slot_count_ += count;
    }
  }

  int AlignFrame(int alignment = kDoubleSize);

  int ReserveSpillSlots(size_t slot_count) {
    DCHECK_EQ(0, spill_slot_count_);
    spill_slot_count_ += static_cast<int>(slot_count);
    frame_slot_count_ += static_cast<int>(slot_count);
    return frame_slot_count_ - 1;
  }

 private:
  void AllocateAlignedFrameSlots(int width) {
    DCHECK_LT(0, width);
    int new_frame_slots = (width + kSystemPointerSize - 1) / kSystemPointerSize;
    // Align to 8 bytes if width is a multiple of 8 bytes, and to 16 bytes if
    // multiple of 16.
    int align_to =
        (width & 15) == 0 ? 16 : (width & 7) == 0 ? 8 : kSystemPointerSize;
    frame_slot_count_ = RoundUp(frame_slot_count_ + new_frame_slots,
                                align_to / kSystemPointerSize);
    DCHECK_LT(0, frame_slot_count_);
  }

 private:
  int fixed_slot_count_;
  int frame_slot_count_;
  int spill_slot_count_;
  int return_slot_count_;
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
    DCHECK_EQ(0, offset & 1);
    return FrameOffset(offset | kFromSp);
  }

  inline static FrameOffset FromFramePointer(int offset) {
    DCHECK_EQ(0, offset & 1);
    return FrameOffset(offset | kFromFp);
  }

 private:
  explicit FrameOffset(int offset) : offset_(offset) {}

  int offset_;  // Encodes SP or FP in the low order bit.

  static const int kFromSp = 1;
  static const int kFromFp = 0;
};

// Encapsulates the mutable state maintained during code generation about the
// current function's frame.
class FrameAccessState : public ZoneObject {
 public:
  explicit FrameAccessState(const Frame* const frame)
      : frame_(frame),
        access_frame_with_fp_(false),
        sp_delta_(0),
        has_frame_(false) {}

  const Frame* frame() const { return frame_; }
  void MarkHasFrame(bool state);

  int sp_delta() const { return sp_delta_; }
  void ClearSPDelta() { sp_delta_ = 0; }
  void IncreaseSPDelta(int amount) { sp_delta_ += amount; }

  bool access_frame_with_fp() const { return access_frame_with_fp_; }

  // Regardless of how we access slots on the stack - using sp or fp - do we
  // have a frame, at the current stage in code generation.
  bool has_frame() const { return has_frame_; }

  void SetFrameAccessToDefault();
  void SetFrameAccessToFP() { access_frame_with_fp_ = true; }
  void SetFrameAccessToSP() { access_frame_with_fp_ = false; }

  int GetSPToFPSlotCount() const {
    int frame_slot_count =
        (has_frame() ? frame()->GetTotalFrameSlotCount() : kElidedFrameSlots) -
        StandardFrameConstants::kFixedSlotCountAboveFp;
    return frame_slot_count + sp_delta();
  }
  int GetSPToFPOffset() const {
    return GetSPToFPSlotCount() * kSystemPointerSize;
  }

  // Get the frame offset for a given spill slot. The location depends on the
  // calling convention and the specific frame layout, and may thus be
  // architecture-specific. Negative spill slots indicate arguments on the
  // caller's frame.
  FrameOffset GetFrameOffset(int spill_slot) const;

 private:
  const Frame* const frame_;
  bool access_frame_with_fp_;
  int sp_delta_;
  bool has_frame_;
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_FRAME_H_
