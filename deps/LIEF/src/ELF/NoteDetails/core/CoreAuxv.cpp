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


#include "LIEF/ELF/hash.hpp"
#include "LIEF/ELF/NoteDetails/core/CoreAuxv.hpp"
#include "LIEF/Visitor.hpp"
#include "LIEF/iostream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "frozen.hpp"
#include "spdlog/fmt/fmt.h"
#include "ELF/Structures.hpp"

namespace LIEF {
namespace ELF {

template<class ELF_T> inline result<uint64_t>
get_impl(CoreAuxv::TYPE type, const Note::description_t& desc) {
  using Elf_Auxv  = typename ELF_T::Elf_Auxv;
  auto stream = SpanStream::from_vector(desc);
  if (!stream) {
    return make_error_code(get_error(stream));
  }
  while (*stream) {
    auto auxv = stream->read<Elf_Auxv>();
    if (!auxv) {
      return make_error_code(lief_errors::not_found);
    }
    const auto atype = static_cast<CoreAuxv::TYPE>(auxv->a_type);
    auto value = auxv->a_un.a_val;
    if (atype == CoreAuxv::TYPE::END) {
      return make_error_code(lief_errors::not_found);
    }
    if (atype == type) {
      return static_cast<uint64_t>(value);
    }
  }
  return make_error_code(lief_errors::not_found);
}

template<class ELF_T> inline std::map<CoreAuxv::TYPE, uint64_t>
get_values_impl(const Note::description_t& desc) {
  using Elf_Auxv  = typename ELF_T::Elf_Auxv;
  auto stream = SpanStream::from_vector(desc);
  if (!stream) {
    return {};
  }

  std::map<CoreAuxv::TYPE, uint64_t> values;
  while (*stream) {
    auto auxv = stream->read<Elf_Auxv>();
    if (!auxv) {
      return values;
    }
    const auto atype = static_cast<CoreAuxv::TYPE>(auxv->a_type);
    auto value = auxv->a_un.a_val;
    if (atype == CoreAuxv::TYPE::END) {
      return values;
    }

    values[atype] = static_cast<uint64_t>(value);
  }
  return values;
}

template<class ELF_T>
inline bool write_impl(Note::description_t& description,
                       const std::map<CoreAuxv::TYPE, uint64_t>& values)
{
  using Elf_Auxv  = typename ELF_T::Elf_Auxv;
  using ptr_t     = typename ELF_T::uint;
  vector_iostream io;
  io.reserve(values.size() * sizeof(Elf_Auxv));

  for (const auto& [type, value] : values) {
    // This will be added at the end
    if (type == CoreAuxv::TYPE::END) {
      continue;
    }
    io.write(static_cast<ptr_t>(type))
      .write(static_cast<ptr_t>(value));
  }
  io.write(static_cast<ptr_t>(CoreAuxv::TYPE::END))
    .write(static_cast<ptr_t>(0));

  io.move(description);
  return true;
}


result<uint64_t> CoreAuxv::get(TYPE type) const {
  return class_ == Header::CLASS::ELF32 ?
                   get_impl<details::ELF32>(type, description_) :
                   get_impl<details::ELF64>(type, description_);
}

std::map<CoreAuxv::TYPE, uint64_t> CoreAuxv::values() const {
  return class_ == Header::CLASS::ELF32 ?
                   get_values_impl<details::ELF32>(description_) :
                   get_values_impl<details::ELF64>(description_);
}


bool CoreAuxv::set(TYPE type, uint64_t value) {
  std::map<TYPE, uint64_t> vals = values();
  vals[type] = value;
  return set(vals);
}

bool CoreAuxv::set(const std::map<TYPE, uint64_t>& values) {
  return class_ == Header::CLASS::ELF32 ?
                   write_impl<details::ELF32>(description_, values) :
                   write_impl<details::ELF64>(description_, values);
}

void CoreAuxv::dump(std::ostream& os) const {
  Note::dump(os);

  const auto& aux_vals = values();
  if (aux_vals.empty()) {
    return;
  }

  os << '\n';
  for (const auto& [type, val] : aux_vals) {
    os << fmt::format("  {}: 0x{:08x}\n", to_string(type), val);
  }
}

void CoreAuxv::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

const char* to_string(CoreAuxv::TYPE type) {
  #define ENTRY(X) std::pair(CoreAuxv::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(END),
    ENTRY(IGNORE_TY),
    ENTRY(EXECFD),
    ENTRY(PHDR),
    ENTRY(PHENT),
    ENTRY(PHNUM),
    ENTRY(PAGESZ),
    ENTRY(BASE),
    ENTRY(FLAGS),
    ENTRY(ENTRY),
    ENTRY(NOTELF),
    ENTRY(UID),
    ENTRY(EUID),
    ENTRY(GID),
    ENTRY(EGID),
    ENTRY(TGT_PLATFORM),
    ENTRY(HWCAP),
    ENTRY(CLKTCK),
    ENTRY(FPUCW),
    ENTRY(DCACHEBSIZE),
    ENTRY(ICACHEBSIZE),
    ENTRY(UCACHEBSIZE),
    ENTRY(IGNOREPPC),
    ENTRY(SECURE),
    ENTRY(BASE_PLATFORM),
    ENTRY(RANDOM),
    ENTRY(HWCAP2),
    ENTRY(EXECFN),
    ENTRY(SYSINFO),
    ENTRY(SYSINFO_EHDR),
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}

} // namespace ELF
} // namespace LIEF
