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
class CompilationInfo;
class Script;
class SharedFunctionInfo;
struct SourcePositionInfo;

// SourcePosition stores
// - script_offset (31 bit non-negative int or kNoSourcePosition)
// - inlining_id (16 bit non-negative int or kNotInlined).
//
// A defined inlining_id refers to positions in
// CompilationInfo::inlined_functions or
// DeoptimizationInputData::InliningPositions, depending on the compilation
// stage.
class SourcePosition final {
 public:
  explicit SourcePosition(int script_offset, int inlining_id = kNotInlined)
      : value_(0) {
    SetScriptOffset(script_offset);
    SetInliningId(inlining_id);
  }

  static SourcePosition Unknown() { return SourcePosition(kNoSourcePosition); }
  bool IsKnown() const {
    return ScriptOffset() != kNoSourcePosition || InliningId() != kNotInlined;
  }
  bool isInlined() const { return InliningId() != kNotInlined; }

  // Assumes that the code object is optimized
  std::vector<SourcePositionInfo> InliningStack(Handle<Code> code) const;
  std::vector<SourcePositionInfo> InliningStack(CompilationInfo* cinfo) const;

  void Print(std::ostream& out, Code* code) const;

  int ScriptOffset() const { return ScriptOffsetField::decode(value_) - 1; }
  int InliningId() const { return InliningIdField::decode(value_) - 1; }

  void SetScriptOffset(int script_offset) {
    DCHECK(script_offset <= ScriptOffsetField::kMax - 2);
    DCHECK(script_offset >= kNoSourcePosition);
    value_ = ScriptOffsetField::update(value_, script_offset + 1);
  }
  void SetInliningId(int inlining_id) {
    DCHECK(inlining_id <= InliningIdField::kMax - 2);
    DCHECK(inlining_id >= kNotInlined);
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
  void Print(std::ostream& out, SharedFunctionInfo* function) const;

  // InliningId is in the high bits for better compression in
  // SourcePositionTable.
  typedef BitField64<int, 0, 31> ScriptOffsetField;
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

  // references position in DeoptimizationInputData::literals()
  int inlined_function_id;
};

struct SourcePositionInfo {
  SourcePositionInfo(SourcePosition pos, Handle<SharedFunctionInfo> f);

  SourcePosition position;
  Handle<SharedFunctionInfo> function;
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
