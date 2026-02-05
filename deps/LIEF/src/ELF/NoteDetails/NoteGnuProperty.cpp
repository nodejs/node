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
#include <algorithm>

#include "spdlog/fmt/fmt.h"

#include "logging.hpp"
#include "frozen.hpp"

#include "ELF/NoteDetails/properties/common.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/ELF/NoteDetails/NoteGnuProperty.hpp"
#include "LIEF/ELF/NoteDetails/properties/Generic.hpp"
#include "LIEF/ELF/NoteDetails/properties/AArch64Feature.hpp"
#include "LIEF/ELF/NoteDetails/properties/AArch64PAuth.hpp"
#include "LIEF/ELF/NoteDetails/properties/StackSize.hpp"
#include "LIEF/ELF/NoteDetails/properties/X86Feature.hpp"
#include "LIEF/ELF/NoteDetails/properties/X86ISA.hpp"
#include "LIEF/ELF/NoteDetails/properties/NoteNoCopyOnProtected.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"

namespace LIEF {
namespace ELF {

inline std::unique_ptr<NoteGnuProperty::Property>
parse_property(ARCH arch, SpanStream& stream) {

  auto res_type = stream.read<uint32_t>();
  if (!res_type) {
    LIEF_DEBUG("Can't read type at offset: 0x{:04x}", stream.pos());
    return nullptr;
  }

  auto data_size = stream.read<uint32_t>();
  if (!data_size) {
    LIEF_DEBUG("Can't read data size at offset: 0x{:04x}", stream.pos());
    return nullptr;
  }

  auto res_content = stream.slice(stream.pos(), *data_size);
  stream.increment_pos(*data_size);
  if (!res_content) {
    return nullptr;
  }

  SpanStream content = std::move(*res_content);
  const uint32_t type = *res_type;
  if (GNU_PROPERTY_LOPROC <= type && type <= GNU_PROPERTY_HIPROC) {
    if (arch == ARCH::X86_64 ||
        arch == ARCH::IAMCU ||
        arch == ARCH::I386)
    {
      switch (type) {
        case GNU_PROPERTY_X86_ISA_1_USED:
        case GNU_PROPERTY_X86_ISA_1_NEEDED:
        case GNU_PROPERTY_X86_COMPAT_ISA_1_USED:
        case GNU_PROPERTY_X86_COMPAT_ISA_1_NEEDED:
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_USED:
        case GNU_PROPERTY_X86_COMPAT_2_ISA_1_NEEDED:
          return X86ISA::create(type, content);
        case GNU_PROPERTY_X86_FEATURE_1_AND:
        case GNU_PROPERTY_X86_FEATURE_2_USED:
        case GNU_PROPERTY_X86_FEATURE_2_NEEDED:
          return X86Features::create(type, content);
        default: return Generic::create(type);
      }
    }

    if (arch == ARCH::AARCH64) {
      if (type == GNU_PROPERTY_AARCH64_FEATURE_1_AND) {
        return AArch64Feature::create(content);
      }

      if (type == GNU_PROPERTY_AARCH64_FEATURE_PAUTH) {
        return AArch64PAuth::create(content);
      }
      return Generic::create(type);
    }
  }

  switch (type) {
    case GNU_PROPERTY_STACK_SIZE:
      {
        const size_t csize = content.size();
        if (csize == sizeof(uint64_t)) {
          return StackSize::create(content.read<uint64_t>().value_or(0));
        }
        if (csize == sizeof(uint32_t)) {
          return StackSize::create(content.read<uint32_t>().value_or(0));
        }
        if (csize == sizeof(uint16_t)) {
          return StackSize::create(content.read<uint16_t>().value_or(0));
        }
        if (csize == sizeof(uint8_t)) {
          return StackSize::create(content.read<uint8_t>().value_or(0));
        }
        LIEF_ERR("Wrong stack size: {}", csize);
        return StackSize::create(0);
      }
    case GNU_PROPERTY_NO_COPY_ON_PROTECTED: return NoteNoCopyOnProtected::create();
    default:
      {
        if (
          (GNU_PROPERTY_UINT32_AND_LO <= type && type <= GNU_PROPERTY_UINT32_AND_HI) ||
          (GNU_PROPERTY_UINT32_OR_LO  <= type && type <= GNU_PROPERTY_UINT32_OR_HI)
        )
        {
          switch (type) {
            case GNU_PROPERTY_1_NEEDED: /* TODO(romain) */ return Generic::create(type);
            default: return Generic::create(type);
          }

        }
      }
  }
  return Generic::create(type);
}

NoteGnuProperty::properties_t NoteGnuProperty::properties() const {
  auto stream = SpanStream::from_vector(description_);
  if (!stream) {
    LIEF_WARN("Can't create stream");
    return {};
  }
  const bool is64 = arch_ == ARCH::IA_64 || arch_ == ARCH::AARCH64 ||
                    arch_ == ARCH::LOONGARCH || arch_ == ARCH::X86_64;

  properties_t props;

  while (stream) {
    if (auto prop = parse_property(arch_, *stream)) {
      props.push_back(std::move(prop));
      stream->align(is64 ? 8 : 4);
    } else {
      break;
    }
  }
  return props;
}


std::unique_ptr<NoteGnuProperty::Property>
NoteGnuProperty::find(Property::TYPE type) const {
  properties_t props = properties();
  auto it = std::find_if(props.begin(), props.end(),
      [type] (const std::unique_ptr<Property>& prop) {
        return prop->type() == type;
      }
  );

  if (it != props.end()) {
    return std::move(*it);
  }
  return nullptr;
}

void NoteGnuProperty::dump(std::ostream& os) const {
  Note::dump(os);
  os << '\n';
  const NoteGnuProperty::properties_t& props = properties();
  for (const std::unique_ptr<NoteGnuProperty::Property>& prop : props) {
    os << "  " << *prop << '\n';
  }
}

void NoteGnuProperty::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

const char* to_string(NoteGnuProperty::Property::TYPE type) {
  #define ENTRY(X) std::pair(NoteGnuProperty::Property::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(GENERIC),
    ENTRY(AARCH64_FEATURES),
    ENTRY(AARCH64_PAUTH),
    ENTRY(STACK_SIZE),
    ENTRY(NO_COPY_ON_PROTECTED),
    ENTRY(X86_ISA),
    ENTRY(X86_FEATURE),
    ENTRY(NEEDED),
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}

void NoteGnuProperty::Property::dump(std::ostream& os) const {
  os << to_string(this->type());
}

} // namespace ELF
} // namespace LIEF
