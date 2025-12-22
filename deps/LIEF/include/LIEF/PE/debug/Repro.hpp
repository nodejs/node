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
#ifndef LIEF_PE_REPRO_H
#define LIEF_PE_REPRO_H
#include <ostream>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/PE/debug/Debug.hpp"
#include "LIEF/span.hpp"

namespace LIEF {
namespace PE {

class Builder;
class Parser;

/// This class represents a reproducible build entry from the debug directory.
/// (`IMAGE_DEBUG_TYPE_REPRO`).
/// This entry is usually generated with the undocumented `/Brepro` linker flag.
///
/// See: https://nikhilism.com/post/2020/windows-deterministic-builds/
class LIEF_API Repro : public Debug {

  friend class Builder;
  friend class Parser;

  public:
  Repro() :
    Debug{Debug::TYPES::REPRO}
  {}

  Repro(std::vector<uint8_t> hash) :
    Debug{Debug::TYPES::REPRO},
    hash_{std::move(hash)}
  {}

  Repro(const details::pe_debug& dbg, std::vector<uint8_t> hash, Section* sec) :
    Debug{dbg, sec},
    hash_{std::move(hash)}
  {}

  Repro(const details::pe_debug& dbg, Section* sec) :
    Debug{dbg, sec}
  {}

  Repro(const Repro& other) = default;
  Repro& operator=(const Repro& other) = default;

  Repro(Repro&&) = default;
  Repro& operator=(Repro&& other) = default;

  /// The hash associated with the reproducible build
  span<const uint8_t> hash() const {
    return hash_;
  }

  span<uint8_t> hash() {
    return hash_;
  }

  void hash(std::vector<uint8_t> h) {
    hash_ = std::move(h);
  }

  std::unique_ptr<Debug> clone() const override {
    return std::unique_ptr<Debug>(new Repro(*this));
  }

  static bool classof(const Debug* debug) {
    return debug->type() == Debug::TYPES::REPRO;
  }

  void accept(Visitor& visitor) const override;

  std::string to_string() const override;

  ~Repro() override = default;

  protected:
  std::vector<uint8_t> hash_;
};

} // Namespace PE
} // Namespace LIEF

#endif
