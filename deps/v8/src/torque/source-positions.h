// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_SOURCE_POSITIONS_H_
#define V8_TORQUE_SOURCE_POSITIONS_H_

#include "src/torque/contextual.h"

namespace v8 {
namespace internal {
namespace torque {

class SourceId {
 private:
  explicit SourceId(int id) : id_(id) {}
  int id_;
  friend class SourceFileMap;
};

struct SourcePosition {
  SourceId source;
  int line;
  int column;
};

DECLARE_CONTEXTUAL_VARIABLE(CurrentSourceFile, SourceId)
DECLARE_CONTEXTUAL_VARIABLE(CurrentSourcePosition, SourcePosition)

class SourceFileMap : public ContextualClass<SourceFileMap> {
 public:
  SourceFileMap() = default;
  static const std::string& GetSource(SourceId source) {
    return Get().sources_[source.id_];
  }

  static SourceId AddSource(std::string path) {
    Get().sources_.push_back(std::move(path));
    return SourceId(static_cast<int>(Get().sources_.size()) - 1);
  }

 private:
  std::vector<std::string> sources_;
};

inline std::string PositionAsString(SourcePosition pos) {
  return SourceFileMap::GetSource(pos.source) + ":" +
         std::to_string(pos.line + 1) + ":" + std::to_string(pos.column + 1);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_SOURCE_POSITIONS_H_
