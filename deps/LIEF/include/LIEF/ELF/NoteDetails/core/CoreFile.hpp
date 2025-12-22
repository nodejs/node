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
#ifndef LIEF_ELF_CORE_FILE_H
#define LIEF_ELF_CORE_FILE_H

#include <vector>
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/ELF/Note.hpp"

namespace LIEF {
namespace ELF {

/// Class representing a core `NT_FILE` which describes the mapped files
/// of the process
class LIEF_API CoreFile : public Note {
  public:
  /// Core file entry
  struct entry_t {
    uint64_t start = 0;    /// Start address of mapped file
    uint64_t end = 0;      ///< End address of mapped file
    uint64_t file_ofs = 0; ///< Offset (in core) of mapped file
    std::string path;      ///< Path of mapped file

    LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const entry_t& entry);
  };

  using files_t        = std::vector<entry_t>;
  using iterator       = files_t::iterator;
  using const_iterator = files_t::const_iterator;

  public:
  CoreFile(ARCH arch, Header::CLASS cls, std::string name,
           uint32_t type, Note::description_t description);

  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<Note>(new CoreFile(*this));
  }

  /// Number of coredump file entries
  uint64_t count() const {
    return files_.size();
  }

  /// Coredump file entries
  const files_t& files() const {
    return files_;
  }

  iterator begin() {
    return files_.begin();
  }

  iterator end() {
    return files_.end();
  }

  const_iterator begin() const {
    return files_.begin();
  }

  const_iterator end() const {
    return files_.end();
  }

  void files(const files_t& file);

  void dump(std::ostream& os) const override;
  void accept(Visitor& visitor) const override;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::CORE_FILE;
  }

  ~CoreFile() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const CoreFile& note) {
    note.dump(os);
    return os;
  }

  protected:
  template<class T>
  LIEF_LOCAL void read_files();

  template<class T>
  LIEF_LOCAL void write_files();

  files_t  files_;
  uint64_t page_size_ = 0;
  ARCH arch_ = ARCH::NONE;
  Header::CLASS class_ = Header::CLASS::NONE;
};

} // namepsace ELF
} // namespace LIEF

#endif
