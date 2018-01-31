// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MARKING_H
#define V8_MARKING_H

#include "src/base/atomic-utils.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class MarkBit {
 public:
  typedef uint32_t CellType;
  STATIC_ASSERT(sizeof(CellType) == sizeof(base::Atomic32));

  inline MarkBit(CellType* cell, CellType mask) : cell_(cell), mask_(mask) {}

#ifdef DEBUG
  bool operator==(const MarkBit& other) {
    return cell_ == other.cell_ && mask_ == other.mask_;
  }
#endif

 private:
  inline MarkBit Next() {
    CellType new_mask = mask_ << 1;
    if (new_mask == 0) {
      return MarkBit(cell_ + 1, 1);
    } else {
      return MarkBit(cell_, new_mask);
    }
  }

  // The function returns true if it succeeded to
  // transition the bit from 0 to 1.
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  inline bool Set();

  template <AccessMode mode = AccessMode::NON_ATOMIC>
  inline bool Get();

  // The function returns true if it succeeded to
  // transition the bit from 1 to 0.
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  inline bool Clear();

  CellType* cell_;
  CellType mask_;

  friend class IncrementalMarking;
  friend class ConcurrentMarkingMarkbits;
  friend class Marking;
};

template <>
inline bool MarkBit::Set<AccessMode::NON_ATOMIC>() {
  CellType old_value = *cell_;
  *cell_ = old_value | mask_;
  return (old_value & mask_) == 0;
}

template <>
inline bool MarkBit::Set<AccessMode::ATOMIC>() {
  return base::AsAtomic32::SetBits(cell_, mask_, mask_);
}

template <>
inline bool MarkBit::Get<AccessMode::NON_ATOMIC>() {
  return (*cell_ & mask_) != 0;
}

template <>
inline bool MarkBit::Get<AccessMode::ATOMIC>() {
  return (base::AsAtomic32::Acquire_Load(cell_) & mask_) != 0;
}

template <>
inline bool MarkBit::Clear<AccessMode::NON_ATOMIC>() {
  CellType old_value = *cell_;
  *cell_ = old_value & ~mask_;
  return (old_value & mask_) == mask_;
}

template <>
inline bool MarkBit::Clear<AccessMode::ATOMIC>() {
  return base::AsAtomic32::SetBits(cell_, 0u, mask_);
}

// Bitmap is a sequence of cells each containing fixed number of bits.
class V8_EXPORT_PRIVATE Bitmap {
 public:
  static const uint32_t kBitsPerCell = 32;
  static const uint32_t kBitsPerCellLog2 = 5;
  static const uint32_t kBitIndexMask = kBitsPerCell - 1;
  static const uint32_t kBytesPerCell = kBitsPerCell / kBitsPerByte;
  static const uint32_t kBytesPerCellLog2 = kBitsPerCellLog2 - kBitsPerByteLog2;

  static const size_t kLength = (1 << kPageSizeBits) >> (kPointerSizeLog2);

  static const size_t kSize = (1 << kPageSizeBits) >>
                              (kPointerSizeLog2 + kBitsPerByteLog2);

  static int CellsForLength(int length) {
    return (length + kBitsPerCell - 1) >> kBitsPerCellLog2;
  }

  int CellsCount() { return CellsForLength(kLength); }

  V8_INLINE static uint32_t IndexToCell(uint32_t index) {
    return index >> kBitsPerCellLog2;
  }

  V8_INLINE static uint32_t IndexInCell(uint32_t index) {
    return index & kBitIndexMask;
  }

  // Retrieves the cell containing the provided markbit index.
  V8_INLINE static uint32_t CellAlignIndex(uint32_t index) {
    return index & ~kBitIndexMask;
  }

  V8_INLINE static bool IsCellAligned(uint32_t index) {
    return (index & kBitIndexMask) == 0;
  }

  V8_INLINE MarkBit::CellType* cells() {
    return reinterpret_cast<MarkBit::CellType*>(this);
  }

  V8_INLINE static Bitmap* FromAddress(Address addr) {
    return reinterpret_cast<Bitmap*>(addr);
  }

  inline MarkBit MarkBitFromIndex(uint32_t index) {
    MarkBit::CellType mask = 1u << IndexInCell(index);
    MarkBit::CellType* cell = this->cells() + (index >> kBitsPerCellLog2);
    return MarkBit(cell, mask);
  }

  void Clear();

  // Clears bits in the given cell. The mask specifies bits to clear: if a
  // bit is set in the mask then the corresponding bit is cleared in the cell.
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  void ClearBitsInCell(uint32_t cell_index, uint32_t mask);

  // Sets bits in the given cell. The mask specifies bits to set: if a
  // bit is set in the mask then the corresponding bit is set in the cell.
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  void SetBitsInCell(uint32_t cell_index, uint32_t mask);

  // Sets all bits in the range [start_index, end_index). The cells at the
  // boundary of the range are updated with atomic compare and swap operation.
  // The inner cells are updated with relaxed write.
  void SetRange(uint32_t start_index, uint32_t end_index);

