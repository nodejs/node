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
#ifndef LIEF_ELF_CORE_PRSTATUS_H
#define LIEF_ELF_CORE_PRSTATUS_H

#include <vector>
#include <ostream>
#include <utility>

#include "LIEF/visibility.h"
#include "LIEF/ELF/enums.hpp"
#include "LIEF/ELF/Note.hpp"

namespace LIEF {
namespace ELF {

class Parser;
class Builder;
class Binary;

/// Class representing core PrPsInfo object
class LIEF_API CorePrStatus : public Note {
  public:
  struct siginfo_t {
    int32_t signo = 0;
    int32_t code = 0;
    int32_t err = 0;
  };

  struct timeval_t {
    uint64_t sec = 0;
    uint64_t usec = 0;
  };

  struct pr_status_t {
    siginfo_t info;

    uint16_t cursig = 0;
    uint16_t reserved = 0;

    uint64_t sigpend = 0;
    uint64_t sighold = 0;

    int32_t  pid = 0;
    int32_t  ppid = 0;
    int32_t  pgrp = 0;
    int32_t  sid = 0;

    timeval_t utime;
    timeval_t stime;
    timeval_t cutime;
    timeval_t cstime;
  };

  struct Registers {
    /// Register for the x86 architecture (ARCH::I386).
    enum class X86 {
      EBX = 0, ECX, EDX, ESI, EDI, EBP, EAX,
      DS, ES, FS, GS, ORIG_EAX, EIP, CS, EFLAGS, ESP, SS,
      _COUNT
    };

    /// Register for the x86-64 architecture (ARCH::X86_64).
    enum class X86_64 {
      R15 = 0, R14, R13, R12, RBP, RBX, R11, R10,
      R9, R8, RAX, RCX, RDX, RSI, RDI, ORIG_RAX,
      RIP, CS, EFLAGS, RSP, SS,
      _COUNT
    };

    /// Register for the ARM architecture (ARCH::ARM).
    enum class ARM {
      R0 = 0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15,
      CPSR,
      _COUNT
    };

    /// Register for the AARCH64 architecture (ARCH::AARCH64).
    enum class AARCH64 {
      X0 = 0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15,
      X16, X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30,
      X31, PC, PSTATE,
      _COUNT
    };
  };

  public:
  CorePrStatus(ARCH arch, Header::CLASS cls, std::string name,
               uint32_t type, description_t description) :
    Note(std::move(name), TYPE::CORE_PRSTATUS, type, std::move(description), ""),
    arch_(arch), class_(cls)
  {}

  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<CorePrStatus>(new CorePrStatus(*this));
  }

  /// Return the pr_status_t structure
  pr_status_t status() const;
  void status(const pr_status_t& status);

  ARCH architecture() const {
    return arch_;
  }

  /// The program counter or an error if not found
  result<uint64_t> pc() const;

  /// The stack pointer or an error if not found
  result<uint64_t> sp() const;

  /// The value of the register that holds the return value according to
  /// the calling convention.
  result<uint64_t> return_value() const;

  /// Get the value for the given X86 register or return an error
  result<uint64_t> get(Registers::X86 reg) const;
  /// Get the value for the given X86_64 register or return an error
  result<uint64_t> get(Registers::X86_64 reg) const;
  /// Get the value for the given ARM register or return an error
  result<uint64_t> get(Registers::ARM reg) const;
  /// Get the value for the given AARCH64 register or return an error
  result<uint64_t> get(Registers::AARCH64 reg) const;

  ok_error_t set(Registers::X86 reg, uint64_t value);
  ok_error_t set(Registers::X86_64 reg, uint64_t value);
  ok_error_t set(Registers::ARM reg, uint64_t value);
  ok_error_t set(Registers::AARCH64 reg, uint64_t value);

  /// A list of the register values.
  /// This list is **guarantee** to be as long as the Registers::ARM::_COUNT or
  /// empty if it can't be resolved. Thus, one can access a specific register
  /// with:
  /// ```cpp
  /// if (architecture() == ARCH::AARCH64) {
  ///   auto reg_vals = register_values()
  ///   if (!reg_vals.empty()) {
  ///     auto x20 = reg_vals[static_cast<size_t>(Register::AARCH64::X20)]
  ///   }
  /// }
  /// ```
  std::vector<uint64_t> register_values() const;

  result<uint64_t> operator[](Registers::X86 reg) const {
    return get(reg);
  }

  result<uint64_t> operator[](Registers::X86_64 reg) const {
    return get(reg);
  }

  result<uint64_t> operator[](Registers::ARM reg) const {
    return get(reg);
  }

  result<uint64_t> operator[](Registers::AARCH64 reg) const {
    return get(reg);
  }

  void dump(std::ostream& os) const override;
  void accept(Visitor& visitor) const override;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::CORE_PRSTATUS;
  }

  ~CorePrStatus() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const CorePrStatus& note) {
    note.dump(os);
    return os;
  }

  private:
  ARCH arch_ = ARCH::NONE;
  Header::CLASS class_ = Header::CLASS::NONE;
};

LIEF_API const char* to_string(CorePrStatus::Registers::X86 e);
LIEF_API const char* to_string(CorePrStatus::Registers::X86_64 e);
LIEF_API const char* to_string(CorePrStatus::Registers::ARM e);
LIEF_API const char* to_string(CorePrStatus::Registers::AARCH64 e);

} // namepsace ELF
} // namespace LIEF

#endif
