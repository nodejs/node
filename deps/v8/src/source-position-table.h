// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SOURCE_POSITION_TABLE_H_
#define V8_SOURCE_POSITION_TABLE_H_

#include "src/assert-scope.h"
#include "src/checks.h"
#include "src/handles.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class AbstractCode;
class BytecodeArray;
class ByteArray;
class Isolate;
class Zone;

struct PositionTableEntry {
  PositionTableEntry()
      : code_offset(0), source_position(0), is_statement(false) {}
  PositionTableEntry(int offset, int source, bool statement)
      : code_offset(offset), source_position(source), is_statement(statement) {}

  int code_offset;
  int source_position;
  bool is_statement;
};

class SourcePositionTableBuilder {
 public:
  enum RecordingMode { OMIT_SOURCE_POSITIONS, RECORD_SOURCE_POSITIONS };

  SourcePositionTableBuilder(Zone* zone,
                             RecordingMode mode = RECORD_SOURCE_POSITIONS);

  void AddPosition(size_t code_offset, int source_position, bool is_statement);

  Handle<ByteArray> ToSourcePositionTable(Isolate* isolate,
                                          Handle<AbstractCode> code);

 private:
  void AddEntry(const PositionTableEntry& entry);

  inline bool Omit() const { return mode_ == OMIT_SOURCE_POSITIONS; }

  RecordingMode mode_;
  ZoneVector<byte> bytes_;
#ifdef ENABLE_SLOW_DCHECKS
  ZoneVector<PositionTableEntry> raw_entries_;
#endif
  PositionTableEntry previous_;  // Previously written entry, to compute delta.
};

class SourcePositionTableIterator {
 public:
  explicit SourcePositionTableIterator(ByteArray* byte_array);

  void Advance();

  int code_offset() const {
    DCHECK(!done());
    return current_.code_offset;
  }
  int source_position() const {
    DCHECK(!done());
    return current_.source_position;
  }
  bool is_statement() const {
    DCHECK(!done());
    return current_.is_statement;
  }
  bool done() const { return index_ == kDone; }

 private:
  static const int kDone = -1;

  ByteArray* table_;
  int index_;
  PositionTableEntry current_;
  DisallowHeapAllocation no_gc;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SOURCE_POSITION_TABLE_H_
