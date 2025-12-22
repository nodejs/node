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
#include "LIEF/ELF/NoteDetails/properties/X86Feature.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "frozen.hpp"
#include "fmt_formatter.hpp"

#include "ELF/NoteDetails/properties/common.hpp"

FMT_FORMATTER(LIEF::ELF::X86Features::FEATURE, LIEF::ELF::to_string);
FMT_FORMATTER(LIEF::ELF::X86Features::FLAG, LIEF::ELF::to_string);

namespace LIEF {
namespace ELF {
std::unique_ptr<X86Features> X86Features::create_feat1(
    X86Features::FLAG flag, BinaryStream& stream
) {
  uint32_t bitmask = stream.read<uint32_t>().value_or(0);
  features_t features;
  while (bitmask) {
    uint32_t bit = bitmask & (- bitmask);
    bitmask &= ~bit;
    switch (bit) {
      case GNU_PROPERTY_X86_FEATURE_1_IBT: features.emplace_back(flag, FEATURE::IBT); break;
      case GNU_PROPERTY_X86_FEATURE_1_SHSTK: features.emplace_back(flag, FEATURE::SHSTK); break;
      case GNU_PROPERTY_X86_FEATURE_1_LAM_U48: features.emplace_back(flag, FEATURE::LAM_U48); break;
      case GNU_PROPERTY_X86_FEATURE_1_LAM_U57: features.emplace_back(flag, FEATURE::LAM_U57); break;
      default: features.emplace_back(flag, FEATURE::UNKNOWN); break;
    }
  }
  return std::unique_ptr<X86Features>(new X86Features(std::move(features)));
}

std::unique_ptr<X86Features> X86Features::create_feat2(
    X86Features::FLAG flag, BinaryStream& stream
) {
  uint32_t bitmask = stream.read<uint32_t>().value_or(0);
  features_t features;
  while (bitmask) {
    uint32_t bit = bitmask & (- bitmask);
    bitmask &= ~bit;
    switch (bit) {
      case GNU_PROPERTY_X86_FEATURE_2_X86: features.emplace_back(flag, FEATURE::X86); break;
      case GNU_PROPERTY_X86_FEATURE_2_X87: features.emplace_back(flag, FEATURE::X87); break;
      case GNU_PROPERTY_X86_FEATURE_2_MMX: features.emplace_back(flag, FEATURE::MMX); break;
      case GNU_PROPERTY_X86_FEATURE_2_XMM: features.emplace_back(flag, FEATURE::XMM); break;
      case GNU_PROPERTY_X86_FEATURE_2_YMM: features.emplace_back(flag, FEATURE::YMM); break;
      case GNU_PROPERTY_X86_FEATURE_2_ZMM: features.emplace_back(flag, FEATURE::ZMM); break;
      case GNU_PROPERTY_X86_FEATURE_2_TMM: features.emplace_back(flag, FEATURE::TMM); break;
      case GNU_PROPERTY_X86_FEATURE_2_MASK: features.emplace_back(flag, FEATURE::MASK); break;
      case GNU_PROPERTY_X86_FEATURE_2_FXSR: features.emplace_back(flag, FEATURE::FXSR); break;
      case GNU_PROPERTY_X86_FEATURE_2_XSAVE: features.emplace_back(flag, FEATURE::XSAVE); break;
      case GNU_PROPERTY_X86_FEATURE_2_XSAVEOPT: features.emplace_back(flag, FEATURE::XSAVEOPT); break;
      case GNU_PROPERTY_X86_FEATURE_2_XSAVEC: features.emplace_back(flag, FEATURE::XSAVEC); break;
      default: features.emplace_back(flag, FEATURE::UNKNOWN); break;
    }
  }
  return std::unique_ptr<X86Features>(new X86Features(std::move(features)));
}

std::unique_ptr<X86Features> X86Features::create(uint32_t type, BinaryStream& stream) {
  switch (type) {
    case GNU_PROPERTY_X86_FEATURE_1_AND: return create_feat1(FLAG::NONE, stream);

    case GNU_PROPERTY_X86_FEATURE_2_USED: return create_feat2(FLAG::USED, stream);
    case GNU_PROPERTY_X86_FEATURE_2_NEEDED: return create_feat2(FLAG::NEEDED, stream);

    default: return nullptr;
  }
}

void X86Features::dump(std::ostream &os) const {
  os << "x86/x86-64 features: " << fmt::to_string(features()) ;
}

const char* to_string(X86Features::FLAG flag) {
  #define ENTRY(X) std::pair(X86Features::FLAG::X, #X)
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

const char* to_string(X86Features::FEATURE feat) {
  #define ENTRY(X) std::pair(X86Features::FEATURE::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(IBT),
    ENTRY(SHSTK),
    ENTRY(LAM_U48),
    ENTRY(LAM_U57),
    ENTRY(X86),
    ENTRY(X87),
    ENTRY(MMX),
    ENTRY(XMM),
    ENTRY(YMM),
    ENTRY(ZMM),
    ENTRY(FXSR),
    ENTRY(XSAVE),
    ENTRY(XSAVEOPT),
    ENTRY(XSAVEC),
    ENTRY(TMM),
    ENTRY(MASK),
  };

  #undef ENTRY
  if (auto it = enums2str.find(feat); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}


}
}
