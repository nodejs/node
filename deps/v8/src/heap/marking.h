// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MARKING_H
#define V8_MARKING_H

#include "src/utils.h"

namespace v8 {
namespace internal {

class MarkBit {
 public:
  typedef uint32_t CellType;

  inline MarkBit(CellType* cell, CellType mask) : cell_(cell), mask_(mask) {}

#ifdef DEBUG
  bool operator==(const MarkBit& other) {
    return cell_ == other.cell_ && mask_ == other.mask_;
  }
#endif

 private:
  inline CellType* cell() { return cell_; }
  inline CellType mask() { return mask_; }

  inline MarkBit Next() {
    CellType new_mask = mask_ << 1;
    if (new_mask == 0) {
      return MarkBit(cell_ + 1, 1);
    } else {
      return MarkBit(cell_, new_mask);
    }
  }

  inline void Set() { *cell_ |= mask_; }
  inline bool Get() { return (*cell_ & mask_) != 0; }
  inline void Clear() { *cell_ &= ~mask_; }

  CellType* cell_;
  CellType mask_;

  friend class IncrementalMarking;
  friend class Marking;
};

// Bitmap is a sequence of cells each containing fixed number of bits.
class Bitmap {
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

  static int SizeFor(int cells_count) {
    return sizeof(MarkBit::CellType) * cells_count;
  }

  INLINE(static uint32_t IndexToCell(uint32_t index)) {
    return index >> kBitsPerCellLog2;
  }

  V8_INLINE static uint32_t IndexInCell(uint32_t index) {
    return index & kBitIndexMask;
  }

  INLINE(static uint32_t CellToIndex(uint32_t index)) {
    return index << kBitsPerCellLog2;
  }

  INLINE(static uint32_t CellAlignIndex(uint32_t index)) {
    return (index + kBitIndexMask) & ~kBitIndexMask;
  }

  INLINE(MarkBit::CellType* cells()) {
    return reinterpret_cast<MarkBit::CellType*>(this);
  }

  INLINE(Address address()) { return reinterpret_cast<Address>(this); }

  INLINE(static Bitmap* FromAddress(Address addr)) {
    return reinterpret_cast<Bitmap*>(addr);
  }

  inline MarkBit MarkBitFromIndex(uint32_t index) {
    MarkBit::CellType mask = 1u << IndexInCell(index);
    MarkBit::CellType* cell = this->cells() + (index >> kBitsPerCellLog2);
    return MarkBit(cell, mask);
  }

  void Clear() {
    for (int i = 0; i < CellsCount(); i++) cells()[i] = 0;
  }

  // Sets all bits in the range [start_index, end_index).
  void SetRange(uint32_t start_index, uint32_t end_index) {
    unsigned int start_cell_index = start_index >> Bitmap::kBitsPerCellLog2;
    MarkBit::CellType start_index_mask = 1u << Bitmap::IndexInCell(start_index);

    unsigned int end_cell_index = end_index >> Bitmap::kBitsPerCellLog2;
    MarkBit::CellType end_index_mask = 1u << Bitmap::IndexInCell(end_index);

    if (start_cell_index != end_cell_index) {
      // Firstly, fill all bits from the start address to the end of the first
      // cell with 1s.
      cells()[start_cell_index] |= ~(start_index_mask - 1);
      // Then fill all in between cells with 1s.
      for (unsigned int i = start_cell_index + 1; i < end_cell_index; i++) {
        cells()[i] = ~0u;
      }
      // Finally, fill all bits until the end address in the last cell with 1s.
      cells()[end_cell_index] |= (end_index_mask - 1);
    } else {
      cells()[start_cell_index] |= end_index_mask - start_index_mask;
    }
  }

  // Clears all bits in the range [start_index, end_index).
  void ClearRange(uint32_t start_index, uint32_t end_index) {
    unsigned int start_cell_index = start_index >> Bitmap::kBitsPerCellLog2;
    MarkBit::CellType start_index_mask = 1u << Bitmap::IndexInCell(start_index);

    unsigned int end_cell_index = end_index >> Bitmap::kBitsPerCellLog2;
    MarkBit::CellType end_index_mask = 1u << Bitmap::IndexInCell(end_index);

    if (start_cell_index != end_cell_index) {
      // Firstly, fill all bits from the start address to the end of the first
      // cell with 0s.
      cells()[start_cell_index] &= (start_index_mask - 1);
      // Then fill all in between cells with 0s.
      for (unsigned int i = start_cell_index + 1; i < end_cell_index; i++) {
        cells()[i] = 0;
      }
      // Finally, set all bits until the end address in the last cell with 0s.
      cells()[end_cell_index] &= ~(end_index_mask - 1);
    } else {
      cells()[start_cell_index] &= ~(end_index_mask - start_index_mask);
    }
  }

  // Returns true if all bits in the range [start_index, end_index) are set.
  bool AllBitsSetInRange(uint32_t start_index, uint32_t end_index) {
    unsigned int start_cell_index = start_index >> Bitmap::kBitsPerCellLog2;
    MarkBit::CellType start_index_mask = 1u << Bitmap::IndexInCell(start_index);

    unsigned int end_cell_index = end_index >> Bitmap::kBitsPerCellLog2;
    MarkBit::CellType end_index_mask = 1u << Bitmap::IndexInCell(end_index);

    MarkBit::CellType matching_mask;
    if (start_cell_index != end_cell_index) {
      matching_mask = ~(start_index_mask - 1);
      if ((cells()[start_cell_index] & matching_mask) != matching_mask) {
        return false;
      }
      for (unsigned int i = start_cell_index + 1; i < end_cell_index; i++) {
        if (cells()[i] != ~0u) return false;
      }
      matching_mask = (end_index_mask - 1);
      return ((cells()[end_cell_index] & matching_mask) == matching_mask);
    } else {
      matching_mask = end_index_mask - start_index_mask;
      return (cells()[end_cell_index] & matching_mask) == matching_mask;
    }
  }

