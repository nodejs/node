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
#include <algorithm>
#include <iterator>

#include "spdlog/fmt/fmt.h"
#include "logging.hpp"
#include "frozen.hpp"
#include "fmt_formatter.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/MachO/Section.hpp"
#include "LIEF/MachO/Relocation.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "MachO/Structures.hpp"

FMT_FORMATTER(LIEF::MachO::Section::FLAGS, LIEF::MachO::to_string);
FMT_FORMATTER(LIEF::MachO::Section::TYPE, LIEF::MachO::to_string);

namespace LIEF {
namespace MachO {

static constexpr auto ARRAY_FLAGS = {
  Section::FLAGS::PURE_INSTRUCTIONS,
  Section::FLAGS::NO_TOC,
  Section::FLAGS::STRIP_STATIC_SYMS,
  Section::FLAGS::NO_DEAD_STRIP,
  Section::FLAGS::LIVE_SUPPORT,
  Section::FLAGS::SELF_MODIFYING_CODE,
  Section::FLAGS::DEBUG_INFO,
  Section::FLAGS::SOME_INSTRUCTIONS,
  Section::FLAGS::EXT_RELOC,
  Section::FLAGS::LOC_RELOC,
};

Section::Section() = default;
Section::~Section() = default;

Section& Section::operator=(Section other) {
  swap(other);
  return *this;
}
Section::Section(std::string name) {
  this->name(std::move(name));
}

Section::Section(std::string name, content_t content) {
  this->name(std::move(name));
  this->content(std::move(content));
}

Section::Section(const Section& other) :
  LIEF::Section{other},
  segment_name_{other.segment_name_},
  original_size_{other.original_size_},
  align_{other.align_},
  relocations_offset_{other.relocations_offset_},
  nbof_relocations_{other.nbof_relocations_},
  flags_{other.flags_},
  reserved1_{other.reserved1_},
  reserved2_{other.reserved2_},
  reserved3_{other.reserved3_},
  content_{other.content_}
{}


Section::Section(const details::section_32& sec) :
  segment_name_{sec.segname, sizeof(sec.sectname)},
  original_size_{sec.size},
  align_{sec.align},
  relocations_offset_{sec.reloff},
  nbof_relocations_{sec.nreloc},
  flags_{sec.flags},
  reserved1_{sec.reserved1},
  reserved2_{sec.reserved2}
{
  name_            = {sec.sectname, sizeof(sec.sectname)};
  size_            = sec.size;
  offset_          = sec.offset;
  virtual_address_ = sec.addr;

  name_         = name_.c_str();
  segment_name_ = segment_name_.c_str();
}

Section::Section(const details::section_64& sec) :
  segment_name_{sec.segname, sizeof(sec.segname)},
  original_size_{sec.size},
  align_{sec.align},
  relocations_offset_{sec.reloff},
  nbof_relocations_{sec.nreloc},
  flags_{sec.flags},
  reserved1_{sec.reserved1},
  reserved2_{sec.reserved2},
  reserved3_{sec.reserved3}
{
  name_            = {sec.sectname, sizeof(sec.sectname)};
  size_            = sec.size;
  offset_          = sec.offset;
  virtual_address_ = sec.addr;

  name_         = name_.c_str();
  segment_name_ = segment_name_.c_str();
}


void Section::swap(Section& other) noexcept {
  std::swap(name_,            other.name_);
  std::swap(virtual_address_, other.virtual_address_);
  std::swap(size_,            other.size_);
  std::swap(offset_,          other.offset_);

  std::swap(segment_name_,        other.segment_name_);
  std::swap(original_size_,       other.original_size_);
  std::swap(align_,               other.align_);
  std::swap(relocations_offset_,  other.relocations_offset_);
  std::swap(nbof_relocations_,    other.nbof_relocations_);
  std::swap(flags_,               other.flags_);
  std::swap(reserved1_,           other.reserved1_);
  std::swap(reserved2_,           other.reserved2_);
  std::swap(reserved3_,           other.reserved3_);
  std::swap(content_,             other.content_);
  std::swap(segment_,             other.segment_);
  std::swap(relocations_,         other.relocations_);
}

span<const uint8_t> Section::content() const {
  if (segment_ == nullptr) {
    return content_;
  }

  if (size_ == 0 || offset_ == 0) { // bss section for instance
    return {};
  }

  if (int64_t(size_) < 0 || int64_t(offset_) < 0) {
    return {};
  }

  int64_t relative_offset = offset_ - segment_->file_offset();
  if (relative_offset < 0) {
    relative_offset = virtual_address_ - segment_->virtual_address();
  }
  span<const uint8_t> content = segment_->content();
  if (relative_offset > (int64_t)content.size() || (relative_offset + size_) > content.size()) {
    LIEF_ERR("Section's size is bigger than segment's size");
    return {};
  }
  return content.subspan(relative_offset, size_);
}

void Section::content(const content_t& data) {
  if (segment_ == nullptr) {
    content_ = data;
    return;
  }

  if (size_ == 0 || offset_ == 0) { // bss section for instance
    LIEF_ERR("Offset or size is null");
    return;
  }

  uint64_t relative_offset = offset_ - segment_->file_offset();

  span<uint8_t> content = segment_->writable_content();

  if (relative_offset > content.size() || (relative_offset + data.size()) > content.size()) {
    LIEF_ERR("New data are bigger than the original one");
    return;
  }

  std::move(std::begin(data), std::end(data),
            content.data() + relative_offset);
}

const std::string& Section::segment_name() const {
  if (segment_ == nullptr || segment_->name().empty()) {
    return segment_name_;
  }
  return segment_->name();
}


std::vector<Section::FLAGS> Section::flags_list() const {
  std::vector<FLAGS> flags;

  std::copy_if(
      std::begin(ARRAY_FLAGS), std::end(ARRAY_FLAGS),
      std::inserter(flags, std::begin(flags)),
      [this] (FLAGS f) { return has(f); });

  return flags;
}

void Section::segment_name(const std::string& name) {
  segment_name_ = name;
  if (segment_ != nullptr && !segment_->name().empty()) {
    segment_->name(name);
  }
}

bool Section::has(FLAGS flag) const {
  return (static_cast<uint32_t>(flag) & uint32_t(flags())) > 0;
}

void Section::add(FLAGS flag) {
  flags(raw_flags() | uint32_t(flag));
}

void Section::remove(FLAGS flag) {
  flags(raw_flags() & (~ uint32_t(flag)));
}

void Section::clear(uint8_t v) {
  content_t clear(size(), v);
  content(std::move(clear));
}

void Section::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::unique_ptr<SpanStream> Section::stream() const {
  return std::make_unique<SpanStream>(content());
}

std::ostream& operator<<(std::ostream& os, const Section& section) {
  const auto& flags = section.flags_list();
  os << fmt::format(
    "name={}, segment={}, address=0x{:06x}, size=0x{:04x} "
    "offset=0x{:06x}, align={}, type={}, reloc_offset={}, nb_reloc={} "
    "reserved1={}, reserved2={}, reserved3={}, flags={}",
    section.name(), section.segment_name(), section.address(),
    section.size(), section.offset(), section.alignment(), section.type(),
    section.relocation_offset(), section.numberof_relocations(),
    section.reserved1(), section.reserved2(), section.reserved3(),
    flags
  );
  return os;
}


const char* to_string(Section::FLAGS e) {
  #define ENTRY(X) std::pair(Section::FLAGS::X, #X)
  STRING_MAP enums2str {
    ENTRY(PURE_INSTRUCTIONS),
    ENTRY(NO_TOC),
    ENTRY(STRIP_STATIC_SYMS),
    ENTRY(NO_DEAD_STRIP),
    ENTRY(LIVE_SUPPORT),
    ENTRY(SELF_MODIFYING_CODE),
    ENTRY(DEBUG_INFO),
    ENTRY(SOME_INSTRUCTIONS),
    ENTRY(EXT_RELOC),
    ENTRY(LOC_RELOC),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(Section::TYPE e) {
  #define ENTRY(X) std::pair(Section::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(REGULAR),
    ENTRY(ZEROFILL),
    ENTRY(CSTRING_LITERALS),
    ENTRY(IS_4BYTE_LITERALS),
    ENTRY(IS_8BYTE_LITERALS),
    ENTRY(LITERAL_POINTERS),
    ENTRY(NON_LAZY_SYMBOL_POINTERS),
    ENTRY(LAZY_SYMBOL_POINTERS),
    ENTRY(SYMBOL_STUBS),
    ENTRY(MOD_INIT_FUNC_POINTERS),
    ENTRY(MOD_TERM_FUNC_POINTERS),
    ENTRY(COALESCED),
    ENTRY(GB_ZEROFILL),
    ENTRY(INTERPOSING),
    ENTRY(IS_16BYTE_LITERALS),
    ENTRY(DTRACE_DOF),
    ENTRY(LAZY_DYLIB_SYMBOL_POINTERS),
    ENTRY(THREAD_LOCAL_REGULAR),
    ENTRY(THREAD_LOCAL_ZEROFILL),
    ENTRY(THREAD_LOCAL_VARIABLES),
    ENTRY(THREAD_LOCAL_VARIABLE_POINTERS),
    ENTRY(THREAD_LOCAL_INIT_FUNCTION_POINTERS),
    ENTRY(INIT_FUNC_OFFSETS),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

} // namespace MachO
} // namespace LIEF
