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
#include "frozen.hpp"
#include "internal_utils.hpp"

#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/NoteDetails/NoteAbi.hpp"

#include "spdlog/fmt/fmt.h"

namespace LIEF {
namespace ELF {

result<NoteAbi::version_t> NoteAbi::version() const {
  NoteAbi::version_t version;
  for (size_t i = 0; i < version.size(); ++i) {
    auto res = read_at<uint32_t>(version_offset + i * sizeof(uint32_t));
    if (!res) {
      return make_error_code(lief_errors::read_error);
    }
    version[i] = *res;
  }
  return version;
}

result<NoteAbi::ABI> NoteAbi::abi() const {
  auto value = read_at<uint32_t>(abi_offset);
  if (!value) {
    return make_error_code(get_error(value));
  }
  return static_cast<ABI>(*value);
}

void NoteAbi::version(const version_t& version) {
  write_at<uint32_t>(version_offset + 0 * sizeof(uint32_t), version[0]);
  write_at<uint32_t>(version_offset + 1 * sizeof(uint32_t), version[1]);
  write_at<uint32_t>(version_offset + 2 * sizeof(uint32_t), version[2]);
}

void NoteAbi::version(ABI abi) {
  write_at<uint32_t>(abi_offset, static_cast<uint32_t>(abi));
}

void NoteAbi::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void NoteAbi::dump(std::ostream& os) const {
  Note::dump(os);
  os << '\n';
  auto version_res = version().value_or(version_t({0, 0, 0}));
  auto abi_res = to_string_or(abi(), "???");
  os << fmt::format("   {}.{}.{} '{}'",
    version_res[0], version_res[1], version_res[2], abi_res
  );
}


const char* to_string(NoteAbi::ABI abi) {
  #define ENTRY(X) std::pair(NoteAbi::ABI::X, #X)
  STRING_MAP enums2str {
    ENTRY(LINUX),
    ENTRY(GNU),
    ENTRY(SOLARIS2),
    ENTRY(FREEBSD),
    ENTRY(NETBSD),
    ENTRY(SYLLABLE),
    ENTRY(NACL),
  };
  #undef ENTRY

  if (auto it = enums2str.find(abi); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";

}

} // namespace ELF
} // namespace LIEF
