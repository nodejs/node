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
#ifndef LIEF_MACHO_SEGMENT_COMMAND_H
#define LIEF_MACHO_SEGMENT_COMMAND_H

#include <string>
#include <vector>
#include <ostream>
#include <memory>

#include "LIEF/enums.hpp"
#include "LIEF/span.hpp"
#include "LIEF/visibility.h"

#include "LIEF/iterators.hpp"
#include "LIEF/MachO/LoadCommand.hpp"


namespace LIEF {
class SpanStream;
namespace MachO {

class Binary;
class BinaryParser;
class Builder;
class DyldChainedFixupsCreator;
class DyldInfo;
class Relocation;
class Section;

namespace details {
struct segment_command_32;
struct segment_command_64;
}

/// Class which represents a LoadCommand::TYPE::SEGMENT / LoadCommand::TYPE::SEGMENT_64 command
class LIEF_API SegmentCommand : public LoadCommand {

  friend class DyldChainedFixupsCreator;
  friend class BinaryParser;
  friend class Binary;
  friend class Section;
  friend class Builder;

  public:
  using content_t = std::vector<uint8_t>;

  /// Internal container for storing Mach-O Section
  using sections_t = std::vector<std::unique_ptr<Section>>;

  /// Iterator which outputs Section&
  using it_sections = ref_iterator<sections_t&, Section*>;

  /// Iterator which outputs const Section&
  using it_const_sections = const_ref_iterator<const sections_t&, const Section*>;

  /// Internal container for storing Mach-O Relocation
  using relocations_t = std::vector<std::unique_ptr<Relocation>>;

  /// Iterator which outputs Relocation&
  using it_relocations = ref_iterator<relocations_t&, Relocation*>;

  /// Iterator which outputs const Relocation&
  using it_const_relocations = const_ref_iterator<const relocations_t&, const Relocation*>;

  enum class FLAGS: uint64_t  {
    HIGHVM              = 0x1u, ///< The file contents for this segment are for the high part of the virtual memory space; the low part is zero filled (for stacks in core files).
    FVMLIB              = 0x2u, ///< this segment is the VM that is allocated by a fixed VM library, for overlap checking in the link editor.
    NORELOC             = 0x4u, ///< This segment has nothing that was relocated in it and nothing relocated to it. It may be safely replaced without relocation.
    PROTECTED_VERSION_1 = 0x8u,
    READ_ONLY           = 0x10u,
  };

  /// Values for segment_command.initprot.
  /// From <mach/vm_prot.h>
  enum class VM_PROTECTIONS  {
    READ    = 0x1, ///< Reading data within the segment is allowed
    WRITE   = 0x2, ///< Writing data within the segment is allowed
    EXECUTE = 0x4, ///< Executing data within the segment is allowed
  };

  public:
  SegmentCommand();
  SegmentCommand(const details::segment_command_32& cmd);
  SegmentCommand(const details::segment_command_64& cmd);

  SegmentCommand& operator=(SegmentCommand other);
  SegmentCommand(const SegmentCommand& copy);

  SegmentCommand(std::string name, content_t content);

  SegmentCommand(std::string name);

  void swap(SegmentCommand& other) noexcept;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<SegmentCommand>(new SegmentCommand(*this));
  }

  ~SegmentCommand() override;

  /// Name of the segment (e.g. ``__TEXT``)
  const std::string& name() const {
    return name_;
  }

  /// Absolute virtual base address of the segment
  uint64_t virtual_address() const {
    return virtual_address_;
  }

  /// Virtual size of the segment
  uint64_t virtual_size() const {
    return virtual_size_;
  }

  /// Size of this segment in the binary file
  uint64_t file_size() const {
    return file_size_;
  }

  /// Offset of the data of this segment in the file
  uint64_t file_offset() const {
    return file_offset_;
  }

  /// The maximum of protections for this segment (cf. VM_PROTECTIONS)
  uint32_t max_protection() const {
    return max_protection_;
  }

  /// The initial protections of this segment (cf. VM_PROTECTIONS)
  uint32_t init_protection() const {
    return init_protection_;
  }

  /// The number of sections associated with this segment
  uint32_t numberof_sections() const {
    return nb_sections_;
  }

