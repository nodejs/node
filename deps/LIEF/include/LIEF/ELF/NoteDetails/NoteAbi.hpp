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
#ifndef LIEF_ELF_NOTE_ABI_H
#define LIEF_ELF_NOTE_ABI_H

#include <ostream>
#include <array>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/ELF/Note.hpp"

namespace LIEF {
namespace ELF {

/// Class that wraps the `NT_GNU_ABI_TAG` note
class LIEF_API NoteAbi : public Note {
  public:
  /// ABI recognized by this note
  enum class ABI {
    LINUX = 0,
    GNU,
    SOLARIS2,
    FREEBSD,
    NETBSD,
    SYLLABLE,
    NACL
  };
  /// Version type: (Major, Minor, Patch)
  using version_t = std::array<uint32_t, 3>;

  static constexpr size_t abi_offset      = 0;
  static constexpr size_t abi_size        = sizeof(uint32_t);

  static constexpr size_t version_offset  = abi_size;
  static constexpr size_t version_size    = 3 * sizeof(uint32_t);

  public:
  using Note::Note;

  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<Note>(new NoteAbi(*this));
  }

  /// Return the version or an error if it can't be parsed
  result<version_t> version() const;

  /// Return the ABI or an error if it can't be parsed
  result<ABI> abi() const;

  void version(const version_t& version);
  void version(ABI abi);

  void dump(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::GNU_ABI_TAG;
  }

  //// Size of the description content
  static constexpr uint8_t description_size() {
    return /* abi */ sizeof(uint32_t) + /* version */ 3 * sizeof(uint32_t);
  }

  ~NoteAbi() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const NoteAbi& note) {
    note.dump(os);
    return os;
  }
};

LIEF_API const char* to_string(NoteAbi::ABI abi);

} // namepsace ELF
} // namespace LIEF

#endif
