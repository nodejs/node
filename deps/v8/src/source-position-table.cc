// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/source-position-table.h"

#include "src/log.h"
#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// We'll use a simple encoding scheme to record the source positions.
// Conceptually, each position consists of:
// - code_offset: An integer index into the BytecodeArray or code.
// - source_position: An integer index into the source string.
// - position type: Each position is either a statement or an expression.
//
// The basic idea for the encoding is to use a variable-length integer coding,
// where each byte contains 7 bits of payload data, and 1 'more' bit that
// determines whether additional bytes follow. Additionally:
// - we record the difference from the previous position,
// - we just stuff one bit for the type into the code offset,
// - we write least-significant bits first,
// - we use zig-zag encoding to encode both positive and negative numbers.

namespace {

// Each byte is encoded as MoreBit | ValueBits.
class MoreBit : public BitField8<bool, 7, 1> {};
class ValueBits : public BitField8<unsigned, 0, 7> {};

// Helper: Add the offsets from 'other' to 'value'. Also set is_statement.
void AddAndSetEntry(PositionTableEntry& value,
                    const PositionTableEntry& other) {
  value.code_offset += other.code_offset;
  value.source_position += other.source_position;
  value.is_statement = other.is_statement;
}

// Helper: Substract the offsets from 'other' from 'value'.
void SubtractFromEntry(PositionTableEntry& value,
                       const PositionTableEntry& other) {
  value.code_offset -= other.code_offset;
  value.source_position -= other.source_position;
}

// Helper: Encode an integer.
void EncodeInt(ZoneVector<byte>& bytes, int value) {
  // Zig-zag encoding.
  static const int kShift = kIntSize * kBitsPerByte - 1;
  value = ((value << 1) ^ (value >> kShift));
  DCHECK_GE(value, 0);
  unsigned int encoded = static_cast<unsigned int>(value);
  bool more;
  do {
    more = encoded > ValueBits::kMax;
    bytes.push_back(MoreBit::encode(more) |
                    ValueBits::encode(encoded & ValueBits::kMask));
    encoded >>= ValueBits::kSize;
  } while (more);
}

// Encode a PositionTableEntry.
void EncodeEntry(ZoneVector<byte>& bytes, const PositionTableEntry& entry) {
  // We only accept ascending code offsets.
  DCHECK(entry.code_offset >= 0);
  // Since code_offset is not negative, we use sign to encode is_statement.
  EncodeInt(bytes,
            entry.is_statement ? entry.code_offset : -entry.code_offset - 1);
  EncodeInt(bytes, entry.source_position);
}

// Helper: Decode an integer.
void DecodeInt(ByteArray* bytes, int* index, int* v) {
  byte current;
  int shift = 0;
  int decoded = 0;
  bool more;
  do {
    current = bytes->get((*index)++);
    decoded |= ValueBits::decode(current) << shift;
    more = MoreBit::decode(current);
    shift += ValueBits::kSize;
  } while (more);
  DCHECK_GE(decoded, 0);
  decoded = (decoded >> 1) ^ (-(decoded & 1));
  *v = decoded;
}

void DecodeEntry(ByteArray* bytes, int* index, PositionTableEntry* entry) {
  int tmp;
  DecodeInt(bytes, index, &tmp);
  if (tmp >= 0) {
    entry->is_statement = true;
    entry->code_offset = tmp;
  } else {
    entry->is_statement = false;
    entry->code_offset = -(tmp + 1);
  }
  DecodeInt(bytes, index, &entry->source_position);
}

}  // namespace

SourcePositionTableBuilder::SourcePositionTableBuilder(
    Zone* zone, SourcePositionTableBuilder::RecordingMode mode)
    : mode_(mode),
      bytes_(zone),
#ifdef ENABLE_SLOW_DCHECKS
      raw_entries_(zone),
#endif
      previous_() {
}

void SourcePositionTableBuilder::AddPosition(size_t code_offset,
                                             int source_position,
                                             bool is_statement) {
  if (Omit()) return;
  int offset = static_cast<int>(code_offset);
  AddEntry({offset, source_position, is_statement});
}

void SourcePositionTableBuilder::AddEntry(const PositionTableEntry& entry) {
  PositionTableEntry tmp(entry);
  SubtractFromEntry(tmp, previous_);
  EncodeEntry(bytes_, tmp);
  previous_ = entry;
#ifdef ENABLE_SLOW_DCHECKS
  raw_entries_.push_back(entry);
#endif
}

Handle<ByteArray> SourcePositionTableBuilder::ToSourcePositionTable(
    Isolate* isolate, Handle<AbstractCode> code) {
  if (bytes_.empty()) return isolate->factory()->empty_byte_array();
  DCHECK(!Omit());

  Handle<ByteArray> table = isolate->factory()->NewByteArray(
      static_cast<int>(bytes_.size()), TENURED);

  MemCopy(table->GetDataStartAddress(), &*bytes_.begin(), bytes_.size());

  LOG_CODE_EVENT(isolate, CodeLinePosInfoRecordEvent(*code, *table));

#ifdef ENABLE_SLOW_DCHECKS
  // Brute force testing: Record all positions and decode
  // the entire table to verify they are identical.
  auto raw = raw_entries_.begin();
  for (SourcePositionTableIterator encoded(*table); !encoded.done();
       encoded.Advance(), raw++) {
    DCHECK(raw != raw_entries_.end());
    DCHECK_EQ(encoded.code_offset(), raw->code_offset);
    DCHECK_EQ(encoded.source_position(), raw->source_position);
    DCHECK_EQ(encoded.is_statement(), raw->is_statement);
  }
  DCHECK(raw == raw_entries_.end());
  // No additional source positions after creating the table.
  mode_ = OMIT_SOURCE_POSITIONS;
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

}  // namespace internal
}  // namespace v8