  /// Flags associated with this segment (cf. SegmentCommand::FLAGS)
  uint32_t flags() const {
    return flags_;
  }

  /// Return an iterator over the MachO::Section linked to this segment
  it_sections sections() {
    return sections_;
  }

  it_const_sections sections() const {
    return sections_;
  }

  /// Return an iterator over the MachO::Relocation linked to this segment
  ///
  /// For Mach-O executable or library this iterator should be empty as
  /// the relocations are managed by the Dyld::rebase_opcodes.
  /// On the other hand, for object files (``.o``) this iterator should not be empty
  it_relocations relocations() {
    return relocations_;
  }
  it_const_relocations relocations() const {
    return relocations_;
  }

  /// Get the section with the given name
  const Section* get_section(const std::string& name) const;
  Section* get_section(const std::string& name);

  /// The raw content of this segment
  span<const uint8_t> content() const {
    return data_;
  }

  /// Return a stream over the content of this segment
  std::unique_ptr<SpanStream> stream() const;

  /// The original index of this segment or -1 if not defined
  int8_t index() const {
    return this->index_;
  }

  void name(std::string name) {
    name_ = std::move(name);
  }

  void virtual_address(uint64_t virtual_address) {
    virtual_address_ = virtual_address;
  }
  void virtual_size(uint64_t virtual_size) {
    virtual_size_ = virtual_size;
  }
  void file_offset(uint64_t file_offset) {
    file_offset_ = file_offset;
  }
  void file_size(uint64_t file_size) {
    file_size_ = file_size;
  }
  void max_protection(uint32_t max_protection) {
    max_protection_ = max_protection;
  }
  void init_protection(uint32_t init_protection) {
    init_protection_ = init_protection;
  }
  void numberof_sections(uint32_t nb_section) {
    nb_sections_ = nb_section;
  }
  void flags(uint32_t flags) {
    flags_ = flags;
  }

  void content(content_t data);

  /// Add a new section in this segment
  Section& add_section(const Section& section);

  /// Remove all the sections linked to this segment
  void remove_all_sections();

  /// Check if the current segment embeds the given section
  bool has(const Section& section) const;

  /// Check if the current segment embeds the given section name
  bool has_section(const std::string& section_name) const;

  bool is(VM_PROTECTIONS prot) const {
    return (init_protection() & (uint32_t)prot) > 0 ||
           (max_protection() & (uint32_t)prot) > 0;
  }

  std::ostream& print(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const LoadCommand* cmd) {
    const LoadCommand::TYPE type = cmd->command();
    return type == LoadCommand::TYPE::SEGMENT ||
           type == LoadCommand::TYPE::SEGMENT_64;
  }

  protected:
  span<uint8_t> writable_content() {
    return data_;
  }

  LIEF_LOCAL void content_resize(size_t size);
  LIEF_LOCAL void content_insert(size_t where, size_t size);

  void content_extend(size_t width) {
    content_resize(data_.size() + width);
  }

  using update_fnc_t    = std::function<void(std::vector<uint8_t>&)>;
  using update_fnc_ws_t = std::function<void(std::vector<uint8_t>&, size_t, size_t)>;

  LIEF_LOCAL virtual void update_data(const update_fnc_t& f);
  LIEF_LOCAL virtual void update_data(const update_fnc_ws_t& f,
                                      size_t where, size_t size);

  std::string name_;
  uint64_t virtual_address_ = 0;
  uint64_t virtual_size_ = 0;
  uint64_t file_offset_ = 0;
  uint64_t file_size_ = 0;
  uint32_t max_protection_ = 0;
  uint32_t init_protection_ = 0;
  uint32_t nb_sections_ = 0;
  uint32_t flags_ = 0;
  int8_t  index_ = -1;
  content_t data_;
  sections_t sections_;
  relocations_t relocations_;
};

LIEF_API const char* to_string(SegmentCommand::FLAGS flag);
LIEF_API const char* to_string(SegmentCommand::VM_PROTECTIONS protection);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::MachO::SegmentCommand::FLAGS);
ENABLE_BITMASK_OPERATORS(LIEF::MachO::SegmentCommand::VM_PROTECTIONS);

#endif
