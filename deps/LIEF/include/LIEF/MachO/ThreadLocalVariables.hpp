/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_MACHO_THREAD_LOCAL_VARIABLES_H
#define LIEF_MACHO_THREAD_LOCAL_VARIABLES_H

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/optional.hpp"

#include "LIEF/MachO/Section.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>

namespace LIEF {

namespace MachO {

/// This class represents a MachO section with type
/// Section::TYPE::THREAD_LOCAL_VARIABLES (`S_THREAD_LOCAL_VARIABLES`).
///
/// Such a section contains an array of thread-local variable descriptors
/// (Thunk) that the dynamic linker (dyld) uses to lazily initialize
/// thread-local storage (TLS) on first access.
///
/// Each descriptor holds a pointer to the initializer function, a TLS key, and
/// the offset of the variable in the TLS block.
class LIEF_API ThreadLocalVariables : public LIEF::MachO::Section {
  public:
  friend class Section;
  using LIEF::MachO::Section::Section;

  ThreadLocalVariables();

  ThreadLocalVariables(const ThreadLocalVariables&) = default;
  ThreadLocalVariables(ThreadLocalVariables&&) noexcept = default;

  ThreadLocalVariables& operator=(const ThreadLocalVariables&) = default;
  ThreadLocalVariables& operator=(ThreadLocalVariables&&) noexcept = default;

  /// Descriptor for a single thread-local variable.
  ///
  /// The layout mirrors the `tlv_descriptor` structure defined in
  /// `<mach-o/loader.h>` (see also `libdyld/ThreadLocalVariables.h` in dyld).
  struct LIEF_API Thunk {
    /// Address of the initializer function (`tlv_thunk`)
    uint64_t func = 0;

    /// `pthread_key_t` key used by the runtime
    uint64_t key = 0;

    /// Offset of the variable in the TLS block
    uint64_t offset = 0;

    std::string to_string() const;

    friend std::ostream& operator<<(std::ostream& os, const Thunk& thunk) {
      os << thunk.to_string();
      return os;
    }
  };

  /// Random-access iterator that materializes Thunk values on the fly from the
  /// raw section content.
  class LIEF_API Iterator
    : public iterator_facade_base<Iterator, std::random_access_iterator_tag,
                                  const Thunk> {
    public:
    Iterator() = default;

    Iterator(const ThreadLocalVariables& parent, size_t pos) :
      parent_(&parent),
      pos_(pos) {}

    Iterator(const Iterator&) = default;
    Iterator& operator=(const Iterator&) = default;

    Iterator(Iterator&&) noexcept = default;
    Iterator& operator=(Iterator&&) noexcept = default;

    ~Iterator() = default;

    bool operator<(const Iterator& rhs) const {
      return pos_ < rhs.pos_;
    }

    std::ptrdiff_t operator-(const Iterator& R) const {
      return pos_ - R.pos_;
    }

    Iterator& operator+=(std::ptrdiff_t n) {
      pos_ += n;
      return *this;
    }

    Iterator& operator-=(std::ptrdiff_t n) {
      pos_ -= n;
      return *this;
    }

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS) {
      assert(LHS.parent_ == RHS.parent_);
      return LHS.pos_ == RHS.pos_;
    }

    friend LIEF_API bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    Thunk operator*() const {
      auto value = parent_->get(pos_);
      assert(value);
      return *value;
    }

    private:
    const ThreadLocalVariables* parent_ = nullptr;
    size_t pos_ = 0;
  };

  using thunks_it = iterator_range<Iterator>;

  std::unique_ptr<Section> clone() const override {
    return std::unique_ptr<Section>(new ThreadLocalVariables(*this));
  }

  /// Return an iterator range over the Thunk descriptors stored in this
  /// section.
  thunks_it thunks() const LIEF_LIFETIMEBOUND {
    auto B = Iterator(*this, 0);
    auto E = Iterator(*this, nb_thunks());
    return make_range(std::move(B), std::move(E)); // NOLINT
  }

  /// Number of Thunk descriptors in this section.
  size_t nb_thunks() const;

  /// Access the Thunk at the given @p idx, or return an empty optional if the
  /// index is out of range.
  optional<Thunk> get(size_t idx) const;

  /// Change the Thunk at the given @p idx
  void set(size_t idx, const Thunk& thunk);

  /// Access the Thunk at the given @p idx
  optional<Thunk> operator[](size_t idx) const {
    return get(idx);
  }

  ~ThreadLocalVariables() override = default;

  static bool classof(const Section* section) {
    return section->type() == Section::TYPE::THREAD_LOCAL_VARIABLES;
  }
};

}
}

#endif
