// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/source-position-table.h"

#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
namespace interpreter {

// We'll use a simple encoding scheme to record the source positions.
// Conceptually, each position consists of:
// - bytecode_offset: An integer index into the BytecodeArray
// - source_position: An integer index into the source string.
// - position type: Each position is either a statement or an expression.
//
// The basic idea for the encoding is to use a variable-length integer coding,
// where each byte contains 7 bits of payload data, and 1 'more' bit that
// determines whether additional bytes follow. Additionally:
// - we record the difference from the previous position,
// - we just stuff one bit for the type into the bytecode offset,
// - we write least-significant bits first,
// - negative numbers occur only rarely, so we use a denormalized
//   most-significant byte (a byte with all zeros, which normally wouldn't
//   make any sense) to encode a negative sign, so that we 'pay' nothing for
//   positive numbers, but have to pay a full byte for negative integers.

namespace {

// A zero-value in the most-significant byte is used to mark negative numbers.
const int kNegativeSignMarker = 0;

// Each byte is encoded as MoreBit | ValueBits.
class MoreBit : public BitField8<bool, 7, 1> {};
class ValueBits : public BitField8<int, 0, 7> {};

// Helper: Add the offsets from 'other' to 'value'. Also set is_statement.
void AddAndSetEntry(PositionTableEntry& value,
                    const PositionTableEntry& other) {
  value.bytecode_offset += other.bytecode_offset;
  value.source_position += other.source_position;
  value.is_statement = other.is_statement;
}

// Helper: Substract the offsets from 'other' from 'value'.
void SubtractFromEntry(PositionTableEntry& value,
                       const PositionTableEntry& other) {
  value.bytecode_offset -= other.bytecode_offset;
  value.source_position -= other.source_position;
}

// Helper: Encode an integer.
void EncodeInt(ZoneVector<byte>& bytes, int value) {
  bool sign = false;
  if (value < 0) {
    sign = true;
    value = -value;
  }

  bool more;
  do {
    more = value > ValueBits::kMax;
    bytes.push_back(MoreBit::encode(more || sign) |
                    ValueBits::encode(value & ValueBits::kMax));
    value >>= ValueBits::kSize;
  } while (more);

  if (sign) {
    bytes.push_back(MoreBit::encode(false) |
                    ValueBits::encode(kNegativeSignMarker));
  }
}

// Encode a PositionTableEntry.
void EncodeEntry(ZoneVector<byte>& bytes, const PositionTableEntry& entry) {
  // 1 bit for sign + is_statement each, which leaves 30b for the value.
  DCHECK(abs(entry.bytecode_offset) < (1 << 30));
  EncodeInt(bytes, (entry.is_statement ? 1 : 0) | (entry.bytecode_offset << 1));
  EncodeInt(bytes, entry.source_position);
}

// Helper: Decode an integer.
void DecodeInt(ByteArray* bytes, int* index, int* v) {
  byte current;
  int n = 0;
  int value = 0;
  bool more;
  do {
    current = bytes->get((*index)++);
    value |= ValueBits::decode(current) << (n * ValueBits::kSize);
    n++;
    more = MoreBit::decode(current);
  } while (more);

  if (ValueBits::decode(current) == kNegativeSignMarker) {
    value = -value;
  }
  *v = value;
}

void DecodeEntry(ByteArray* bytes, int* index, PositionTableEntry* entry) {
  int tmp;
  DecodeInt(bytes, index, &tmp);
  entry->is_statement = (tmp & 1);

  // Note that '>>' needs to be arithmetic shift in order to handle negative
  // numbers properly.
  entry->bytecode_offset = (tmp >> 1);

  DecodeInt(bytes, index, &entry->source_position);
}

}  // namespace

void SourcePositionTableBuilder::AddPosition(size_t bytecode_offset,
                                             int source_position,
                                             bool is_statement) {
  int offset = static_cast<int>(bytecode_offset);
  AddEntry({offset, source_position, is_statement});
}

void SourcePositionTableBuilder::AddEntry(const PositionTableEntry& entry) {
  PositionTableEntry tmp(entry);
  SubtractFromEntry(tmp, previous_);
  EncodeEntry(bytes_, tmp);
  previous_ = entry;

  if (entry.is_statement) {
    LOG_CODE_EVENT(isolate_, CodeLinePosInfoAddStatementPositionEvent(
                                 jit_handler_data_, entry.bytecode_offset,
                                 entry.source_position));
  }
  LOG_CODE_EVENT(isolate_, CodeLinePosInfoAddPositionEvent(
                               jit_handler_data_, entry.bytecode_offset,
                               entry.source_position));

#ifdef ENABLE_SLOW_DCHECKS
  raw_entries_.push_back(entry);
#endif
}

Handle<ByteArray> SourcePositionTableBuilder::ToSourcePositionTable() {
  if (bytes_.empty()) return isolate_->factory()->empty_byte_array();

  Handle<ByteArray> table = isolate_->factory()->NewByteArray(
      static_cast<int>(bytes_.size()), TENURED);

  MemCopy(table->GetDataStartAddress(), &*bytes_.begin(), bytes_.size());

#ifdef ENABLE_SLOW_DCHECKS
  // Brute force testing: Record all positions and decode
  // the entire table to verify they are identical.
  auto raw = raw_entries_.begin();
  for (SourcePositionTableIterator encoded(*table); !encoded.done();
       encoded.Advance(), raw++) {
    DCHECK(raw != raw_entries_.end());
    DCHECK_EQ(encoded.bytecode_offset(), raw->bytecode_offset);
    DCHECK_EQ(encoded.source_position(), raw->source_position);
    DCHECK_EQ(encoded.is_statement(), raw->is_statement);
  }
  DCHECK(raw == raw_entries_.end());
#endif

  return table;
}

SourcePositionTableIterator::SourcePositionTableIterator(ByteArray* byte_array)
    : table_(byte_array), index_(0), current_() {
  Advance();
}

void SourcePositionTableIterator::Advance() {
  DCHECK(!done());
  DCHECK(index_ >= 0 && index_ <= table_->length());
  if (index_ == table_->length()) {
    index_ = kDone;
  } else {
    PositionTableEntry tmp;
    DecodeEntry(table_, &index_, &tmp);
    AddAndSetEntry(current_, tmp);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