  // Clears all bits in the range [start_index, end_index). The cells at the
  // boundary of the range are updated with atomic compare and swap operation.
  // The inner cells are updated with relaxed write.
  void ClearRange(uint32_t start_index, uint32_t end_index);

  // Returns true if all bits in the range [start_index, end_index) are set.
  bool AllBitsSetInRange(uint32_t start_index, uint32_t end_index);

  // Returns true if all bits in the range [start_index, end_index) are cleared.
  bool AllBitsClearInRange(uint32_t start_index, uint32_t end_index);

  void Print();

  bool IsClean();
};

template <>
inline void Bitmap::SetBitsInCell<AccessMode::NON_ATOMIC>(uint32_t cell_index,
                                                          uint32_t mask) {
  cells()[cell_index] |= mask;
}

template <>
inline void Bitmap::SetBitsInCell<AccessMode::ATOMIC>(uint32_t cell_index,
                                                      uint32_t mask) {
  base::AsAtomic32::SetBits(cells() + cell_index, mask, mask);
}

template <>
inline void Bitmap::ClearBitsInCell<AccessMode::NON_ATOMIC>(uint32_t cell_index,
                                                            uint32_t mask) {
  cells()[cell_index] &= ~mask;
}

template <>
inline void Bitmap::ClearBitsInCell<AccessMode::ATOMIC>(uint32_t cell_index,
                                                        uint32_t mask) {
  base::AsAtomic32::SetBits(cells() + cell_index, 0u, mask);
}

class Marking : public AllStatic {
 public:
  // TODO(hpayer): The current mark bit operations use as default NON_ATOMIC
  // mode for access. We should remove the default value or switch it with
  // ATOMIC as soon we add concurrency.

  // Impossible markbits: 01
  static const char* kImpossibleBitPattern;
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static bool IsImpossible(MarkBit mark_bit)) {
    if (mode == AccessMode::NON_ATOMIC) {
      return !mark_bit.Get<mode>() && mark_bit.Next().Get<mode>();
    }
    // If we are in concurrent mode we can only tell if an object has the
    // impossible bit pattern if we read the first bit again after reading
    // the first and the second bit. If the first bit is till zero and the
    // second bit is one then the object has the impossible bit pattern.
    bool is_impossible = !mark_bit.Get<mode>() && mark_bit.Next().Get<mode>();
    if (is_impossible) {
      return !mark_bit.Get<mode>();
    }
    return false;
  }

  // Black markbits: 11
  static const char* kBlackBitPattern;
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static bool IsBlack(MarkBit mark_bit)) {
    return mark_bit.Get<mode>() && mark_bit.Next().Get<mode>();
  }

  // White markbits: 00 - this is required by the mark bit clearer.
  static const char* kWhiteBitPattern;
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static bool IsWhite(MarkBit mark_bit)) {
    DCHECK(!IsImpossible<mode>(mark_bit));
    return !mark_bit.Get<mode>();
  }

  // Grey markbits: 10
  static const char* kGreyBitPattern;
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static bool IsGrey(MarkBit mark_bit)) {
    return mark_bit.Get<mode>() && !mark_bit.Next().Get<mode>();
  }

  // IsBlackOrGrey assumes that the first bit is set for black or grey
  // objects.
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static bool IsBlackOrGrey(MarkBit mark_bit)) {
    return mark_bit.Get<mode>();
  }

  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static void MarkWhite(MarkBit markbit)) {
    STATIC_ASSERT(mode == AccessMode::NON_ATOMIC);
    markbit.Clear<mode>();
    markbit.Next().Clear<mode>();
  }

  // Warning: this method is not safe in general in concurrent scenarios.
  // If you know that nobody else will change the bits on the given location
  // then you may use it.
  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static void MarkBlack(MarkBit markbit)) {
    markbit.Set<mode>();
    markbit.Next().Set<mode>();
  }

  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static bool WhiteToGrey(MarkBit markbit)) {
    return markbit.Set<mode>();
  }

  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static bool WhiteToBlack(MarkBit markbit)) {
    return markbit.Set<mode>() && markbit.Next().Set<mode>();
  }

  template <AccessMode mode = AccessMode::NON_ATOMIC>
  INLINE(static bool GreyToBlack(MarkBit markbit)) {
    return markbit.Get<mode>() && markbit.Next().Set<mode>();
  }

  enum ObjectColor {
    BLACK_OBJECT,
    WHITE_OBJECT,
    GREY_OBJECT,
    IMPOSSIBLE_COLOR
  };

  static const char* ColorName(ObjectColor color) {
    switch (color) {
      case BLACK_OBJECT:
        return "black";
      case WHITE_OBJECT:
        return "white";
      case GREY_OBJECT:
        return "grey";
      case IMPOSSIBLE_COLOR:
        return "impossible";
    }
    return "error";
  }

  static ObjectColor Color(MarkBit mark_bit) {
    if (IsBlack(mark_bit)) return BLACK_OBJECT;
    if (IsWhite(mark_bit)) return WHITE_OBJECT;
    if (IsGrey(mark_bit)) return GREY_OBJECT;
    UNREACHABLE();
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Marking);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_MARKING_H_
