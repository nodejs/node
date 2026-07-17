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
#ifndef LIEF_PE_EX_DLL_CHARACTERISTICS_H
#define LIEF_PE_EX_DLL_CHARACTERISTICS_H
#include <vector>

#include "LIEF/enums.hpp"
#include "LIEF/visibility.h"
#include "LIEF/PE/debug/Debug.hpp"

namespace LIEF {
namespace PE {

/// This class represents the `IMAGE_DEBUG_TYPE_EX_DLLCHARACTERISTICS` debug
/// entry
class LIEF_API ExDllCharacteristics : public Debug {
  public:
  /// Extended DLL Characteristics
  enum class CHARACTERISTICS : uint32_t {
    CET_COMPAT                                 = 0x01,
    CET_COMPAT_STRICT_MODE                     = 0x02,
    CET_SET_CONTEXT_IP_VALIDATION_RELAXED_MODE = 0x04,
    CET_DYNAMIC_APIS_ALLOW_IN_PROC             = 0x08,
    CET_RESERVED_1                             = 0x10,
    CET_RESERVED_2                             = 0x20,
    FORWARD_CFI_COMPAT                         = 0x40,
    HOTPATCH_COMPATIBLE                        = 0x80,
  };

  static std::unique_ptr<ExDllCharacteristics>
    parse(const details::pe_debug& hdr, Section* section, span<uint8_t> payload);

  ExDllCharacteristics(const details::pe_debug& debug, Section* sec,
                       uint32_t characteristics) :
    Debug(debug, sec),
    characteristics_(characteristics)
  {}

  ExDllCharacteristics(const ExDllCharacteristics& other) = default;
  ExDllCharacteristics& operator=(const ExDllCharacteristics& other) = default;

  ExDllCharacteristics(ExDllCharacteristics&&) = default;
  ExDllCharacteristics& operator=(ExDllCharacteristics&& other) = default;

  std::unique_ptr<Debug> clone() const override {
    return std::unique_ptr<Debug>(new ExDllCharacteristics(*this));
  }

  /// Return the characteristics
  CHARACTERISTICS characteristics() const {
    return (CHARACTERISTICS)characteristics_;
  }

  /// Characteristics as a vector
  std::vector<CHARACTERISTICS> characteristics_list() const;

  /// Check if the given CHARACTERISTICS is used
  bool has(CHARACTERISTICS c) const {
    return (characteristics_ & (uint32_t)c) != 0;
  }

  static bool classof(const Debug* debug) {
    return debug->type() == Debug::TYPES::EX_DLLCHARACTERISTICS;
  }

  ~ExDllCharacteristics() override = default;

  std::string to_string() const override;

  private:
  uint32_t characteristics_ = 0;
};

LIEF_API const char* to_string(ExDllCharacteristics::CHARACTERISTICS e);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::PE::ExDllCharacteristics::CHARACTERISTICS);

#endif
