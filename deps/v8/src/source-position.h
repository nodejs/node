// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SOURCE_POSITION_H_
#define V8_SOURCE_POSITION_H_

#include <ostream>

#include "src/flags.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class Code;
class OptimizedCompilationInfo;
class Script;
class SharedFunctionInfo;
struct SourcePositionInfo;

// SourcePosition stores
// - is_external (1 bit true/false)
//
// - if is_external is true:
// - external_line (20 bits, non-negative int)
// - external_file_id (10 bits, non-negative int)
//
// - if is_external is false:
// - script_offset (30 bit non-negative int or kNoSourcePosition)
//
// - In both cases, there is an inlining_id.
// - inlining_id (16 bit non-negative int or kNotInlined).
//
// An "external" SourcePosition is one given by a file_id and a line,
// suitable for embedding references to .cc or .tq files.
// Otherwise, a SourcePosition contains an offset into a JavaScript
// file.
//
// A defined inlining_id refers to positions in
// OptimizedCompilationInfo::inlined_functions or
// DeoptimizationData::InliningPositions, depending on the compilation stage.
class SourcePosition final {
 public:
  explicit SourcePosition(int script_offset, int inlining_id = kNotInlined)
      : value_(0) {
    SetIsExternal(false);
    SetScriptOffset(script_offset);
    SetInliningId(inlining_id);
  }

  // External SourcePositions should use the following method to construct
  // SourcePositions to avoid confusion.
  static SourcePosition External(int line, int file_id) {
    return SourcePosition(line, file_id, kNotInlined);
  }

  static SourcePosition Unknown() { return SourcePosition(kNoSourcePosition); }
  bool IsKnown() const {
    if (IsExternal()) return true;
    return ScriptOffset() != kNoSourcePosition || InliningId() != kNotInlined;
  }
  bool isInlined() const {
    if (IsExternal()) return false;
    return InliningId() != kNotInlined;
  }

  bool IsExternal() const { return IsExternalField::decode(value_); }
  bool IsJavaScript() const { return !IsExternal(); }

  int ExternalLine() const {
    DCHECK(IsExternal());
    return ExternalLineField::decode(value_);
  }

  int ExternalFileId() const {
    DCHECK(IsExternal());
    return ExternalFileIdField::decode(value_);
  }

  // Assumes that the code object is optimized
  std::vector<SourcePositionInfo> InliningStack(Handle<Code> code) const;
  std::vector<SourcePositionInfo> InliningStack(
      OptimizedCompilationInfo* cinfo) const;

  void Print(std::ostream& out, Code code) const;
  void PrintJson(std::ostream& out) const;

  int ScriptOffset() const {
    DCHECK(IsJavaScript());
    return ScriptOffsetField::decode(value_) - 1;
  }
  int InliningId() const { return InliningIdField::decode(value_) - 1; }

  void SetIsExternal(bool external) {
    value_ = IsExternalField::update(value_, external);
  }
  void SetExternalLine(int line) {
    DCHECK(IsExternal());
    DCHECK(line <= ExternalLineField::kMax - 1);
    value_ = ExternalLineField::update(value_, line);
  }
  void SetExternalFileId(int file_id) {
    DCHECK(IsExternal());
    DCHECK(file_id <= ExternalFileIdField::kMax - 1);
    value_ = ExternalFileIdField::update(value_, file_id);
  }

  void SetScriptOffset(int script_offset) {
    DCHECK(IsJavaScript());
    DCHECK(script_offset <= ScriptOffsetField::kMax - 2);
    DCHECK_GE(script_offset, kNoSourcePosition);
    value_ = ScriptOffsetField::update(value_, script_offset + 1);
  }
  void SetInliningId(int inlining_id) {
    DCHECK(inlining_id <= InliningIdField::kMax - 2);
    DCHECK_GE(inlining_id, kNotInlined);
    value_ = InliningIdField::update(value_, inlining_id + 1);
  }

  static const int kNotInlined = -1;
  STATIC_ASSERT(kNoSourcePosition == -1);

  int64_t raw() const { return static_cast<int64_t>(value_); }
  static SourcePosition FromRaw(int64_t raw) {
    SourcePosition position = Unknown();
    DCHECK_GE(raw, 0);
    position.value_ = static_cast<uint64_t>(raw);
    return position;
  }

 private:
  // Used by SourcePosition::External(line, file_id).
  SourcePosition(int line, int file_id, int inlining_id) : value_(0) {
    SetIsExternal(true);
    SetExternalLine(line);
    SetExternalFileId(file_id);
    SetInliningId(inlining_id);
  }

  void Print(std::ostream& out, SharedFunctionInfo function) const;

  typedef BitField64<bool, 0, 1> IsExternalField;

  // The two below are only used if IsExternal() is true.
  typedef BitField64<int, 1, 20> ExternalLineField;
  typedef BitField64<int, 21, 10> ExternalFileIdField;

  // ScriptOffsetField is only used if IsExternal() is false.
  typedef BitField64<int, 1, 30> ScriptOffsetField;

  // InliningId is in the high bits for better compression in
  // SourcePositionTable.
  typedef BitField64<int, 31, 16> InliningIdField;

  // Leaving the highest bit untouched to allow for signed conversion.
  uint64_t value_;
};

inline bool operator==(const SourcePosition& lhs, const SourcePosition& rhs) {
  return lhs.raw() == rhs.raw();
}

inline bool operator!=(const SourcePosition& lhs, const SourcePosition& rhs) {
  return !(lhs == rhs);
}

struct InliningPosition {
  // position of the inlined call
  SourcePosition position = SourcePosition::Unknown();

  // references position in DeoptimizationData::literals()
  int inlined_function_id;
};

struct SourcePositionInfo {
  SourcePositionInfo(SourcePosition pos, Handle<SharedFunctionInfo> f);

  SourcePosition position;
  Handle<SharedFunctionInfo> shared;
  Handle<Script> script;
  int line = -1;
  int column = -1;
};

std::ostream& operator<<(std::ostream& out, const SourcePosition& pos);

std::ostream& operator<<(std::ostream& out, const SourcePositionInfo& pos);
std::ostream& operator<<(std::ostream& out,
                         const std::vector<SourcePositionInfo>& stack);

}  // namespace internal
}  // namespace v8

#endif  // V8_SOURCE_POSITION_H_
