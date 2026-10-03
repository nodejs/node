// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/handler-table.h"

#include <algorithm>
#include <iomanip>

#include "src/base/iterator.h"
#include "src/codegen/assembler-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/objects-inl.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

HandlerTable::HandlerTable(Tagged<Code> code)
    : HandlerTable(code->handler_table_address(), code->handler_table_size(),
                   kReturnAddressBasedEncoding) {}

#if V8_ENABLE_WEBASSEMBLY
HandlerTable::HandlerTable(const wasm::WasmCode* code)
    : HandlerTable(code->handler_table(), code->handler_table_size(),
                   kReturnAddressBasedEncoding) {}
#endif  // V8_ENABLE_WEBASSEMBLY

HandlerTable::HandlerTable(Tagged<BytecodeArray> bytecode_array)
    : HandlerTable(bytecode_array->handler_table()) {}

HandlerTable::HandlerTable(Tagged<TrustedByteArray> byte_array)
    : HandlerTable(reinterpret_cast<Address>(byte_array->begin()),
                   byte_array->ulength().value(), kRangeBasedEncoding) {}

HandlerTable::HandlerTable(Address handler_table, uint32_t handler_table_size,
                           EncodingMode encoding_mode)
    : number_of_entries_(handler_table_size / EntrySizeFromMode(encoding_mode) /
                         sizeof(int32_t)),
#ifdef DEBUG
      mode_(encoding_mode),
#endif
      raw_encoded_data_(handler_table) {
  // Check padding.
  static_assert(4 < kReturnEntrySize * sizeof(int32_t), "allowed padding");
  // For return address encoding, maximum padding is 4; otherwise, there should
  // be no padding.
  DCHECK_GE(kReturnAddressBasedEncoding == encoding_mode ? 4 : 0,
            handler_table_size %
                (EntrySizeFromMode(encoding_mode) * sizeof(int32_t)));
}

// static
int HandlerTable::EntrySizeFromMode(EncodingMode mode) {
  switch (mode) {
    case kReturnAddressBasedEncoding:
      return kReturnEntrySize;
    case kRangeBasedEncoding:
      return kRangeEntrySize;
  }
  UNREACHABLE();
}

int HandlerTable::GetRangeStart(uint32_t index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  uint32_t offset = index * kRangeEntrySize + kRangeStartIndex;
  return Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
}

int HandlerTable::GetRangeEnd(uint32_t index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  uint32_t offset = index * kRangeEntrySize + kRangeEndIndex;
  return Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
}

int HandlerTable::GetRangeHandlerBitfield(uint32_t index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  uint32_t offset = index * kRangeEntrySize + kRangeHandlerIndex;
  return base::Relaxed_Load(
      &Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)));
}

int HandlerTable::GetRangeHandler(uint32_t index) const {
  return HandlerOffsetField::decode(GetRangeHandlerBitfield(index));
}

int HandlerTable::GetRangeData(uint32_t index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  uint32_t offset = index * kRangeEntrySize + kRangeDataIndex;
  return Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
}

HandlerTable::CatchPrediction HandlerTable::GetRangePrediction(
    uint32_t index) const {
  return HandlerPredictionField::decode(GetRangeHandlerBitfield(index));
}

bool HandlerTable::HandlerWasUsed(uint32_t index) const {
  return HandlerWasUsedField::decode(GetRangeHandlerBitfield(index));
}

void HandlerTable::MarkHandlerUsed(uint32_t index) {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  uint32_t offset = index * kRangeEntrySize + kRangeHandlerIndex;
  auto& mem = Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
  base::Relaxed_Store(&mem, HandlerWasUsedField::update(mem, true));
}

int HandlerTable::GetReturnOffset(uint32_t index) const {
  DCHECK_EQ(kReturnAddressBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfReturnEntries());
  uint32_t offset = index * kReturnEntrySize + kReturnOffsetIndex;
  return Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
}

int HandlerTable::GetReturnHandler(uint32_t index) const {
  DCHECK_EQ(kReturnAddressBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfReturnEntries());
  uint32_t offset = index * kReturnEntrySize + kReturnHandlerIndex;
  return HandlerOffsetField::decode(
      Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)));
}

void HandlerTable::SetRangeStart(uint32_t index, int value) {
  uint32_t offset = index * kRangeEntrySize + kRangeStartIndex;
  Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)) = value;
}

void HandlerTable::SetRangeEnd(uint32_t index, int value) {
  uint32_t offset = index * kRangeEntrySize + kRangeEndIndex;
  Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)) = value;
}

void HandlerTable::SetRangeHandler(uint32_t index, int handler_offset,
                                   CatchPrediction prediction) {
  CHECK(HandlerOffsetField::is_valid(handler_offset));
  int value = HandlerOffsetField::encode(handler_offset) |
              HandlerWasUsedField::encode(false) |
              HandlerPredictionField::encode(prediction);
  uint32_t offset = index * kRangeEntrySize + kRangeHandlerIndex;
  Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)) = value;
}