  // Returns true if all bits in the range [start_index, end_index) are cleared.
  bool AllBitsClearInRange(uint32_t start_index, uint32_t end_index) {
    unsigned int start_cell_index = start_index >> Bitmap::kBitsPerCellLog2;
    MarkBit::CellType start_index_mask = 1u << Bitmap::IndexInCell(start_index);

    unsigned int end_cell_index = end_index >> Bitmap::kBitsPerCellLog2;
    MarkBit::CellType end_index_mask = 1u << Bitmap::IndexInCell(end_index);

    MarkBit::CellType matching_mask;
    if (start_cell_index != end_cell_index) {
      matching_mask = ~(start_index_mask - 1);
      if ((cells()[start_cell_index] & matching_mask)) return false;
      for (unsigned int i = start_cell_index + 1; i < end_cell_index; i++) {
        if (cells()[i]) return false;
      }
      matching_mask = (end_index_mask - 1);
      return !(cells()[end_cell_index] & matching_mask);
    } else {
      matching_mask = end_index_mask - start_index_mask;
      return !(cells()[end_cell_index] & matching_mask);
    }
  }

  static void PrintWord(uint32_t word, uint32_t himask = 0) {
    for (uint32_t mask = 1; mask != 0; mask <<= 1) {
      if ((mask & himask) != 0) PrintF("[");
      PrintF((mask & word) ? "1" : "0");
      if ((mask & himask) != 0) PrintF("]");
    }
  }

  class CellPrinter {
   public:
    CellPrinter() : seq_start(0), seq_type(0), seq_length(0) {}

    void Print(uint32_t pos, uint32_t cell) {
      if (cell == seq_type) {
        seq_length++;
        return;
      }

      Flush();

      if (IsSeq(cell)) {
        seq_start = pos;
        seq_length = 0;
        seq_type = cell;
        return;
      }

      PrintF("%d: ", pos);
      PrintWord(cell);
      PrintF("\n");
    }

    void Flush() {
      if (seq_length > 0) {
        PrintF("%d: %dx%d\n", seq_start, seq_type == 0 ? 0 : 1,
               seq_length * kBitsPerCell);
        seq_length = 0;
      }
    }

    static bool IsSeq(uint32_t cell) { return cell == 0 || cell == 0xFFFFFFFF; }

   private:
    uint32_t seq_start;
    uint32_t seq_type;
    uint32_t seq_length;
  };

  void Print() {
    CellPrinter printer;
    for (int i = 0; i < CellsCount(); i++) {
      printer.Print(i, cells()[i]);
    }
    printer.Flush();
    PrintF("\n");
  }

  bool IsClean() {
    for (int i = 0; i < CellsCount(); i++) {
      if (cells()[i] != 0) {
        return false;
      }
    }
    return true;
  }
};

class Marking : public AllStatic {
 public:
  // Impossible markbits: 01
  static const char* kImpossibleBitPattern;
  INLINE(static bool IsImpossible(MarkBit mark_bit)) {
    return !mark_bit.Get() && mark_bit.Next().Get();
  }

  // Black markbits: 11
  static const char* kBlackBitPattern;
  INLINE(static bool IsBlack(MarkBit mark_bit)) {
    return mark_bit.Get() && mark_bit.Next().Get();
  }

  // White markbits: 00 - this is required by the mark bit clearer.
  static const char* kWhiteBitPattern;
  INLINE(static bool IsWhite(MarkBit mark_bit)) {
    DCHECK(!IsImpossible(mark_bit));
    return !mark_bit.Get();
  }

  // Grey markbits: 10
  static const char* kGreyBitPattern;
  INLINE(static bool IsGrey(MarkBit mark_bit)) {
    return mark_bit.Get() && !mark_bit.Next().Get();
  }

  // IsBlackOrGrey assumes that the first bit is set for black or grey
  // objects.
  INLINE(static bool IsBlackOrGrey(MarkBit mark_bit)) { return mark_bit.Get(); }

  INLINE(static void MarkBlack(MarkBit mark_bit)) {
    mark_bit.Set();
    mark_bit.Next().Set();
  }

  INLINE(static void MarkWhite(MarkBit mark_bit)) {
    mark_bit.Clear();
    mark_bit.Next().Clear();
  }

  INLINE(static void BlackToWhite(MarkBit markbit)) {
    DCHECK(IsBlack(markbit));
    markbit.Clear();
    markbit.Next().Clear();
  }

  INLINE(static void GreyToWhite(MarkBit markbit)) {
    DCHECK(IsGrey(markbit));
    markbit.Clear();
    markbit.Next().Clear();
  }

  INLINE(static void BlackToGrey(MarkBit markbit)) {
    DCHECK(IsBlack(markbit));
    markbit.Next().Clear();
  }

  INLINE(static void WhiteToGrey(MarkBit markbit)) {
    DCHECK(IsWhite(markbit));
    markbit.Set();
  }

  INLINE(static void WhiteToBlack(MarkBit markbit)) {
    DCHECK(IsWhite(markbit));
    markbit.Set();
    markbit.Next().Set();
  }

  INLINE(static void GreyToBlack(MarkBit markbit)) {
    DCHECK(IsGrey(markbit));
    markbit.Next().Set();
  }

  INLINE(static void AnyToGrey(MarkBit markbit)) {
    markbit.Set();
    markbit.Next().Clear();
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
    return IMPOSSIBLE_COLOR;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Marking);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_MARKING_H_
