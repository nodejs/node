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
#ifndef LIEF_ELF_CORE_PRPSINFO_H
#define LIEF_ELF_CORE_PRPSINFO_H

#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/ELF/enums.hpp"
#include "LIEF/ELF/Note.hpp"

namespace LIEF {
namespace ELF {

/// Class representing the NT_PRPSINFO core note.
/// This kind of note represents general information about the process
class LIEF_API CorePrPsInfo : public Note {
  public:
  struct info_t {
    uint8_t state = 0;    /// Numeric process state
    char sname = ' ';     /// printable character representing state
    bool zombie = false;  /// Whether the process is a zombie
    uint8_t nice = 0;     /// Nice value
    uint64_t flag = 0;    /// Process flag
    uint32_t uid = 0;     /// Process user ID
    uint32_t gid = 0;     /// Process group ID
    uint32_t pid = 0;     /// Process ID
    uint32_t ppid = 0;    /// Process parent ID
    uint32_t pgrp = 0;    /// Process group
    uint32_t sid = 0;     /// Process session id
    std::string filename; /// Filename of the executable
    std::string args;     /// Initial part of the arguments

    /// Return the filename without the ending `\x00`
    std::string filename_stripped() const {
      return filename.c_str();
    }

    /// Return the args without the ending `\x00`
    std::string args_stripped() const {
      return args.c_str();
    }
  };
  CorePrPsInfo(ARCH arch, Header::CLASS cls, std::string name,
               uint32_t type, description_t description) :
    Note(std::move(name), TYPE::CORE_PRPSINFO, type, std::move(description), ""),
    arch_(arch), class_(cls)
  {}

  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<Note>(new CorePrPsInfo(*this));
  }

  /// Return a `elf_prpsinfo`-like structure or an error if it can't be parsed.
  result<info_t> info() const;
  void info(const info_t& info);

  void dump(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::CORE_PRPSINFO;
  }

  ~CorePrPsInfo() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const CorePrPsInfo& note) {
    note.dump(os);
    return os;
  }
  private:
  [[maybe_unused]] ARCH arch_ = ARCH::NONE;
  Header::CLASS class_ = Header::CLASS::NONE;
};

} // namepsace ELF
} // namespace LIEF

#endif
