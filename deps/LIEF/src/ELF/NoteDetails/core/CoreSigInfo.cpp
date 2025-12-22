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
#include "LIEF/ELF/NoteDetails/core/CoreSigInfo.hpp"
#include "ELF/Structures.hpp"

#include "spdlog/fmt/fmt.h"

namespace LIEF {
namespace ELF {

static constexpr auto signo_offset = offsetof(details::Elf_siginfo, si_signo);
static constexpr auto sigcode_offset = offsetof(details::Elf_siginfo, si_code);
static constexpr auto si_errno_offset = offsetof(details::Elf_siginfo, si_errno);

result<int32_t> CoreSigInfo::signo() const {
  return read_at<uint32_t>(signo_offset);
}

result<int32_t> CoreSigInfo::sigcode() const {
  return read_at<uint32_t>(sigcode_offset);
}

result<int32_t> CoreSigInfo::sigerrno() const {
  return read_at<uint32_t>(si_errno_offset);
}

void CoreSigInfo::signo(uint32_t value) {
  write_at(signo_offset, value);
}

void CoreSigInfo::sigcode(uint32_t value) {
  write_at(sigcode_offset, value);
}

void CoreSigInfo::sigerrno(uint32_t value) {
  write_at(si_errno_offset, value);
}

void CoreSigInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void CoreSigInfo::dump(std::ostream& os) const {
  Note::dump(os);
  os << '\n'
     << fmt::format("  signo: {} code: {} errno: {}\n",
                    signo().value_or(-1),
                    sigcode().value_or(-1),
                    sigerrno().value_or(-1));
}

} // namespace ELF
} // namespace LIEF
