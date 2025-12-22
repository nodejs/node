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
#ifndef LIEF_PE_FPO_H
#define LIEF_PE_FPO_H

#include <vector>
#include "LIEF/iterators.hpp"
#include "LIEF/visibility.h"
#include "LIEF/PE/debug/Debug.hpp"

namespace LIEF {
namespace PE {

/// This class represents the `IMAGE_DEBUG_TYPE_FPO` debug entry
class LIEF_API FPO : public Debug {
  public:

  enum class FRAME_TYPE {
    FPO = 0,
    TRAP = 1,
    TSS = 2,
    NON_FPO = 3,
  };

  /// Represents the stack frame layout for a x86 function when frame pointer
  /// omission (FPO) optimization is used.
  struct LIEF_API entry_t {
    /// The function RVA
    uint32_t rva = 0;

    /// The number of bytes in the function.
    uint32_t proc_size = 0;

    /// The number of local variables.
    uint32_t nb_locals = 0;

    /// The size of the parameters
    uint32_t parameters_size = 0;

    /// The number of bytes in the function prolog code.
    uint16_t prolog_size = 0;

    /// Number of registers saved
    uint16_t nb_saved_regs = 0;

    /// Whether the function uses structured exception handling.
    bool use_seh = false;

    /// Whether the EBP register has been allocated.
    bool use_bp = false;

    /// Reserved for future use
    uint16_t reserved = 0;

    /// Variable that indicates the frame type.
    FRAME_TYPE type = FRAME_TYPE::FPO;

    std::string to_string() const;

    friend LIEF_API
      std::ostream& operator<<(std::ostream& os, const entry_t& entry)
    {
      os << entry.to_string();
      return os;
    }
  };

  using entries_t = std::vector<entry_t>;
  using it_entries = ref_iterator<entries_t&>;
  using it_const_entries = const_ref_iterator<const entries_t&>;

  static std::unique_ptr<FPO>
    parse(const details::pe_debug& hdr, Section* section, span<uint8_t> payload);

  FPO(const details::pe_debug& hdr, Section* section) :
    Debug(hdr, section)
  {}

  FPO(const FPO& other) = default;
  FPO& operator=(const FPO& other) = default;

  FPO(FPO&&) = default;
  FPO& operator=(FPO&& other) = default;

  std::unique_ptr<Debug> clone() const override {
    return std::unique_ptr<Debug>(new FPO(*this));
  }

  /// Iterator over the FPO entries
  it_const_entries entries() const {
    return entries_;
  }

  it_entries entries() {
    return entries_;
  }

  static bool classof(const Debug* debug) {
    return debug->type() == Debug::TYPES::FPO;
  }

  ~FPO() override = default;

  std::string to_string() const override;

  private:
  entries_t entries_;
};

LIEF_API const char* to_string(FPO::FRAME_TYPE e);

}
}

#endif