void HandlerTable::SetRangeData(uint32_t index, int value) {
  uint32_t offset = index * kRangeEntrySize + kRangeDataIndex;
  Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)) = value;
}

// static
uint32_t HandlerTable::LengthForRange(uint32_t entries) {
  return entries * kRangeEntrySize * sizeof(int32_t);
}

// static
int HandlerTable::EmitReturnTableStart(Assembler* masm) {
  masm->DataAlign(InstructionStream::kMetadataAlignment);
  masm->RecordComment(";;; Exception handler table.");
  int table_start = masm->pc_offset();
  return table_start;
}

// static
void HandlerTable::EmitReturnEntry(Assembler* masm, int offset, int handler) {
  masm->dd(offset);
  masm->dd(HandlerOffsetField::encode(handler));
}

uint32_t HandlerTable::NumberOfRangeEntries() const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  return number_of_entries_;
}

uint32_t HandlerTable::NumberOfReturnEntries() const {
  DCHECK_EQ(kReturnAddressBasedEncoding, mode_);
  return number_of_entries_;
}

int HandlerTable::LookupHandlerIndexForRange(int pc_offset) const {
  int innermost_handler = kNoHandlerFound;
#ifdef DEBUG
  // Assuming that ranges are well nested, we don't need to track the innermost
  // offsets. This is just to verify that the table is actually well nested.
  int innermost_start = std::numeric_limits<int>::min();
  int innermost_end = std::numeric_limits<int>::max();
#endif
  for (uint32_t i = 0; i < NumberOfRangeEntries(); ++i) {
    int start_offset = GetRangeStart(i);
    int end_offset = GetRangeEnd(i);
    if (end_offset <= pc_offset) continue;
    if (start_offset > pc_offset) break;
    DCHECK_GE(start_offset, innermost_start);
    DCHECK_LT(end_offset, innermost_end);
    innermost_handler = i;
#ifdef DEBUG
    innermost_start = start_offset;
    innermost_end = end_offset;
#endif
  }
  return innermost_handler;
}

int HandlerTable::LookupReturn(int pc_offset) {
  // We only implement the methods needed by the standard libraries we care
  // about. This is not technically a full random access iterator by the spec.
  struct Iterator : base::iterator<std::random_access_iterator_tag, int> {
    Iterator(HandlerTable* tbl, uint32_t idx) : table(tbl), index(idx) {}
    value_type operator*() const { return table->GetReturnOffset(index); }
    bool operator!=(const Iterator& other) const { return !(*this == other); }
    bool operator==(const Iterator& other) const {
      return index == other.index;
    }
    // GLIBCXX_DEBUG checks uses the <= comparator.
    bool operator<=(const Iterator& other) { return index <= other.index; }
    Iterator& operator++() {
      index++;
      return *this;
    }
    Iterator& operator--() {
      DCHECK_GT(index, 0);
      index--;
      return *this;
    }
    Iterator& operator+=(difference_type offset) {
      index += offset;
      return *this;
    }
    difference_type operator-(const Iterator& other) const {
      DCHECK_GE(index, other.index);
      return index - other.index;
    }
    HandlerTable* table;
    uint32_t index;
  };
  Iterator begin{this, 0}, end{this, NumberOfReturnEntries()};
  SLOW_DCHECK(std::is_sorted(begin, end));  // Must be sorted.
  Iterator result = std::lower_bound(begin, end, pc_offset);
  if (result != end && *result == pc_offset) {
    return GetReturnHandler(result.index);
  }
  return -1;
}

#ifdef ENABLE_DISASSEMBLER

void HandlerTable::HandlerTableRangePrint(std::ostream& os) {
  os << "   from   to       hdlr (prediction,   data)\n";
  for (uint32_t i = 0; i < NumberOfRangeEntries(); ++i) {
    int pc_start = GetRangeStart(i);
    int pc_end = GetRangeEnd(i);
    int handler_offset = GetRangeHandler(i);
    int handler_data = GetRangeData(i);
    CatchPrediction prediction = GetRangePrediction(i);
    os << "  (" << std::setw(4) << pc_start << "," << std::setw(4) << pc_end
       << ")  ->  " << std::setw(4) << handler_offset
       << " (prediction=" << prediction << ", data=" << handler_data << ")\n";
  }
}

void HandlerTable::HandlerTableReturnPrint(std::ostream& os) {
  os << "  offset   handler\n";
  for (uint32_t i = 0; i < NumberOfReturnEntries(); ++i) {
    int pc_offset = GetReturnOffset(i);
    int handler_offset = GetReturnHandler(i);
    os << std::hex << "    " << std::setw(4) << pc_offset << "  ->  "
       << std::setw(4) << handler_offset << std::dec << "\n";
  }
}

#endif  // ENABLE_DISASSEMBLER

}  // namespace internal
}  // namespace v8
