// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/marking.h"

namespace v8 {
namespace internal {

template <>
bool ConcurrentBitmap<AccessMode::NON_ATOMIC>::AllBitsSetInRange(
    uint32_t start_index, uint32_t end_index) {
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

template <>
bool ConcurrentBitmap<AccessMode::NON_ATOMIC>::AllBitsClearInRange(
    uint32_t start_index, uint32_t end_index) {
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

template <>
void ConcurrentBitmap<AccessMode::NON_ATOMIC>::Print() {
  CellPrinter printer;
  for (int i = 0; i < CellsCount(); i++) {
    printer.Print(i, cells()[i]);
  }
  printer.Flush();
  PrintF("\n");
}

template <>
bool ConcurrentBitmap<AccessMode::NON_ATOMIC>::IsClean() {
  for (int i = 0; i < CellsCount(); i++) {
    if (cells()[i] != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace internal
}  // namespace v8
