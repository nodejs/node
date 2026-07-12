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

#include "logging.hpp"
#include "frozen.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/iostream.hpp"

#include "LIEF/ELF/hash.hpp"
#include "LIEF/ELF/EnumToString.hpp"
#include "LIEF/ELF/NoteDetails/core/CorePrStatus.hpp"
#include "ELF/Structures.hpp"

namespace LIEF {
namespace ELF {

template<class ELF_T>
inline CorePrStatus::pr_status_t
get_status_impl(const Note::description_t& description) {
  using Elf_Prstatus  = typename ELF_T::Elf_Prstatus;
  CorePrStatus::pr_status_t status;

  auto stream = SpanStream::from_vector(description);
  if (!stream) {
    return {};
  }

  auto raw_pr_status = stream->read<Elf_Prstatus>();
  if (!raw_pr_status) {
    return {};
  }

  status.info.signo = raw_pr_status->pr_info.si_signo;
  status.info.code  = raw_pr_status->pr_info.si_code;
  status.info.err = raw_pr_status->pr_info.si_errno;

  status.cursig  = raw_pr_status->pr_cursig;
  status.reserved   = raw_pr_status->reserved;
  status.sigpend = raw_pr_status->pr_sigpend;
  status.sighold = raw_pr_status->pr_sighold;

  status.pid  = raw_pr_status->pr_pid;
  status.ppid = raw_pr_status->pr_ppid;
  status.pgrp = raw_pr_status->pr_pgrp;
  status.sid  = raw_pr_status->pr_sid;

  status.utime.sec = raw_pr_status->pr_utime.tv_sec;
  status.utime.usec = raw_pr_status->pr_utime.tv_usec;

  status.stime.sec = raw_pr_status->pr_stime.tv_sec;
  status.stime.usec = raw_pr_status->pr_stime.tv_usec;

  status.cutime.sec = raw_pr_status->pr_cutime.tv_sec;
  status.cutime.usec = raw_pr_status->pr_cutime.tv_usec;

  status.cstime.sec = raw_pr_status->pr_cstime.tv_sec;
  status.cstime.usec = raw_pr_status->pr_cstime.tv_usec;

  return status;
}

template<class ELF_T>
inline Note::description_t write_status_impl(const CorePrStatus::pr_status_t& status) {
  using Elf_Prstatus  = typename ELF_T::Elf_Prstatus;
  Elf_Prstatus raw_pr_status;

  raw_pr_status.pr_info.si_signo = status.info.signo;
  raw_pr_status.pr_info.si_code = status.info.code;
  raw_pr_status.pr_info.si_errno = status.info.err;

  raw_pr_status.pr_cursig = status.cursig;
  raw_pr_status.reserved = status.reserved;
  raw_pr_status.pr_sigpend = static_cast<decltype(raw_pr_status.pr_sigpend)>(status.sigpend);
  raw_pr_status.pr_sighold = static_cast<decltype(raw_pr_status.pr_sighold)>(status.sighold);

  raw_pr_status.pr_pid = status.pid;
  raw_pr_status.pr_ppid = status.ppid;
  raw_pr_status.pr_pgrp = status.pgrp;
  raw_pr_status.pr_sid = status.sid;

  raw_pr_status.pr_utime.tv_sec = status.utime.sec;
  raw_pr_status.pr_utime.tv_usec = status.utime.usec;

  raw_pr_status.pr_cutime.tv_sec = status.cutime.sec;
  raw_pr_status.pr_cutime.tv_usec = status.cutime.usec;

  raw_pr_status.pr_stime.tv_sec = status.stime.sec;
  raw_pr_status.pr_stime.tv_usec = status.stime.usec;

  raw_pr_status.pr_cstime.tv_sec = status.cstime.sec;
  raw_pr_status.pr_cstime.tv_usec = status.cstime.usec;

  vector_iostream ios;
  ios.write<Elf_Prstatus>(raw_pr_status);
  return ios.raw();
}

template<class REG_T>
inline result<uint64_t>
get_reg_impl(REG_T reg, const Note::description_t& description,
             Header::CLASS cls, ARCH arch)
{
  if constexpr (std::is_same_v<REG_T, CorePrStatus::Registers::X86>)
  {
    if (arch != ARCH::I386) {
      return make_error_code(lief_errors::not_found);
    }
  }
  else if constexpr (std::is_same_v<REG_T, CorePrStatus::Registers::X86_64>)
  {
    if (arch != ARCH::X86_64) {
      return make_error_code(lief_errors::not_found);
    }
  }
  else if constexpr (std::is_same_v<REG_T, CorePrStatus::Registers::ARM>)
  {
    if (arch != ARCH::ARM) {
      return make_error_code(lief_errors::not_found);
    }
  }
  else if constexpr (std::is_same_v<REG_T, CorePrStatus::Registers::AARCH64>)
  {
    if (arch != ARCH::AARCH64) {
      return make_error_code(lief_errors::not_found);
    }
  }
  else {
    LIEF_WARN("Architecture not supported");
    return make_error_code(lief_errors::not_found);
  }

  auto pos = static_cast<int32_t>(reg);
  if (pos < 0 || pos >= static_cast<int32_t>(REG_T::_COUNT)) {
    return make_error_code(lief_errors::not_found);
  }

  auto stream = SpanStream::from_vector(description);
  if (!stream) {
    return make_error_code(lief_errors::not_found);
  }

  if (cls == Header::CLASS::ELF32) {
    stream->increment_pos(sizeof(details::ELF32::Elf_Prstatus));
    stream->increment_pos(pos * sizeof(uint32_t));
    auto value = stream->read<uint32_t>();
    if (!value) {
      return make_error_code(lief_errors::corrupted);
    }
    return *value;
  }

  if (cls == Header::CLASS::ELF64) {
    stream->increment_pos(sizeof(details::ELF64::Elf_Prstatus));
    stream->increment_pos(pos * sizeof(uint64_t));
    auto value = stream->read<uint64_t>();
    if (!value) {
      return make_error_code(lief_errors::corrupted);
    }
    return *value;
  }

  return make_error_code(lief_errors::not_found);
}

template<class REG_T>
inline ok_error_t
set_reg_impl(REG_T reg, uint64_t value, Note::description_t& description,
             Header::CLASS cls, ARCH arch)
{
  if constexpr (std::is_same_v<REG_T, CorePrStatus::Registers::X86>)
  {
    if (arch != ARCH::I386) {
      return make_error_code(lief_errors::not_found);
    }
  }
  else if constexpr (std::is_same_v<REG_T, CorePrStatus::Registers::X86_64>)
  {
    if (arch != ARCH::X86_64) {
      return make_error_code(lief_errors::not_found);
    }
  }
  else if constexpr (std::is_same_v<REG_T, CorePrStatus::Registers::ARM>)
  {
    if (arch != ARCH::ARM) {
      return make_error_code(lief_errors::not_found);
    }
  }
  else if constexpr (std::is_same_v<REG_T, CorePrStatus::Registers::AARCH64>)
  {
    if (arch != ARCH::AARCH64) {
      return make_error_code(lief_errors::not_found);
    }
  }
  else {
    LIEF_WARN("Architecture not supported");
    return make_error_code(lief_errors::not_found);
  }

  auto pos = static_cast<int32_t>(reg);
  if (pos < 0 || pos >= static_cast<int32_t>(REG_T::_COUNT)) {
    return make_error_code(lief_errors::not_found);
  }

  size_t offset = 0;
  vector_iostream os;
  os.write(description);

  if (cls == Header::CLASS::ELF32) {
    offset += sizeof(details::ELF32::Elf_Prstatus) + pos * sizeof(uint32_t);
    os.seekp(offset);
    os.write<uint32_t>(value);
    return ok();
  }

  if (cls == Header::CLASS::ELF64) {
    offset += sizeof(details::ELF64::Elf_Prstatus) + pos * sizeof(uint64_t);
    os.seekp(offset);
    os.write<uint64_t>(value);
    os.move(description);
    return ok();
  }

  return make_error_code(lief_errors::not_found);
}

std::vector<uint64_t> CorePrStatus::register_values() const {
  std::vector<uint64_t> values;
  switch (arch_) {
    case ARCH::X86_64:
      {
        using Reg = Registers::X86_64;
        const auto count = static_cast<size_t>(Reg::_COUNT);
        values.reserve(count);
        for (size_t i = 0; i < count; ++i) {
          if (auto val = get(Reg(i))) {
            values.push_back(*val);
          } else {
            return {};
          }
        }
        return values;
      }
    case ARCH::I386:
      {
        using Reg = Registers::X86;
        const auto count = static_cast<size_t>(Reg::_COUNT);
        values.reserve(count);
        for (size_t i = 0; i < count; ++i) {
          if (auto val = get(Reg(i))) {
            values.push_back(*val);
          } else {
            return {};
          }
        }
        return values;
      }
    case ARCH::ARM:
      {
        using Reg = Registers::ARM;
        const auto count = static_cast<size_t>(Reg::_COUNT);
        values.reserve(count);
        for (size_t i = 0; i < count; ++i) {
          if (auto val = get(Reg(i))) {
            values.push_back(*val);
          } else {
            return {};
          }
        }
        return values;
      }
    case ARCH::AARCH64:
      {
        using Reg = Registers::AARCH64;
        const auto count = static_cast<size_t>(Reg::_COUNT);
        values.reserve(count);
        for (size_t i = 0; i < count; ++i) {
          if (auto val = get(Reg(i))) {
            values.push_back(*val);
          } else {
            return {};
          }
        }
        return values;
      }
    default: return {};
  }
}

CorePrStatus::pr_status_t CorePrStatus::status() const {
  if (class_ == Header::CLASS::ELF32) {
    return get_status_impl<details::ELF32>(description_);
  }
  return get_status_impl<details::ELF64>(description_);
}

void CorePrStatus::status(const pr_status_t& status) {
  Note::description_t description = class_ == Header::CLASS::ELF32 ?
      write_status_impl<details::ELF32>(status) :
      write_status_impl<details::ELF64>(status);

  if (description.empty()) {
    return;
  }

  if (description_.size() < description.size()) {
    description_.resize(description.size());
  }

  std::move(description.begin(), description.end(), description_.begin());
}

result<uint64_t> CorePrStatus::get(Registers::X86 reg) const {
  return get_reg_impl<Registers::X86>(reg, description_, class_, arch_);
}

result<uint64_t> CorePrStatus::get(Registers::X86_64 reg) const {
  return get_reg_impl<Registers::X86_64>(reg, description_, class_, arch_);
}

result<uint64_t> CorePrStatus::get(Registers::ARM reg) const {
  return get_reg_impl<Registers::ARM>(reg, description_, class_, arch_);
}

result<uint64_t> CorePrStatus::get(Registers::AARCH64 reg) const {
  return get_reg_impl<Registers::AARCH64>(reg, description_, class_, arch_);
}

ok_error_t CorePrStatus::set(Registers::X86 reg, uint64_t value) {
  return set_reg_impl<Registers::X86>(reg, value, description_, class_, arch_);
}

ok_error_t CorePrStatus::set(Registers::X86_64 reg, uint64_t value) {
  return set_reg_impl<Registers::X86_64>(reg, value, description_, class_, arch_);
}

ok_error_t CorePrStatus::set(Registers::ARM reg, uint64_t value) {
  return set_reg_impl<Registers::ARM>(reg, value, description_, class_, arch_);
}

ok_error_t CorePrStatus::set(Registers::AARCH64 reg, uint64_t value) {
  return set_reg_impl<Registers::AARCH64>(reg, value, description_, class_, arch_);
}


result<uint64_t> CorePrStatus::pc() const {
  switch (arch_) {
    case ARCH::AARCH64: return get(Registers::AARCH64::PC);
    case ARCH::ARM: return get(Registers::ARM::R15);
    case ARCH::I386: return get(Registers::X86::EIP);
    case ARCH::X86_64: return get(Registers::X86_64::RIP);
    default: return make_error_code(lief_errors::not_supported);
  }
}

result<uint64_t> CorePrStatus::sp() const {
  switch (arch_) {
    case ARCH::AARCH64: return get(Registers::AARCH64::X31);
    case ARCH::ARM: return get(Registers::ARM::R13);
    case ARCH::I386: return get(Registers::X86::ESP);
    case ARCH::X86_64: return get(Registers::X86_64::RSP);
    default: return make_error_code(lief_errors::not_supported);
  }
}

result<uint64_t> CorePrStatus::return_value() const {
  switch (arch_) {
    case ARCH::AARCH64: return get(Registers::AARCH64::X0);
    case ARCH::ARM: return get(Registers::ARM::R0);
    case ARCH::I386: return get(Registers::X86::EAX);
    case ARCH::X86_64: return get(Registers::X86_64::RAX);
    default: return make_error_code(lief_errors::not_supported);
  }
}

template<class REG>
void dump_impl(std::ostream& os, const std::vector<uint64_t>& reg_vals) {
  for (size_t i = 0; i < reg_vals.size(); ++i) {
    if constexpr (std::is_void_v<REG>) {
      os << fmt::format("   0x{:08x}\n", reg_vals[i]);
    } else {
      os << fmt::format("   {}: 0x{:08x}\n", to_string(REG(i)), reg_vals[i]);
    }
  }
}

void CorePrStatus::dump(std::ostream& os) const {
  Note::dump(os);
  const CorePrStatus::pr_status_t& status = this->status();
  os << '\n'
     << fmt::format("  PID: {:04d} PPID: {:04d} PGRP: {:04d}\n",
                    status.pid, status.ppid, status.pgrp)
     << fmt::format("  SID: {:04d} SIGNO: {:04d} SIGCODE: {:04d}\n",
                    status.sid, status.info.signo, status.info.code)
     << fmt::format("  SIGERR: {:04d} SIGPEND: {:04d} SIGHOLD: {:04d}\n",
                    status.info.err, status.sigpend, status.sighold)
     << fmt::format("  CURRSIG: 0x{:04d} reserved: {}\n",
                    status.cursig, status.reserved);
  const std::vector<uint64_t>& reg_vals = register_values();
  switch (architecture()) {
    case ARCH::ARM:
      dump_impl<CorePrStatus::Registers::ARM>(os, reg_vals); break;
    case ARCH::AARCH64:
      dump_impl<CorePrStatus::Registers::AARCH64>(os, reg_vals); break;
    case ARCH::I386:
      dump_impl<CorePrStatus::Registers::X86>(os, reg_vals); break;
    case ARCH::X86_64:
      dump_impl<CorePrStatus::Registers::X86_64>(os, reg_vals); break;
    default:
      dump_impl<void>(os, reg_vals); break;

  }
}

void CorePrStatus::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

const char* to_string(CorePrStatus::Registers::X86 e) {
  #define ENTRY(X) std::pair(CorePrStatus::Registers::X86::X, #X)
  STRING_MAP enum_strings {
    ENTRY(EBX),
    ENTRY(ECX),
    ENTRY(EDX),
    ENTRY(ESI),
    ENTRY(EDI),
    ENTRY(EBP),
    ENTRY(EAX),
    ENTRY(DS),
    ENTRY(ES),
    ENTRY(FS),
    ENTRY(GS),
    ENTRY(ORIG_EAX),
    ENTRY(EIP),
    ENTRY(CS),
    ENTRY(EFLAGS),
    ENTRY(ESP),
    ENTRY(SS),
  };
  #undef ENTRY

  if (auto it = enum_strings.find(e); it != enum_strings.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(CorePrStatus::Registers::X86_64 e) {
  #define ENTRY(X) std::pair(CorePrStatus::Registers::X86_64::X, #X)
  STRING_MAP enums2str {
    ENTRY(R15),
    ENTRY(R14),
    ENTRY(R13),
    ENTRY(R12),
    ENTRY(RBP),
    ENTRY(RBX),
    ENTRY(R11),
    ENTRY(R10),
    ENTRY(R9),
    ENTRY(R8),
    ENTRY(RAX),
    ENTRY(RCX),
    ENTRY(RDX),
    ENTRY(RSI),
    ENTRY(RDI),
    ENTRY(ORIG_RAX),
    ENTRY(RIP),
    ENTRY(CS),
    ENTRY(EFLAGS),
    ENTRY(RSP),
    ENTRY(SS),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(CorePrStatus::Registers::ARM e) {
  #define ENTRY(X) std::pair(CorePrStatus::Registers::ARM::X, #X)
  STRING_MAP enums2str {
    ENTRY(R0),
    ENTRY(R1),
    ENTRY(R2),
    ENTRY(R3),
    ENTRY(R4),
    ENTRY(R5),
    ENTRY(R6),
    ENTRY(R7),
    ENTRY(R8),
    ENTRY(R9),
    ENTRY(R10),
    ENTRY(R11),
    ENTRY(R12),
    ENTRY(R13),
    ENTRY(R14),
    ENTRY(R15),
    ENTRY(CPSR),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}

const char* to_string(CorePrStatus::Registers::AARCH64 e) {
  #define ENTRY(X) std::pair(CorePrStatus::Registers::AARCH64::X, #X)
  STRING_MAP enum_strings {
    ENTRY(X0),
    ENTRY(X1),
    ENTRY(X2),
    ENTRY(X3),
    ENTRY(X4),
    ENTRY(X5),
    ENTRY(X6),
    ENTRY(X7),
    ENTRY(X8),
    ENTRY(X9),
    ENTRY(X10),
    ENTRY(X11),
    ENTRY(X12),
    ENTRY(X13),
    ENTRY(X14),
    ENTRY(X15),
    ENTRY(X15),
    ENTRY(X16),
    ENTRY(X17),
    ENTRY(X18),
    ENTRY(X19),
    ENTRY(X20),
    ENTRY(X21),
    ENTRY(X22),
    ENTRY(X23),
    ENTRY(X24),
    ENTRY(X25),
    ENTRY(X26),
    ENTRY(X27),
    ENTRY(X28),
    ENTRY(X29),
    ENTRY(X30),
    ENTRY(X31),
    ENTRY(PC),
    ENTRY(PSTATE),
  };
  #undef ENTRY

  if (auto it = enum_strings.find(e); it != enum_strings.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

} // namespace ELF
} // namespace LIEF
