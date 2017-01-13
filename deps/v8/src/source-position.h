// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SOURCE_POSITION_H_
#define V8_SOURCE_POSITION_H_

#include <ostream>

#include "src/flags.h"
#include "src/globals.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// This class encapsulates encoding and decoding of sources positions from
// which hydrogen values originated.
// When FLAG_track_hydrogen_positions is set this object encodes the
// identifier of the inlining and absolute offset from the start of the
// inlined function.
// When the flag is not set we simply track absolute offset from the
// script start.
class SourcePosition {
 public:
  static SourcePosition Unknown() {
    return SourcePosition::FromRaw(kNoPosition);
  }

  bool IsUnknown() const { return value_ == kNoPosition; }

  uint32_t position() const { return PositionField::decode(value_); }
  void set_position(uint32_t position) {
    if (FLAG_hydrogen_track_positions) {
      value_ = static_cast<uint32_t>(PositionField::update(value_, position));
    } else {
      value_ = position;
    }
  }

  uint32_t inlining_id() const { return InliningIdField::decode(value_); }
  void set_inlining_id(uint32_t inlining_id) {
    if (FLAG_hydrogen_track_positions) {
      value_ =
          static_cast<uint32_t>(InliningIdField::update(value_, inlining_id));
    }
  }

  uint32_t raw() const { return value_; }

 private:
  static const uint32_t kNoPosition = static_cast<uint32_t>(kNoSourcePosition);
  typedef BitField<uint32_t, 0, 9> InliningIdField;

  // Offset from the start of the inlined function.
  typedef BitField<uint32_t, 9, 23> PositionField;

  friend class HPositionInfo;
  friend class Deoptimizer;

  static SourcePosition FromRaw(uint32_t raw_position) {
    SourcePosition position;
    position.value_ = raw_position;
    return position;
  }

  // If FLAG_hydrogen_track_positions is set contains bitfields InliningIdField
  // and PositionField.
  // Otherwise contains absolute offset from the script start.
  uint32_t value_;
};

inline std::ostream& operator<<(std::ostream& os, const SourcePosition& p) {
  if (p.IsUnknown()) {
    return os << "<?>";
  } else if (FLAG_hydrogen_track_positions) {
    return os << "<" << p.inlining_id() << ":" << p.position() << ">";
  } else {
    return os << "<0:" << p.raw() << ">";
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SOURCE_POSITION_H_
