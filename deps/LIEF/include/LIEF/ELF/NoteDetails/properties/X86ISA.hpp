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
#ifndef LIEF_ELF_NOTE_DETAILS_PROPERTIES_X86ISA_H
#define LIEF_ELF_NOTE_DETAILS_PROPERTIES_X86ISA_H
#include <vector>
#include <utility>

#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace ELF {

/// This class interfaces the different ``GNU_PROPERTY_X86_ISA_*``
/// properties which includes:
/// - ``GNU_PROPERTY_X86_ISA_1_USED``
/// - ``GNU_PROPERTY_X86_ISA_1_NEEDED``
/// - ``GNU_PROPERTY_X86_COMPAT_ISA_1_USED``
/// - ``GNU_PROPERTY_X86_COMPAT_ISA_1_NEEDED``
/// - ``GNU_PROPERTY_X86_COMPAT_2_ISA_1_USED``
/// - ``GNU_PROPERTY_X86_COMPAT_2_ISA_1_NEEDED``
class LIEF_API X86ISA : public NoteGnuProperty::Property {
  public:
  enum class FLAG {
    NONE = 0,
    USED,
    NEEDED,
  };
  enum class ISA {
    UNKNOWN = 0,
    BASELINE,
    V2,
    V3,
    V4,

    CMOV,
    FMA,
    I486,
    I586,
    I686,
    SSE,
    SSE2,
    SSE3,
    SSSE3,
    SSE4_1,
    SSE4_2,
    AVX,
    AVX2,
    AVX512F,
    AVX512CD,
    AVX512ER,
    AVX512PF,
    AVX512VL,
    AVX512DQ,
    AVX512BW,
    AVX512_4FMAPS,
    AVX512_4VNNIW,
    AVX512_BITALG,
    AVX512_IFMA,
    AVX512_VBMI,
    AVX512_VBMI2,
    AVX512_VNNI,
    AVX512_BF16,
  };
  using values_t = std::vector<std::pair<FLAG, ISA>>;

  static bool classof(const NoteGnuProperty::Property* prop) {
    return prop->type() == NoteGnuProperty::Property::TYPE::X86_ISA;
  }

  static std::unique_ptr<X86ISA> create(uint32_t type, BinaryStream& stream);

  /// List of the ISA values in this property
  const values_t& values() const {
    return values_;
  }

  ~X86ISA() override = default;

  void dump(std::ostream &os) const override;

  protected:
  inline static std::unique_ptr<X86ISA>
    create_isa_1(FLAG flag, BinaryStream& stream);
  inline static std::unique_ptr<X86ISA>
    create_compat_isa_1(FLAG flag, BinaryStream& stream, bool is_compat2);
  X86ISA(values_t values) :
    NoteGnuProperty::Property(NoteGnuProperty::Property::TYPE::X86_ISA),
    values_(std::move(values))
  {}

  values_t values_;
};

LIEF_API const char* to_string(X86ISA::FLAG flag);
LIEF_API const char* to_string(X86ISA::ISA isa);

}
}

#endif
