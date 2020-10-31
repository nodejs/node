/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_BASE_LOOKUP_SET_H_
#define INCLUDE_PERFETTO_EXT_BASE_LOOKUP_SET_H_

#include <set>

namespace perfetto {
namespace base {

// Set that allows lookup from const member of the object.
template <typename T, typename U, U T::*p>
class LookupSet {
 public:
  T* Get(const U& key) {
    // This will be nicer with C++14 transparent comparators.
    // Then we will be able to look up by just the key using a sutiable
    // comparator.
    //
    // For now we need to allow to construct a T from the key.
    T node(key);
    auto it = set_.find(node);
    if (it == set_.end())
      return nullptr;
    return const_cast<T*>(&(*it));
  }

  template <typename... P>
  T* Emplace(P&&... args) {
    auto r = set_.emplace(std::forward<P>(args)...);
    return const_cast<T*>(&(*r.first));
  }

  bool Remove(const T& child) { return set_.erase(child); }

  void Clear() { set_.clear(); }

  static_assert(std::is_const<U>::value, "key must be const");

 private:
  class Comparator {
   public:
    bool operator()(const T& one, const T& other) const {
      return (&one)->*p < (&other)->*p;
    }
  };

  std::set<T, Comparator> set_;
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_LOOKUP_SET_H_
