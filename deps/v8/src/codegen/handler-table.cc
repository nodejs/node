// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/handler-table.h"

#include <iomanip>

#include "src/codegen/assembler-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

HandlerTable::HandlerTable(Code code)
    : HandlerTable(code.InstructionStart() + code.handler_table_offset(),
                   code.handler_table_size()) {}

HandlerTable::HandlerTable(BytecodeArray bytecode_array)
    : HandlerTable(bytecode_array.handler_table()) {}

HandlerTable::HandlerTable(ByteArray byte_array)
    : number_of_entries_(byte_array.length() / kRangeEntrySize /
                         sizeof(int32_t)),
#ifdef DEBUG
      mode_(kRangeBasedEncoding),
#endif
      raw_encoded_data_(
          reinterpret_cast<Address>(byte_array.GetDataStartAddress())) {
  DCHECK_EQ(0, byte_array.length() % (kRangeEntrySize * sizeof(int32_t)));
}

HandlerTable::HandlerTable(Address handler_table, int handler_table_size)
    : number_of_entries_(handler_table_size / kReturnEntrySize /
                         sizeof(int32_t)),
#ifdef DEBUG
      mode_(kReturnAddressBasedEncoding),
#endif
      raw_encoded_data_(handler_table) {
  static_assert(4 < kReturnEntrySize * sizeof(int32_t), "allowed padding");
  DCHECK_GE(4, handler_table_size % (kReturnEntrySize * sizeof(int32_t)));
}

int HandlerTable::GetRangeStart(int index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  int offset = index * kRangeEntrySize + kRangeStartIndex;
  return Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
}

int HandlerTable::GetRangeEnd(int index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  int offset = index * kRangeEntrySize + kRangeEndIndex;
  return Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
}

int HandlerTable::GetRangeHandler(int index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  int offset = index * kRangeEntrySize + kRangeHandlerIndex;
  return HandlerOffsetField::decode(
      Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)));
}

int HandlerTable::GetRangeData(int index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  int offset = index * kRangeEntrySize + kRangeDataIndex;
  return Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
}

HandlerTable::CatchPrediction HandlerTable::GetRangePrediction(
    int index) const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfRangeEntries());
  int offset = index * kRangeEntrySize + kRangeHandlerIndex;
  return HandlerPredictionField::decode(
      Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)));
}

int HandlerTable::GetReturnOffset(int index) const {
  DCHECK_EQ(kReturnAddressBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfReturnEntries());
  int offset = index * kReturnEntrySize + kReturnOffsetIndex;
  return Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t));
}

int HandlerTable::GetReturnHandler(int index) const {
  DCHECK_EQ(kReturnAddressBasedEncoding, mode_);
  DCHECK_LT(index, NumberOfReturnEntries());
  int offset = index * kReturnEntrySize + kReturnHandlerIndex;
  return HandlerOffsetField::decode(
      Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)));
}

void HandlerTable::SetRangeStart(int index, int value) {
  int offset = index * kRangeEntrySize + kRangeStartIndex;
  Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)) = value;
}

void HandlerTable::SetRangeEnd(int index, int value) {
  int offset = index * kRangeEntrySize + kRangeEndIndex;
  Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)) = value;
}

void HandlerTable::SetRangeHandler(int index, int handler_offset,
                                   CatchPrediction prediction) {
  int value = HandlerOffsetField::encode(handler_offset) |
              HandlerPredictionField::encode(prediction);
  int offset = index * kRangeEntrySize + kRangeHandlerIndex;
  Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)) = value;
}

void HandlerTable::SetRangeData(int index, int value) {
  int offset = index * kRangeEntrySize + kRangeDataIndex;
  Memory<int32_t>(raw_encoded_data_ + offset * sizeof(int32_t)) = value;
}

// static
int HandlerTable::LengthForRange(int entries) {
  return entries * kRangeEntrySize * sizeof(int32_t);
}

// static
int HandlerTable::EmitReturnTableStart(Assembler* masm) {
  masm->DataAlign(sizeof(int32_t));  // Make sure entries are aligned.
  masm->RecordComment(";;; Exception handler table.");
  int table_start = masm->pc_offset();
  return table_start;
}

// static
void HandlerTable::EmitReturnEntry(Assembler* masm, int offset, int handler) {
  masm->dd(offset);
  masm->dd(HandlerOffsetField::encode(handler));
}

int HandlerTable::NumberOfRangeEntries() const {
  DCHECK_EQ(kRangeBasedEncoding, mode_);
  return number_of_entries_;
}

int HandlerTable::NumberOfReturnEntries() const {
  DCHECK_EQ(kReturnAddressBasedEncoding, mode_);
  return number_of_entries_;
}

int HandlerTable::LookupRange(int pc_offset, int* data_out,
                              CatchPrediction* prediction_out) {
  int innermost_handler = -1;
#ifdef DEBUG
  // Assuming that ranges are well nested, we don't need to track the innermost
  // offsets. This is just to verify that the table is actually well nested.
  int innermost_start = std::numeric_limits<int>::min();
  int innermost_end = std::numeric_limits<int>::max();
#endif
  for (int i = 0; i < NumberOfRangeEntries(); ++i) {
    int start_offset = GetRangeStart(i);
    int end_offset = GetRangeEnd(i);
    int handler_offset = GetRangeHandler(i);
    int handler_data = GetRangeData(i);
    CatchPrediction prediction = GetRangePrediction(i);
    if (pc_offset >= start_offset && pc_offset < end_offset) {
      DCHECK_GE(start_offset, innermost_start);
      DCHECK_LT(end_offset, innermost_end);
      innermost_handler = handler_offset;
#ifdef DEBUG
      innermost_start = start_offset;
      innermost_end = end_offset;
#endif
      if (data_out) *data_out = handler_data;
      if (prediction_out) *prediction_out = prediction;
    }
  }
  return innermost_handler;
}

// TODO(turbofan): Make sure table is sorted and use binary search.
int HandlerTable::LookupReturn(int pc_offset) {
  for (int i = 0; i < NumberOfReturnEntries(); ++i) {
    int return_offset = GetReturnOffset(i);
    if (pc_offset == return_offset) {
      return GetReturnHandler(i);
    }
  }
  return -1;
}

#ifdef ENABLE_DISASSEMBLER

void HandlerTable::HandlerTableRangePrint(std::ostream& os) {
  os << "   from   to       hdlr (prediction,   data)\n";
  for (int i = 0; i < NumberOfRangeEntries(); ++i) {
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
  for (int i = 0; i < NumberOfReturnEntries(); ++i) {
    int pc_offset = GetReturnOffset(i);
    int handler_offset = GetReturnHandler(i);
    os << std::hex << "    " << std::setw(4) << pc_offset << "  ->  "
       << std::setw(4) << handler_offset << std::dec << "\n";
  }
}

#endif  // ENABLE_DISASSEMBLER

}  // namespace internal
}  // namespace v8
