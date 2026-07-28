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
#include "LIEF/ELF/NoteDetails/properties/X86ISA.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "ELF/NoteDetails/properties/common.hpp"

#include "frozen.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::ELF::X86ISA::FLAG, LIEF::ELF::to_string);
FMT_FORMATTER(LIEF::ELF::X86ISA::ISA, LIEF::ELF::to_string);

namespace LIEF {
namespace ELF {

std::unique_ptr<X86ISA> X86ISA::create_isa_1(X86ISA::FLAG flag, BinaryStream& stream) {
  uint32_t bitmask = stream.read<uint32_t>().value_or(0);

  values_t values;
  while (bitmask) {
    uint32_t bit = bitmask & (- bitmask);
    bitmask &= ~bit;

    switch (bit) {
      case GNU_PROPERTY_X86_ISA_1_BASELINE: values.emplace_back(flag, ISA::BASELINE); break;
      case GNU_PROPERTY_X86_ISA_1_V2:       values.emplace_back(flag, ISA::V2); break;
      case GNU_PROPERTY_X86_ISA_1_V3:       values.emplace_back(flag, ISA::V3); break;
      case GNU_PROPERTY_X86_ISA_1_V4:       values.emplace_back(flag, ISA::V4); break;

      default: values.emplace_back(flag, ISA::UNKNOWN); break;
    }
  }

  return std::unique_ptr<X86ISA>(new X86ISA(std::move(values)));
}

std::unique_ptr<X86ISA> X86ISA::create_compat_isa_1(
    X86ISA::FLAG flag, BinaryStream& stream, bool is_compat2
) {
  uint32_t bitmask = stream.read<uint32_t>().value_or(0);

  values_t values;
  while (bitmask) {
    uint32_t bit = bitmask & (- bitmask);
    bitmask &= ~bit;

    if (is_compat2) {
      switch (bit) {
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_CMOV:           values.emplace_back(flag, ISA::CMOV); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE:            values.emplace_back(flag, ISA::SSE); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE2:           values.emplace_back(flag, ISA::SSE2); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE3:           values.emplace_back(flag, ISA::SSE3); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSSE3:          values.emplace_back(flag, ISA::SSSE3); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE4_1:         values.emplace_back(flag, ISA::SSE4_1); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_SSE4_2:         values.emplace_back(flag, ISA::SSE4_2); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX:            values.emplace_back(flag, ISA::AVX); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX2:           values.emplace_back(flag, ISA::AVX2); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_FMA:            values.emplace_back(flag, ISA::FMA); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512F:        values.emplace_back(flag, ISA::AVX512F); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512CD:       values.emplace_back(flag, ISA::AVX512CD); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512ER:       values.emplace_back(flag, ISA::AVX512ER); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512PF:       values.emplace_back(flag, ISA::AVX512PF); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512VL:       values.emplace_back(flag, ISA::AVX512VL); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512DQ:       values.emplace_back(flag, ISA::AVX512DQ); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512BW:       values.emplace_back(flag, ISA::AVX512BW); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_4FMAPS:  values.emplace_back(flag, ISA::AVX512_4FMAPS); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_4VNNIW:  values.emplace_back(flag, ISA::AVX512_4VNNIW); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_BITALG:  values.emplace_back(flag, ISA::AVX512_BITALG); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_IFMA:    values.emplace_back(flag, ISA::AVX512_IFMA); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_VBMI:    values.emplace_back(flag, ISA::AVX512_VBMI); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_VBMI2:   values.emplace_back(flag, ISA::AVX512_VBMI2); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_VNNI:    values.emplace_back(flag, ISA::AVX512_VNNI); break;
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_AVX512_BF16:    values.emplace_back(flag, ISA::AVX512_BF16); break;

        default: values.emplace_back(flag, ISA::UNKNOWN); break;
      }
    } else {
      switch (bit) {
        case GNU_PROPERTY_X86_COMPAT_ISA_1_486:      values.emplace_back(flag, ISA::I486); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_586:      values.emplace_back(flag, ISA::I586); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_686:      values.emplace_back(flag, ISA::I686); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_SSE:      values.emplace_back(flag, ISA::SSE); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_SSE2:     values.emplace_back(flag, ISA::SSE2); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_SSE3:     values.emplace_back(flag, ISA::SSE3); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_SSSE3:    values.emplace_back(flag, ISA::SSSE3); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_SSE4_1:   values.emplace_back(flag, ISA::SSE4_1); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_SSE4_2:   values.emplace_back(flag, ISA::SSE4_2); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX:      values.emplace_back(flag, ISA::AVX); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX2:     values.emplace_back(flag, ISA::AVX2); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512F:  values.emplace_back(flag, ISA::AVX512F); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512CD: values.emplace_back(flag, ISA::AVX512CD); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512ER: values.emplace_back(flag, ISA::AVX512ER); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512PF: values.emplace_back(flag, ISA::AVX512PF); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512VL: values.emplace_back(flag, ISA::AVX512VL); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512DQ: values.emplace_back(flag, ISA::AVX512DQ); break;
        case GNU_PROPERTY_X86_COMPAT_ISA_1_AVX512BW: values.emplace_back(flag, ISA::AVX512BW); break;

        default: values.emplace_back(flag, ISA::UNKNOWN); break;
      }
    }
  }

  return std::unique_ptr<X86ISA>(new X86ISA(std::move(values)));
}


std::unique_ptr<X86ISA> X86ISA::create(uint32_t type, BinaryStream& stream) {
  switch (type) {
    case GNU_PROPERTY_X86_ISA_1_USED: return create_isa_1(FLAG::USED, stream);
    case GNU_PROPERTY_X86_ISA_1_NEEDED: return create_isa_1(FLAG::NEEDED, stream);

    case GNU_PROPERTY_X86_COMPAT_ISA_1_USED: return create_compat_isa_1(FLAG::USED, stream, /*is_compat2*/ false);
    case GNU_PROPERTY_X86_COMPAT_ISA_1_NEEDED: return create_compat_isa_1(FLAG::NEEDED, stream,  /*is_compat2*/ false);

    case GNU_PROPERTY_X86_COMPAT_2_ISA_1_USED: return create_compat_isa_1(FLAG::USED, stream, /*is_compat2*/ true);
    case GNU_PROPERTY_X86_COMPAT_2_ISA_1_NEEDED: return create_compat_isa_1(FLAG::NEEDED, stream, /*is_compat2*/ true);

    default: return nullptr;
  }
}

void X86ISA::dump(std::ostream &os) const {
  os << "x86/x86-64 ISA: " << fmt::to_string(values());
}

const char* to_string(X86ISA::FLAG flag) {
#define ENTRY(X) std::pair(X86ISA::FLAG::X, #X)
  STRING_MAP enums2str {
    ENTRY(NONE),
    ENTRY(NEEDED),
    ENTRY(USED),
  };
#undef ENTRY
  if (auto it = enums2str.find(flag); it != enums2str.end()) {
    return it->second;
  }

  return "NONE";
}

const char* to_string(X86ISA::ISA isa) {
#define ENTRY(X) std::pair(X86ISA::ISA::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(BASELINE),
    ENTRY(V2),
    ENTRY(V3),
    ENTRY(V4),

    ENTRY(CMOV),
    ENTRY(FMA),
    ENTRY(I486),
    ENTRY(I586),
    ENTRY(I686),
    ENTRY(SSE),
    ENTRY(SSE2),
    ENTRY(SSE3),
    ENTRY(SSSE3),
    ENTRY(SSE4_1),
    ENTRY(SSE4_2),
    ENTRY(AVX),
    ENTRY(AVX2),
    ENTRY(AVX512F),
    ENTRY(AVX512CD),
    ENTRY(AVX512ER),
    ENTRY(AVX512PF),
    ENTRY(AVX512VL),
    ENTRY(AVX512DQ),
    ENTRY(AVX512BW),
    ENTRY(AVX512_4FMAPS),
    ENTRY(AVX512_4VNNIW),
    ENTRY(AVX512_BITALG),
    ENTRY(AVX512_IFMA),
    ENTRY(AVX512_VBMI),
    ENTRY(AVX512_VBMI2),
    ENTRY(AVX512_VNNI),
    ENTRY(AVX512_BF16),
  };
#undef ENTRY
  if (auto it = enums2str.find(isa); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}
}
}
