// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INTERNAL_INDEX_H_
#define V8_OBJECTS_INTERNAL_INDEX_H_

#include <stdint.h>

#include <limits>

#include "src/base/logging.h"

namespace v8 {
namespace internal {

// Simple wrapper around an entry (which is notably different from "index" for
// dictionary backing stores). Most code should treat this as an opaque
// wrapper: get it via GetEntryForIndex, pass it on to consumers.
class InternalIndex {
 public:
  explicit constexpr InternalIndex(size_t raw) : entry_(raw) {}
  static InternalIndex NotFound() { return InternalIndex(kNotFound); }

  V8_WARN_UNUSED_RESULT InternalIndex adjust_down(size_t subtract) const {
    DCHECK_GE(entry_, subtract);
    return InternalIndex(entry_ - subtract);
  }
  V8_WARN_UNUSED_RESULT InternalIndex adjust_up(size_t add) const {
    DCHECK_LT(entry_, std::numeric_limits<size_t>::max() - add);
    return InternalIndex(entry_ + add);
  }

  bool is_found() const { return entry_ != kNotFound; }
  bool is_not_found() const { return entry_ == kNotFound; }

  size_t raw_value() const { return entry_; }
  uint32_t as_uint32() const {
    DCHECK_LE(entry_, std::numeric_limits<uint32_t>::max());
    return static_cast<uint32_t>(entry_);
  }
  constexpr int as_int() const {
    CONSTEXPR_DCHECK(entry_ <=
                     static_cast<size_t>(std::numeric_limits<int>::max()));
    return static_cast<int>(entry_);
  }

  bool operator==(const InternalIndex& other) { return entry_ == other.entry_; }

  // Iteration support.
  InternalIndex operator*() { return *this; }
  bool operator!=(const InternalIndex& other) { return entry_ != other.entry_; }
  InternalIndex& operator++() {
    entry_++;
    return *this;
  }

  class Range {
   public:
    explicit Range(size_t max) : min_(0), max_(max) {}
    Range(size_t min, size_t max) : min_(min), max_(max) {}

    InternalIndex begin() { return InternalIndex(min_); }
    InternalIndex end() { return InternalIndex(max_); }

   private:
    size_t min_;
    size_t max_;
  };

 private:
  static const size_t kNotFound = std::numeric_limits<size_t>::max();

  size_t entry_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_INTERNAL_INDEX_H_
