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

#ifndef SRC_PROFILING_COMMON_INTERNER_H_
#define SRC_PROFILING_COMMON_INTERNER_H_

#include <stddef.h>
#include <stdint.h>
#include <functional>
#include <unordered_set>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace profiling {

using InternID = uint32_t;

// Interner that hands out refcounted references.
template <typename T>
class Interner {
 private:
  struct Entry {
    template <typename... U>
    Entry(Interner<T>* in, InternID i, U... args)
        : data(std::forward<U...>(args...)), id(i), interner(in) {}

    bool operator<(const Entry& other) const { return data < other.data; }
    bool operator==(const Entry& other) const { return data == other.data; }

    struct Hash {
      size_t operator()(const Entry& e) const noexcept {
        return std::hash<T>{}(e.data);
      }
    };

    const T data;
    InternID id;
    size_t ref_count = 0;
    Interner<T>* interner;
  };

 public:
  class Interned {
   public:
    friend class Interner<T>;
    Interned(Entry* entry) : entry_(entry) {}
    Interned(const Interned& other) : entry_(other.entry_) {
      if (entry_ != nullptr)
        entry_->ref_count++;
    }

    Interned(Interned&& other) noexcept : entry_(other.entry_) {
      other.entry_ = nullptr;
    }

    Interned& operator=(Interned other) noexcept {
      using std::swap;
      swap(*this, other);
      return *this;
    }

    const T& data() const { return entry_->data; }

    InternID id() const { return entry_->id; }

    ~Interned() {
      if (entry_ != nullptr)
        entry_->interner->Return(entry_);
    }

    bool operator<(const Interned& other) const {
      return entry_ < other.entry_;
    }

    bool operator==(const Interned& other) const {
      return entry_ == other.entry_;
    }

    const T* operator->() const { return &entry_->data; }

   private:
    Interner::Entry* entry_;
  };

  template <typename... U>
  Interned Intern(U... args) {
    Entry item(this, next_id, std::forward<U...>(args...));
    auto it = entries_.find(item);
    if (it == entries_.cend()) {
      // This does not invalidate pointers to entries we hold in Interned. See
      // https://timsong-cpp.github.io/cppwp/n3337/unord.req#8
      auto it_and_inserted = entries_.emplace(std::move(item));
      next_id++;
      it = it_and_inserted.first;
      PERFETTO_DCHECK(it_and_inserted.second);
    }
    Entry& entry = const_cast<Entry&>(*it);
    entry.ref_count++;
    return Interned(&entry);
  }

  ~Interner() { PERFETTO_DCHECK(entries_.empty()); }

  size_t entry_count_for_testing() { return entries_.size(); }

 private:
  void Return(Entry* entry) {
    if (--entry->ref_count == 0)
      entries_.erase(*entry);
  }

  InternID next_id = 1;
  std::unordered_set<Entry, typename Entry::Hash> entries_;
  static_assert(sizeof(Interned) == sizeof(void*),
                "interned things should be small");
};

template <typename T>
void swap(typename Interner<T>::Interned a, typename Interner<T>::Interned b) {
  std::swap(a.entry_, b.entry_);
}

template <typename T>
using Interned = typename Interner<T>::Interned;

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_COMMON_INTERNER_H_
