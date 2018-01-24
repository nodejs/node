// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SOURCE_POSITION_TABLE_H_
#define V8_SOURCE_POSITION_TABLE_H_

#include "src/assert-scope.h"
#include "src/checks.h"
#include "src/globals.h"
#include "src/source-position.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class ByteArray;
template <typename T>
class Handle;
class Isolate;
class Zone;

struct PositionTableEntry {
  PositionTableEntry()
      : code_offset(0), source_position(0), is_statement(false) {}
  PositionTableEntry(int offset, int64_t source, bool statement)
      : code_offset(offset), source_position(source), is_statement(statement) {}

  int code_offset;
  int64_t source_position;
  bool is_statement;
};

class V8_EXPORT_PRIVATE SourcePositionTableBuilder {
 public:
  enum RecordingMode { OMIT_SOURCE_POSITIONS, RECORD_SOURCE_POSITIONS };

  explicit SourcePositionTableBuilder(
      RecordingMode mode = RECORD_SOURCE_POSITIONS);

  void AddPosition(size_t code_offset, SourcePosition source_position,
                   bool is_statement);

  Handle<ByteArray> ToSourcePositionTable(Isolate* isolate);

 private:
  void AddEntry(const PositionTableEntry& entry);

  inline bool Omit() const { return mode_ == OMIT_SOURCE_POSITIONS; }

  RecordingMode mode_;
  std::vector<byte> bytes_;
#ifdef ENABLE_SLOW_DCHECKS
  std::vector<PositionTableEntry> raw_entries_;
#endif
  PositionTableEntry previous_;  // Previously written entry, to compute delta.
};

class V8_EXPORT_PRIVATE SourcePositionTableIterator {
 public:
  // Used for saving/restoring the iterator.
  struct IndexAndPosition {
    int index_;
    PositionTableEntry position_;
  };

  // We expose two flavours of the iterator, depending on the argument passed
  // to the constructor:

  // Handlified iterator allows allocation, but it needs a handle (and thus
  // a handle scope). This is the preferred version.
  explicit SourcePositionTableIterator(Handle<ByteArray> byte_array);

  // Non-handlified iterator does not need a handle scope, but it disallows
  // allocation during its lifetime. This is useful if there is no handle
  // scope around.
  explicit SourcePositionTableIterator(ByteArray* byte_array);

  void Advance();

  int code_offset() const {
    DCHECK(!done());
    return current_.code_offset;
  }
  SourcePosition source_position() const {
    DCHECK(!done());
    return SourcePosition::FromRaw(current_.source_position);
  }
  bool is_statement() const {
    DCHECK(!done());
    return current_.is_statement;
  }
  bool done() const { return index_ == kDone; }

  IndexAndPosition GetState() const { return {index_, current_}; }

  void RestoreState(const IndexAndPosition& saved_state) {
    index_ = saved_state.index_;
    current_ = saved_state.position_;
  }

 private:
  static const int kDone = -1;

  ByteArray* raw_table_ = nullptr;
  Handle<ByteArray> table_;
  int index_ = 0;
  PositionTableEntry current_;
  DisallowHeapAllocation no_gc;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SOURCE_POSITION_TABLE_H_
