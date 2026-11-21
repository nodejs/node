/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#include "LIEF/MachO/ThreadLocalVariables.hpp"
#include "MachO/Structures.hpp"

FMT_FORMATTER(LIEF::MachO::Section::FLAGS, LIEF::MachO::to_string);
FMT_FORMATTER(LIEF::MachO::Section::TYPE, LIEF::MachO::to_string);


namespace LIEF::MachO {

static constexpr auto ARRAY_FLAGS = {
    Section::FLAGS::PURE_INSTRUCTIONS, Section::FLAGS::NO_TOC,
    Section::FLAGS::STRIP_STATIC_SYMS, Section::FLAGS::NO_DEAD_STRIP,
    Section::FLAGS::LIVE_SUPPORT,      Section::FLAGS::SELF_MODIFYING_CODE,
    Section::FLAGS::DEBUG_INFO,        Section::FLAGS::SOME_INSTRUCTIONS,
    Section::FLAGS::EXT_RELOC,         Section::FLAGS::LOC_RELOC,
};

template<class T>
static std::unique_ptr<Section> create_from_raw(const T& raw) {
  auto type = (Section::TYPE)(raw.flags & Section::TYPE_MASK);
  switch (type) {
    case Section::TYPE::THREAD_LOCAL_VARIABLES:
      return std::make_unique<ThreadLocalVariables>(raw);
    default: return std::make_unique<Section>(raw);
  }
}

Section::Section() = default;
Section::~Section() = default;

Section::Section(Section&&) = default;
Section& Section::operator=(Section&&) = default;

Section& Section::operator=(const Section& copy) {
  if (this == &copy) {
    return *this;
  }
  LIEF::Section::operator=(copy);

  segment_name_ = copy.segment_name_;
  original_size_ = copy.original_size_;
  align_ = copy.align_;
  relocations_offset_ = copy.relocations_offset_;
  nbof_relocations_ = copy.nbof_relocations_;
  flags_ = copy.flags_;
  reserved1_ = copy.reserved1_;
  reserved2_ = copy.reserved2_;
  reserved3_ = copy.reserved3_;
  content_ = copy.content_;

  return *this;
}

Section::Section(const Section& copy) :
  LIEF::Section{copy},
  segment_name_(copy.segment_name_),
  original_size_(copy.original_size_),
  align_(copy.align_),
  relocations_offset_(copy.relocations_offset_),
  nbof_relocations_(copy.nbof_relocations_),
  flags_(copy.flags_),
  reserved1_(copy.reserved1_),
  reserved2_(copy.reserved2_),
  reserved3_(copy.reserved3_),
  content_(copy.content_) {}

Section::Section(const details::section_32& sec) :
  segment_name_{sec.segname, sizeof(sec.sectname)},
  original_size_{sec.size},
  align_{sec.align},
  relocations_offset_{sec.reloff},
  nbof_relocations_{sec.nreloc},
  flags_{sec.flags},
  reserved1_{sec.reserved1},
  reserved2_{sec.reserved2} {
  name_ = {sec.sectname, sizeof(sec.sectname)};
  size_ = sec.size;
  offset_ = sec.offset;
  virtual_address_ = sec.addr;

  name_ = name_.c_str();
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
  reserved3_{sec.reserved3} {
  name_ = {sec.sectname, sizeof(sec.sectname)};
  size_ = sec.size;
  offset_ = sec.offset;
  virtual_address_ = sec.addr;

  name_ = name_.c_str();
  segment_name_ = segment_name_.c_str();
}

std::unique_ptr<Section> Section::create(const details::section_32& sec) {
  return create_from_raw(sec);
}

std::unique_ptr<Section> Section::create(const details::section_64& sec) {
  return create_from_raw(sec);
}

std::unique_ptr<Section> Section::create(std::string name,
                                         const content_t& content, TYPE type) {
  std::unique_ptr<Section> sec;
  switch (type) {
    case TYPE::THREAD_LOCAL_VARIABLES:
      sec = std::make_unique<ThreadLocalVariables>();
      break;
    default: sec = std::unique_ptr<Section>(new Section{});
  }

  assert(sec != nullptr);

  sec->name(std::move(name));

  if (!content.empty()) {
    sec->content(content);
  }
  return sec;
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

  if (relative_offset < 0) {
    return {};
  }
  span<const uint8_t> content = segment_->content();
  if (relative_offset > (int64_t)content.size() ||
      (relative_offset + size_) > content.size())
  {
    LIEF_ERR("Section size exceeds segment size");
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
    LIEF_ERR("Offset or size is zero");
    return;
  }

  uint64_t relative_offset = offset_ - segment_->file_offset();

  span<uint8_t> content = segment_->content();

  if (relative_offset > content.size() ||
      (relative_offset + data.size()) > content.size())
  {
    LIEF_ERR("New data exceeds original size");
    return;
  }

  std::move(data.begin(), data.end(), content.data() + relative_offset);
}

const std::string& Section::segment_name() const {
  if (segment_ == nullptr || segment_->name().empty()) {
    return segment_name_;
  }
  return segment_->name();
}


std::vector<Section::FLAGS> Section::flags_list() const {
  std::vector<FLAGS> flags;

  std::copy_if(ARRAY_FLAGS.begin(), ARRAY_FLAGS.end(),
               std::inserter(flags, flags.begin()),
               [this](FLAGS f) { return has(f); });

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
  flags(raw_flags() & (~uint32_t(flag)));
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
      "name={}, segment={}, address={:#08x}, size={:#06x} "
      "offset={:#08x}, align={}, type={}, reloc_offset={}, nb_reloc={} "
      "reserved1={}, reserved2={}, reserved3={}, flags={}",
      section.name(), section.segment_name(), section.address(), section.size(),
      section.offset(), section.alignment(), section.type(),
      section.relocation_offset(), section.numberof_relocations(),
      section.reserved1(), section.reserved2(), section.reserved3(), flags
  );
  return os;
}


const char* to_string(Section::FLAGS e) {
#define ENTRY(X) std::pair(Section::FLAGS::X, #X)
  STRING_MAP enums2str{
      ENTRY(PURE_INSTRUCTIONS), ENTRY(NO_TOC),
      ENTRY(STRIP_STATIC_SYMS), ENTRY(NO_DEAD_STRIP),
      ENTRY(LIVE_SUPPORT),      ENTRY(SELF_MODIFYING_CODE),
      ENTRY(DEBUG_INFO),        ENTRY(SOME_INSTRUCTIONS),
      ENTRY(EXT_RELOC),         ENTRY(LOC_RELOC),
  };
#undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(Section::TYPE e) {
#define ENTRY(X) std::pair(Section::TYPE::X, #X)
  STRING_MAP enums2str{
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

} // namespace LIEF::MachO
