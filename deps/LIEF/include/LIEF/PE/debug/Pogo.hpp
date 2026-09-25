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
#ifndef LIEF_PE_POGO_H
#define LIEF_PE_POGO_H
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/PE/debug/Debug.hpp"
#include "LIEF/PE/debug/PogoEntry.hpp"

namespace LIEF {
namespace PE {

class Builder;
class Parser;

/// This class represents a *Profile Guided Optimization* entry from the
/// debug directory (``IMAGE_DEBUG_TYPE_POGO``).
class LIEF_API Pogo : public Debug {

  friend class Builder;
  friend class Parser;

  public:
  using entries_t        = std::vector<PogoEntry>;
  using it_entries       = ref_iterator<entries_t&>;
  using it_const_entries = const_ref_iterator<const entries_t&>;

  enum class SIGNATURES {
    UNKNOWN = 0x0fffffff,
    ZERO    = 0x00000000,
    LCTG    = 0x4C544347, // LCTG
    PGI     = 0x50474900, // PGI\0
  };

  Pogo() {
    type_ = Debug::TYPES::POGO;
  }

  Pogo(SIGNATURES sig) :
    Debug{Debug::TYPES::POGO},
    sig_{sig}
  {}

  Pogo(const details::pe_debug& debug, SIGNATURES sig, Section* sec) :
    Debug(debug, sec),
    sig_(sig)
  {}

  Pogo(const Pogo&) = default;
  Pogo& operator=(const Pogo&) = default;

  Pogo(Pogo&&) = default;
  Pogo& operator=(Pogo&&) = default;

  std::unique_ptr<Debug> clone() const override {
    return std::unique_ptr<Debug>(new Pogo(*this));
  }

  SIGNATURES signature() const {
    return sig_;
  }

  /// An iterator over the different POGO elements
  it_entries entries() {
    return entries_;
  }

  it_const_entries entries() const {
    return entries_;
  }

  void add(PogoEntry entry) {
    entries_.push_back(std::move(entry));
  }

  static bool classof(const Debug* debug) {
    return debug->type() == Debug::TYPES::POGO;
  }

  void accept(Visitor& visitor) const override;

  std::string to_string() const override;

  ~Pogo() override = default;

  protected:
  SIGNATURES sig_ = SIGNATURES::UNKNOWN;
  entries_t entries_;
};

LIEF_API const char* to_string(Pogo::SIGNATURES e);

} // Namespace PE
} // Namespace LIEF

#endif
