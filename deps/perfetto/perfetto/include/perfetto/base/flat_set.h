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

#ifndef INCLUDE_PERFETTO_BASE_FLAT_SET_H_
#define INCLUDE_PERFETTO_BASE_FLAT_SET_H_

#include <algorithm>
#include <vector>

// A vector-based set::set-like container.
// It's more cache friendly than std::*set and performs for cases where:
// 1. A high number of dupes is expected (e.g. pid tracking in ftrace).
// 2. The working set is small (hundreds of elements).

// Performance characteristics (for uniformly random insertion order):
// - For smaller insertions (up to ~500), it outperforms both std::set<int> and
//   std::unordered_set<int> by ~3x.
// - Up until 4k insertions, it is always faster than std::set<int>.
// - unordered_set<int> is faster with more than 2k insertions.
// - unordered_set, however, it's less memory efficient and has more caveats
//   (see chromium's base/containers/README.md).
//
// See flat_set_benchmark.cc and the charts in go/perfetto-int-set-benchmark.

namespace perfetto {
namespace base {

template <typename T>
class FlatSet {
 public:
  using value_type = T;
  using const_pointer = const T*;
  using iterator = typename std::vector<T>::iterator;
  using const_iterator = typename std::vector<T>::const_iterator;

  FlatSet() = default;

  // Mainly for tests. Deliberately not marked as "expicit".
  FlatSet(std::initializer_list<T> initial) : entries_(initial) {
    std::sort(entries_.begin(), entries_.end());
    entries_.erase(std::unique(entries_.begin(), entries_.end()),
                   entries_.end());
  }

  const_iterator find(T value) const {
    auto entries_end = entries_.end();
    auto it = std::lower_bound(entries_.begin(), entries_end, value);
    return (it != entries_end && *it == value) ? it : entries_end;
  }

  size_t count(T value) const { return find(value) == entries_.end() ? 0 : 1; }

  std::pair<iterator, bool> insert(T value) {
    auto entries_end = entries_.end();
    auto it = std::lower_bound(entries_.begin(), entries_end, value);
    if (it != entries_end && *it == value)
      return std::make_pair(it, false);
    // If the value is not found |it| is either end() or the next item strictly
    // greater than |value|. In both cases we want to insert just before that.
    it = entries_.insert(it, std::move(value));
    return std::make_pair(it, true);
  }

  size_t erase(T value) {
    auto it = find(value);
    if (it == entries_.end())
      return 0;
    entries_.erase(it);
    return 1;
  }

  void clear() { entries_.clear(); }

  bool empty() const { return entries_.empty(); }
  void reserve(size_t n) { entries_.reserve(n); }
  size_t size() const { return entries_.size(); }
  const_iterator begin() const { return entries_.begin(); }
  const_iterator end() const { return entries_.end(); }

 private:
  std::vector<T> entries_;
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_BASE_FLAT_SET_H_
