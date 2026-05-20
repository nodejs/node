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
#ifndef LIEF_MACHO_BINDING_INFO_IT_H
#define LIEF_MACHO_BINDING_INFO_IT_H
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/MachO/BindingInfo.hpp"

namespace LIEF {
namespace MachO {
class Binary;
class DyldInfo;
class DyldChainedFixups;

class LIEF_API BindingInfoIterator :
    public iterator_facade_base<BindingInfoIterator,
                                std::random_access_iterator_tag,
                                const BindingInfo&,
                                std::ptrdiff_t,
                                const BindingInfo*,
                                const BindingInfo&
    >
{
  public:
  enum class ORIGIN : uint8_t {
    NONE = 0,
    DYLD,
    CHAINED_FIXUPS,
    INDIRECT,
  };

  BindingInfoIterator& operator=(const BindingInfoIterator&) = default;
  BindingInfoIterator(const BindingInfoIterator&) = default;

  BindingInfoIterator(BindingInfoIterator&&) = default;
  BindingInfoIterator& operator=(BindingInfoIterator&&) = default;
  ~BindingInfoIterator() = default;

  BindingInfoIterator(const DyldInfo& dyld_info, size_t pos) :
    pos_(pos),
    origin_(ORIGIN::DYLD),
    dyld_info_(&dyld_info)
  {}

  BindingInfoIterator(const DyldChainedFixups& fixups, size_t pos) :
    pos_(pos),
    origin_(ORIGIN::CHAINED_FIXUPS),
    chained_fixups_(&fixups)
  {}

  BindingInfoIterator(const Binary& binary, size_t pos) :
    pos_(pos),
    origin_(ORIGIN::INDIRECT),
    binary_(&binary)
  {}

  bool operator<(const BindingInfoIterator& rhs) const {
    return pos_ < rhs.pos_;
  }

  std::ptrdiff_t operator-(const BindingInfoIterator& R) const {
    return pos_ - R.pos_;
  }

  BindingInfoIterator& operator+=(std::ptrdiff_t n) {
    pos_ += n;
    return *this;
  }

  BindingInfoIterator& operator-=(std::ptrdiff_t n) {
    pos_ -= n;
    return *this;
  }

  friend bool operator==(const BindingInfoIterator& LHS, const BindingInfoIterator& RHS) {
    return LHS.pos_ == RHS.pos_;
  }

  const BindingInfo& operator*() const;

  private:
  LIEF_LOCAL BindingInfoIterator() = default;
  size_t pos_ = 0;
  ORIGIN origin_ = ORIGIN::NONE;
  union {
    const DyldInfo* dyld_info_ = nullptr;
    const DyldChainedFixups* chained_fixups_;
    const Binary* binary_;
  };

};

}
}
#endif

