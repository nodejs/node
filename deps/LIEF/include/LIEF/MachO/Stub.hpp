/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
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
#ifndef LIEF_MACHO_STUB_H
#define LIEF_MACHO_STUB_H
#include "LIEF/visibility.h"
#include "LIEF/span.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/errors.hpp"

#include "LIEF/MachO/Header.hpp"

#include <vector>
#include <cstdint>
#include <ostream>


namespace LIEF {
namespace MachO {

class Binary;
class Section;

/// This class represents a stub entry in sections like `__stubs,__auth_stubs`.
///
/// It wraps assembly instructions which are used to access the *got* where the
/// address of the symbol is resolved.
///
/// Example:
///
/// ```text
/// 0000000236a3c1bc: ___memcpy_chk
///   adrp            x17, #0x241513aa8
///   add             x17, x17, #0x241513aa8
///   ldr             x16, [x17]
///   braa            x16, x17
/// ```
class LIEF_API Stub {
  public:
  struct target_info_t {
    Header::CPU_TYPE arch;
    uint32_t subtype = 0;
    friend bool operator==(const Stub::target_info_t& lhs,
                           const Stub::target_info_t& rhs)
    {
      return lhs.arch == rhs.arch && lhs.subtype == rhs.subtype;
    }
  };
  class LIEF_API Iterator :
    public iterator_facade_base<Iterator, std::random_access_iterator_tag, const Stub>
  {
    public:
    Iterator() = default;

    Iterator(target_info_t target_info, std::vector<const Section*> sections,
             size_t pos) :
      target_info_(target_info),
      stubs_(std::move(sections)),
      pos_(pos)
    {
    }

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
      return LHS.pos_ == RHS.pos_;
    }

    friend LIEF_API bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    Stub operator*() const;

    private:
    const Section* find_section_offset(size_t pos, uint64_t& offset) const;
    target_info_t target_info_;
    std::vector<const Section*> stubs_;
    size_t pos_ = 0;
  };
  public:
  Stub() = delete;
  Stub(const Stub&) = default;
  Stub& operator=(const Stub&) = default;

  Stub(Stub&&) noexcept = default;
  Stub& operator=(Stub&&) noexcept = default;
  ~Stub() = default;

  Stub(target_info_t target_info, uint64_t addr, std::vector<uint8_t> raw) :
    target_info_(target_info),
    address_(addr),
    raw_(std::move(raw))
  {}

  /// The (raw) instructions of this entry as a slice of bytes
  span<const uint8_t> raw() const {
    return raw_;
  }

  /// The virtual address where the stub is located
  uint64_t address() const {
    return address_;
  }

  /// The address resolved by this stub.
  ///
  /// For instance, given this stub:
  ///
  /// ```text
  /// 0x3eec: adrp    x16, #4096
  /// 0x3ef0: ldr     x16, [x16, #24]
  /// 0x3ef4: br      x16
  /// ```
  ///
  /// The function returns: `0x4018`.
  ///
  /// @warning This function is only available with LIEF's extended version
  result<uint64_t> target() const;

  friend LIEF_API std::ostream& operator<<(std::ostream& os, const Stub& stub);

  private:
  target_info_t target_info_;
  uint64_t address_ = 0;
  [[maybe_unused]] mutable uint64_t target_addr_ = 0;
  std::vector<uint8_t> raw_;
};

}
}
#endif
