// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_SOURCE_POSITIONS_H_
#define V8_TORQUE_SOURCE_POSITIONS_H_

#include <iostream>

#include "src/base/contextual.h"

namespace v8 {
namespace internal {
namespace torque {

struct SourcePosition;

class SourceId {
 public:
  static SourceId Invalid() { return SourceId(-1); }
  bool IsValid() const { return id_ != -1; }
  int operator==(const SourceId& s) const { return id_ == s.id_; }
  bool operator<(const SourceId& s) const { return id_ < s.id_; }

 private:
  explicit SourceId(int id) : id_(id) {}
  int id_;
  friend struct SourcePosition;
  friend class SourceFileMap;
};

struct LineAndColumn {
  static constexpr int kUnknownOffset = -1;

  int offset;
  int line;
  int column;

  static LineAndColumn Invalid() { return {-1, -1, -1}; }
  static LineAndColumn WithUnknownOffset(int line, int column) {
    return {kUnknownOffset, line, column};
  }

  bool operator==(const LineAndColumn& other) const {
    if (offset == kUnknownOffset || other.offset == kUnknownOffset) {
      return line == other.line && column == other.column;
    }
    DCHECK_EQ(offset == other.offset,
              line == other.line && column == other.column);
    return offset == other.offset;
  }
  bool operator!=(const LineAndColumn& other) const {
    return !operator==(other);
  }
};

struct SourcePosition {
  SourceId source;
  LineAndColumn start;
  LineAndColumn end;

  static SourcePosition Invalid() {
    SourcePosition pos{SourceId::Invalid(), LineAndColumn::Invalid(),
                       LineAndColumn::Invalid()};
    return pos;
  }

  bool CompareStartIgnoreColumn(const SourcePosition& pos) const {
    return start.line == pos.start.line && source == pos.source;
  }

  bool Contains(LineAndColumn pos) const {
    if (pos.line < start.line || pos.line > end.line) return false;

    if (pos.line == start.line && pos.column < start.column) return false;
    if (pos.line == end.line && pos.column >= end.column) return false;
    return true;
  }

  bool operator==(const SourcePosition& pos) const {
    return source == pos.source && start == pos.start && end == pos.end;
  }
  bool operator!=(const SourcePosition& pos) const { return !(*this == pos); }
};

DECLARE_CONTEXTUAL_VARIABLE(CurrentSourceFile, SourceId);
DECLARE_CONTEXTUAL_VARIABLE(CurrentSourcePosition, SourcePosition);

class V8_EXPORT_PRIVATE SourceFileMap
    : public base::ContextualClass<SourceFileMap> {
 public:
  explicit SourceFileMap(std::string v8_root) : v8_root_(std::move(v8_root)) {}
  static const std::string& PathFromV8Root(SourceId file);
  static std::string PathFromV8RootWithoutExtension(SourceId file);
  static std::string AbsolutePath(SourceId file);
  static SourceId AddSource(std::string path);
  static SourceId GetSourceId(const std::string& path);
  static std::vector<SourceId> AllSources();
  static bool FileRelativeToV8RootExists(const std::string& path);

 private:
  std::vector<std::string> sources_;
  std::string v8_root_;
};

inline std::string PositionAsString(SourcePosition pos) {
  return SourceFileMap::PathFromV8Root(pos.source) + ":" +
         std::to_string(pos.start.line + 1) + ":" +
         std::to_string(pos.start.column + 1);
}

inline std::ostream& operator<<(std::ostream& out, SourcePosition pos) {
  return out << "https://source.chromium.org/chromium/chromium/src/+/main:v8/"
             << SourceFileMap::PathFromV8Root(pos.source)
             << "?l=" << (pos.start.line + 1)
             << "&c=" << (pos.start.column + 1);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_SOURCE_POSITIONS_H_
