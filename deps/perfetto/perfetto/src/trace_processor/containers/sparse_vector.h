/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_CONTAINERS_SPARSE_VECTOR_H_
#define SRC_TRACE_PROCESSOR_CONTAINERS_SPARSE_VECTOR_H_

#include <stdint.h>

#include <deque>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/optional.h"
#include "src/trace_processor/containers/row_map.h"

namespace perfetto {
namespace trace_processor {

// Base class for SparseVector which allows type erasure to be implemented (e.g.
// allows for std::unique_ptr<SparseVectorBase>).
class SparseVectorBase {
 public:
  virtual ~SparseVectorBase();
};

// A data structure which compactly stores a list of possibly nullable data.
//
// Internally, this class is implemented using a combination of a std::deque
// with a BitVector used to store whether each index is null or not.
// For each null value, it only uses a single bit inside the BitVector at
// a slight cost (searching the BitVector to find the index into the std::deque)
// when looking up the data.
template <typename T>
class SparseVector : public SparseVectorBase {
 public:
  // Creates an empty SparseVector.
  SparseVector() = default;
  ~SparseVector() override = default;

  SparseVector(SparseVector&&) = default;
  SparseVector& operator=(SparseVector&&) noexcept = default;

  // Returns the optional value at |idx| or base::nullopt if the value is null.
  base::Optional<T> Get(uint32_t idx) const {
    auto opt_idx = valid_.IndexOf(idx);
    return opt_idx ? base::Optional<T>(data_[*opt_idx]) : base::nullopt;
  }

  // Returns the non-null value at |ordinal| where |ordinal| gives the index
  // of the entry in-terms of non-null entries only.
  //
  // For example:
  // this = [0, null, 2, null, 4]
  //
  // GetNonNull(0) = 0
  // GetNonNull(1) = 2
  // GetNoNull(2) = 4
  // ...
  T GetNonNull(uint32_t ordinal) const {
    PERFETTO_DCHECK(ordinal < data_.size());
    return data_[ordinal];
  }

  // Adds the given value to the SparseVector.
  void Append(T val) {
    data_.emplace_back(val);
    valid_.Insert(size_++);
  }

  // Adds a null value to the SparseVector.
  void AppendNull() { size_++; }

  // Adds the given optional value to the SparseVector.
  void Append(base::Optional<T> val) {
    if (val) {
      Append(*val);
    } else {
      AppendNull();
    }
  }

  // Sets the value at |idx| to the given |val|.
  void Set(uint32_t idx, T val) {
    auto opt_idx = valid_.IndexOf(idx);

    // Generally, we will be setting a null row to non-null so optimize for that
    // path.
    if (PERFETTO_UNLIKELY(opt_idx)) {
      data_[*opt_idx] = val;
    } else {
      valid_.Insert(idx);

      opt_idx = valid_.IndexOf(idx);
      PERFETTO_DCHECK(opt_idx);
      data_.insert(data_.begin() + static_cast<ptrdiff_t>(*opt_idx), val);
    }
  }

  // Returns the size of the SparseVector; this includes any null values.
  uint32_t size() const { return size_; }

 private:
  explicit SparseVector(const SparseVector&) = delete;
  SparseVector& operator=(const SparseVector&) = delete;

  std::deque<T> data_;
  RowMap valid_;
  uint32_t size_ = 0;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_CONTAINERS_SPARSE_VECTOR_H_
