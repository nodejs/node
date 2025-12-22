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
#include "LIEF/ELF/hash.hpp"
#include "LIEF/ELF/NoteDetails/core/CoreFile.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "ELF/Structures.hpp"
#include "LIEF/iostream.hpp"

namespace LIEF {
namespace ELF {

template<class ELF_T>
void CoreFile::read_files() {
  static constexpr auto MAX_ENTRIES = 6000;
  using Elf_Addr      = typename ELF_T::Elf_Addr;
  using Elf_FileEntry = typename ELF_T::Elf_FileEntry;

  auto stream = SpanStream::from_vector(description_);
  if (!stream) {
    return;
  }

  auto count = stream->read<Elf_Addr>();
  if (!count) {
    return;
  }

  if (*count > MAX_ENTRIES) {
    LIEF_ERR("Too many entries ({} while limited at {})", *count, MAX_ENTRIES);
    return;
  }

  auto page_size = stream->read<Elf_Addr>();
  if (!page_size) {
    return;
  }

  page_size_ = *page_size;
  files_.resize(*count);
  for (size_t i = 0; i < files_.size(); ++i) {
    auto entry = stream->read<Elf_FileEntry>();
    if (!entry) {
      break;
    }
    files_[i] = {entry->start, entry->end, entry->file_ofs, ""};
  }

  for (size_t i = 0; i < files_.size(); ++i) {
    auto path = stream->read_string();
    if (!path) {
      break;
    }
    files_[i].path = std::move(*path);
  }
}

template<class ELF_T>
void CoreFile::write_files() {
  using Elf_Addr      = typename ELF_T::Elf_Addr;
  using Elf_FileEntry = typename ELF_T::Elf_FileEntry;

  vector_iostream ios;
  ios.write(static_cast<Elf_Addr>(files_.size()));
  ios.write(static_cast<Elf_Addr>(page_size_));
  for (const entry_t& entry : files_) {
    Elf_FileEntry raw_entry;
    std::memset(&raw_entry, 0, sizeof(Elf_FileEntry));

    raw_entry.start    = static_cast<Elf_Addr>(entry.start);
    raw_entry.end      = static_cast<Elf_Addr>(entry.end);
    raw_entry.file_ofs = static_cast<Elf_Addr>(entry.file_ofs);
    ios.write(raw_entry);
  }

  for (const entry_t& entry : files_) {
    ios.write(entry.path);
  }
  ios.move(description_);
}

CoreFile::CoreFile(ARCH arch, Header::CLASS cls, std::string name,
                   uint32_t type, Note::description_t description) :
  Note(std::move(name), Note::TYPE::CORE_FILE, type, std::move(description), ""),
  arch_(arch), class_(cls)
{

  class_ == Header::CLASS::ELF32 ? read_files<details::ELF32>() :
                                   read_files<details::ELF64>();
}


void CoreFile::files(const CoreFile::files_t& files) {
  files_ = files;
  class_ == Header::CLASS::ELF32 ? write_files<details::ELF32>() :
                                   write_files<details::ELF64>();

}

void CoreFile::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void CoreFile::dump(std::ostream& os) const {
  Note::dump(os);
  const files_t& files = this->files();
  if (files.empty()) {
    return;
  }
  os << '\n';
  for (const entry_t& entry : files) {
    os << "  " << entry << '\n';
  }
}

std::ostream& operator<<(std::ostream& os, const CoreFile::entry_t& entry) {
  os << fmt::format("{}: [0x{:04x}, 0x{:04x}]@0x{:x}",
                    entry.path, entry.start, entry.end, entry.file_ofs);
  return os;
}

} // namespace ELF
} // namespace LIEF
