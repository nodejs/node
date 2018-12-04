// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/marking.h"

namespace v8 {
namespace internal {

void Bitmap::Clear() {
  base::Atomic32* cell_base = reinterpret_cast<base::Atomic32*>(cells());
  for (int i = 0; i < CellsCount(); i++) {
    base::Relaxed_Store(cell_base + i, 0);
  }
  // This fence prevents re-ordering of publishing stores with the mark-bit
  // clearing stores.
  base::SeqCst_MemoryFence();
}

void Bitmap::MarkAllBits() {
  base::Atomic32* cell_base = reinterpret_cast<base::Atomic32*>(cells());
  for (int i = 0; i < CellsCount(); i++) {
    base::Relaxed_Store(cell_base + i, 0xffffffff);
  }
  // This fence prevents re-ordering of publishing stores with the mark-bit
  // clearing stores.
  base::SeqCst_MemoryFence();
}

void Bitmap::SetRange(uint32_t start_index, uint32_t end_index) {
  if (start_index >= end_index) return;
  end_index--;

  unsigned int start_cell_index = start_index >> Bitmap::kBitsPerCellLog2;
  MarkBit::CellType start_index_mask = 1u << Bitmap::IndexInCell(start_index);
  unsigned int end_cell_index = end_index >> Bitmap::kBitsPerCellLog2;
  MarkBit::CellType end_index_mask = 1u << Bitmap::IndexInCell(end_index);
  if (start_cell_index != end_cell_index) {
    // Firstly, fill all bits from the start address to the end of the first
    // cell with 1s.
    SetBitsInCell<AccessMode::ATOMIC>(start_cell_index,
                                      ~(start_index_mask - 1));
    // Then fill all in between cells with 1s.
    base::Atomic32* cell_base = reinterpret_cast<base::Atomic32*>(cells());
    for (unsigned int i = start_cell_index + 1; i < end_cell_index; i++) {
      base::Relaxed_Store(cell_base + i, ~0u);
    }
    // Finally, fill all bits until the end address in the last cell with 1s.
    SetBitsInCell<AccessMode::ATOMIC>(end_cell_index,
                                      end_index_mask | (end_index_mask - 1));
  } else {
    SetBitsInCell<AccessMode::ATOMIC>(
        start_cell_index, end_index_mask | (end_index_mask - start_index_mask));
  }
  // This fence prevents re-ordering of publishing stores with the mark-
  // bit setting stores.
  base::SeqCst_MemoryFence();
}

void Bitmap::ClearRange(uint32_t start_index, uint32_t end_index) {
  if (start_index >= end_index) return;
  end_index--;

  unsigned int start_cell_index = start_index >> Bitmap::kBitsPerCellLog2;
  MarkBit::CellType start_index_mask = 1u << Bitmap::IndexInCell(start_index);

  unsigned int end_cell_index = end_index >> Bitmap::kBitsPerCellLog2;
  MarkBit::CellType end_index_mask = 1u << Bitmap::IndexInCell(end_index);

  if (start_cell_index != end_cell_index) {
    // Firstly, fill all bits from the start address to the end of the first
    // cell with 0s.
    ClearBitsInCell<AccessMode::ATOMIC>(start_cell_index,
                                        ~(start_index_mask - 1));
    // Then fill all in between cells with 0s.
    base::Atomic32* cell_base = reinterpret_cast<base::Atomic32*>(cells());
    for (unsigned int i = start_cell_index + 1; i < end_cell_index; i++) {
      base::Relaxed_Store(cell_base + i, 0);
    }
    // Finally, set all bits until the end address in the last cell with 0s.
    ClearBitsInCell<AccessMode::ATOMIC>(end_cell_index,
                                        end_index_mask | (end_index_mask - 1));
  } else {
    ClearBitsInCell<AccessMode::ATOMIC>(
        start_cell_index, end_index_mask | (end_index_mask - start_index_mask));
  }
  // This fence prevents re-ordering of publishing stores with the mark-
  // bit clearing stores.
  base::SeqCst_MemoryFence();
}

bool Bitmap::AllBitsSetInRange(uint32_t start_index, uint32_t end_index) {
  if (start_index >= end_index) return false;
  end_index--;

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
    matching_mask = end_index_mask | (end_index_mask - 1);
    return ((cells()[end_cell_index] & matching_mask) == matching_mask);
  } else {
    matching_mask = end_index_mask | (end_index_mask - start_index_mask);
    return (cells()[end_cell_index] & matching_mask) == matching_mask;
  }
}

bool Bitmap::AllBitsClearInRange(uint32_t start_index, uint32_t end_index) {
  if (start_index >= end_index) return true;
  end_index--;

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
    matching_mask = end_index_mask | (end_index_mask - 1);
    return !(cells()[end_cell_index] & matching_mask);
  } else {
    matching_mask = end_index_mask | (end_index_mask - start_index_mask);
    return !(cells()[end_cell_index] & matching_mask);
  }
}

namespace {

void PrintWord(uint32_t word, uint32_t himask = 0) {
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
             seq_length * Bitmap::kBitsPerCell);
      seq_length = 0;
    }
  }

  static bool IsSeq(uint32_t cell) { return cell == 0 || cell == 0xFFFFFFFF; }

 private:
  uint32_t seq_start;
  uint32_t seq_type;
  uint32_t seq_length;
};

}  // anonymous namespace

void Bitmap::Print() {
  CellPrinter printer;
  for (int i = 0; i < CellsCount(); i++) {
    printer.Print(i, cells()[i]);
  }
  printer.Flush();
  PrintF("\n");
}

bool Bitmap::IsClean() {
  for (int i = 0; i < CellsCount(); i++) {
    if (cells()[i] != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace internal
}  // namespace v8
