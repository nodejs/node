// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_SIDETABLE_H_
#define V8_COMPILER_TURBOSHAFT_SIDETABLE_H_

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>

#include "src/base/iterator.h"
#include "src/base/small-vector.h"
#include "src/base/vector.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::compiler::turboshaft {

// This sidetable is a conceptually infinite mapping from Turboshaft operation
// indices to values. It grows automatically and default-initializes the table
// when accessed out-of-bounds.
template <class T>
class GrowingSidetable {
 public:
  explicit GrowingSidetable(Zone* zone) : table_(zone) {}

  T& operator[](OpIndex op) {
    size_t i = op.id();
    if (V8_UNLIKELY(i >= table_.size())) {
      table_.resize(NextSize(i));
      // Make sure we also get access to potential over-allocation by
      // `resize()`.
      table_.resize(table_.capacity());
    }
    return table_[i];
  }

  const T& operator[](OpIndex op) const {
    size_t i = op.id();
    if (V8_UNLIKELY(i >= table_.size())) {
      table_.resize(NextSize(i));
      // Make sure we also get access to potential over-allocation by
      // `resize()`.
      table_.resize(table_.capacity());
    }
    return table_[i];
  }

  // Reset by filling the table with the default value instead of shrinking to
  // keep the memory for later phases.
  void Reset() { std::fill(table_.begin(), table_.end(), T{}); }

  // Returns `true` if the table never contained any values, even before
  // `Reset()`.
  bool empty() { return table_.empty(); }

 private:
  mutable ZoneVector<T> table_;

  size_t NextSize(size_t out_of_bounds_index) const {
    DCHECK_GE(out_of_bounds_index, table_.size());
    return out_of_bounds_index + out_of_bounds_index / 2 + 32;
  }
};

// A fixed-size sidetable mapping from `OpIndex` to `T`.
// Elements are default-initialized.
template <class T>
class FixedSidetable {
 public:
  explicit FixedSidetable(size_t size, Zone* zone) : table_(size, zone) {}

  T& operator[](OpIndex op) {
    DCHECK_LT(op.id(), table_.size());
    return table_[op.id()];
  }

  const T& operator[](OpIndex op) const {
    DCHECK_LT(op.id(), table_.size());
    return table_[op.id()];
  }

 private:
  ZoneVector<T> table_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_SIDETABLE_H_
