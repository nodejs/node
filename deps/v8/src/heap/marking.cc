// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/marking.h"

namespace v8 {
namespace internal {

bool MarkingBitmap::AllBitsSetInRange(MarkBitIndex start_index,
                                      MarkBitIndex end_index) const {
  if (start_index >= end_index) return false;
  end_index--;

  const CellIndex start_cell_index = IndexToCell(start_index);
  MarkBit::CellType start_index_mask = IndexInCellMask(start_index);
  const CellIndex end_cell_index = IndexToCell(end_index);
  MarkBit::CellType end_index_mask = IndexInCellMask(end_index);

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

bool MarkingBitmap::AllBitsClearInRange(MarkBitIndex start_index,
                                        MarkBitIndex end_index) const {
  if (start_index >= end_index) return true;
  end_index--;

  const CellIndex start_cell_index = IndexToCell(start_index);
  MarkBit::CellType start_index_mask = IndexInCellMask(start_index);
  const CellIndex end_cell_index = IndexToCell(end_index);
  MarkBit::CellType end_index_mask = IndexInCellMask(end_index);

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

class CellPrinter final {
 public:
  CellPrinter() = default;

  void Print(size_t pos, uint32_t cell) {
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

    PrintF("%zu: ", pos);
    PrintWord(cell);
    PrintF("\n");
  }

  void Flush() {
    if (seq_length > 0) {
      PrintF("%zu: %dx%zu\n", seq_start, seq_type == 0 ? 0 : 1,
             seq_length * MarkingBitmap::kBitsPerCell);
      seq_length = 0;
    }
  }

  static bool IsSeq(uint32_t cell) { return cell == 0 || cell == 0xFFFFFFFF; }

 private:
  size_t seq_start = 0;
  uint32_t seq_type = 0;
  size_t seq_length = 0;
};

}  // anonymous namespace

void MarkingBitmap::Print() const {
  CellPrinter printer;
  for (size_t i = 0; i < kCellsCount; i++) {
    printer.Print(i, cells()[i]);
  }
  printer.Flush();
  PrintF("\n");
}

bool MarkingBitmap::IsClean() const {
  for (size_t i = 0; i < kCellsCount; i++) {
    if (cells()[i] != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace internal
}  // namespace v8
