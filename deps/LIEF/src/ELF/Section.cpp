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

#include "logging.hpp"
#include "frozen.hpp"
#include "fmt_formatter.hpp"

#include "LIEF/Visitor.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/ELF/Section.hpp"
#include "LIEF/ELF/Segment.hpp"

#include "LIEF/ELF/EnumToString.hpp"

#include "ELF/DataHandler/Handler.hpp"
#include "ELF/Structures.hpp"

FMT_FORMATTER(LIEF::ELF::Section::FLAGS, LIEF::ELF::to_string);
FMT_FORMATTER(LIEF::ELF::Section::TYPE, LIEF::ELF::to_string);

namespace LIEF {
namespace ELF {

static constexpr uint64_t SHF_MASKPROC = 0xf0000000;
static constexpr uint64_t SHT_LOPROC = 0x70000000;
static constexpr uint64_t SHT_HIPROC = 0x7fffffff;

static constexpr auto ARRAY_FLAGS = {
  Section::FLAGS::NONE, Section::FLAGS::WRITE,
  Section::FLAGS::ALLOC, Section::FLAGS::EXECINSTR,
  Section::FLAGS::MERGE, Section::FLAGS::STRINGS,
  Section::FLAGS::INFO_LINK, Section::FLAGS::LINK_ORDER,
  Section::FLAGS::OS_NONCONFORMING, Section::FLAGS::GROUP,
  Section::FLAGS::TLS, Section::FLAGS::COMPRESSED,
  Section::FLAGS::GNU_RETAIN, Section::FLAGS::EXCLUDE,
  Section::FLAGS::XCORE_SHF_DP_SECTION, Section::FLAGS::XCORE_SHF_CP_SECTION,
  Section::FLAGS::X86_64_LARGE, Section::FLAGS::HEX_GPREL,
  Section::FLAGS::MIPS_NODUPES, Section::FLAGS::MIPS_NAMES,
  Section::FLAGS::MIPS_LOCAL, Section::FLAGS::MIPS_NOSTRIP,
  Section::FLAGS::MIPS_GPREL, Section::FLAGS::MIPS_MERGE,
  Section::FLAGS::MIPS_ADDR, Section::FLAGS::MIPS_STRING,
  Section::FLAGS::ARM_PURECODE,
};

Section::TYPE Section::type_from(uint32_t value, ARCH arch) {
  if (SHT_LOPROC <= value && value <= SHT_HIPROC) {
    switch (arch) {
      case ARCH::ARM:
        return TYPE(value + (uint64_t(TYPE::_ARM_ID_) << uint8_t(TYPE::_ID_SHIFT_)));

      case ARCH::HEXAGON:
        return TYPE(value + (uint64_t(TYPE::_HEX_ID_) << uint8_t(TYPE::_ID_SHIFT_)));

      case ARCH::X86_64:
        return TYPE(value + (uint64_t(TYPE::_X86_64_ID_) << uint8_t(TYPE::_ID_SHIFT_)));

      case ARCH::MIPS:
        return TYPE(value + (uint64_t(TYPE::_MIPS_ID_) << uint8_t(TYPE::_ID_SHIFT_)));

      case ARCH::RISCV:
        return TYPE(value + (uint64_t(TYPE::_RISCV_ID_) << uint8_t(TYPE::_ID_SHIFT_)));

      default:
        {
          LIEF_ERR("Arch-specific section: 0x{:08x} is not recognized for {}",
                   value, to_string(arch));
          return TYPE::SHT_NULL_;
        }
    }
  }
  return TYPE(value);
}

template<class T>
Section::Section(const T& header, ARCH arch) :
  arch_{arch},
  type_{type_from(header.sh_type, arch)},
  flags_{header.sh_flags},
  original_size_{header.sh_size},
  link_{header.sh_link},
  info_{header.sh_info},
  address_align_{header.sh_addralign},
  entry_size_{header.sh_entsize}
{
  virtual_address_ = header.sh_addr;
  offset_          = header.sh_offset;
  size_            = header.sh_size;
}

template Section::Section(const details::Elf32_Shdr& header, ARCH);
template Section::Section(const details::Elf64_Shdr& header, ARCH);

Section::Section(const Section& other) :
  LIEF::Section{other},
  arch_{other.arch_},
  type_{other.type_},
  flags_{other.flags_},
  original_size_{other.original_size_},
  link_{other.link_},
  info_{other.info_},
  address_align_{other.address_align_},
  entry_size_{other.entry_size_},
  is_frame_{other.is_frame_},
  content_c_{other.content_c_}
{}

void Section::swap(Section& other) noexcept {
  std::swap(name_,            other.name_);
  std::swap(virtual_address_, other.virtual_address_);
  std::swap(offset_,          other.offset_);
  std::swap(size_,            other.size_);

  std::swap(arch_,           other.arch_);
  std::swap(type_,           other.type_);
  std::swap(flags_,          other.flags_);
  std::swap(original_size_,  other.original_size_);
  std::swap(link_,           other.link_);
  std::swap(info_,           other.info_);
  std::swap(address_align_,  other.address_align_);
  std::swap(entry_size_,     other.entry_size_);
  std::swap(segments_,       other.segments_);
  std::swap(is_frame_,       other.is_frame_);
  std::swap(datahandler_,    other.datahandler_);
  std::swap(content_c_,      other.content_c_);
}

bool Section::has(const Segment& segment) const {
  auto it_segment = std::find_if(std::begin(segments_), std::end(segments_),
      [&segment] (Segment* s) {
        return *s == segment;
      });
  return it_segment != std::end(segments_);
}


void Section::size(uint64_t size) {
  if (datahandler_ != nullptr && !is_frame()) {
    if (auto node = datahandler_->get(file_offset(), this->size(), DataHandler::Node::SECTION)) {
      node->get().size(size);
    } else {
      if (type() != TYPE::NOBITS) {
        LIEF_ERR("Node not found. Can't resize the section {}", name());
        std::abort();
      }
    }
  }
  size_ = size;
}


void Section::offset(uint64_t offset) {
  if (datahandler_ != nullptr && !is_frame()) {
    if (auto node = datahandler_->get(file_offset(), size(), DataHandler::Node::SECTION)) {
      node->get().offset(offset);
    } else {
      if (type() != TYPE::NOBITS) {
        LIEF_WARN("Node not found. Can't change the offset of the section {}", name());
      }
    }
  }
  offset_ = offset;
}

span<const uint8_t> Section::content() const {
  if (size() == 0 || is_frame()) {
    return {};
  }

  if (datahandler_ == nullptr) {
    return content_c_;
  }

  if (size() > MAX_SECTION_SIZE) {
    return {};
  }

  auto res = datahandler_->get(offset(), size(), DataHandler::Node::SECTION);
  if (!res) {
    if (type() != TYPE::NOBITS) {
      LIEF_WARN("Section '{}' does not have content", name());
    }
    return {};
  }
  const std::vector<uint8_t>& binary_content = datahandler_->content();
  DataHandler::Node& node = res.value();
  auto end_offset = (int64_t)node.offset() + (int64_t)node.size();
  if (end_offset <= 0 || end_offset > (int64_t)binary_content.size()) {
    return {};
  }

  const uint8_t* ptr = binary_content.data() + node.offset();
  return {ptr, ptr + node.size()};
}

std::vector<Section::FLAGS> Section::flags_list() const {
  std::vector<FLAGS> flags;
  std::copy_if(std::begin(ARRAY_FLAGS), std::end(ARRAY_FLAGS),
               std::back_inserter(flags),
               [this] (FLAGS f) { return has(f); });
  return flags;
}

void Section::content(const std::vector<uint8_t>& data) {
  if (is_frame()) {
    return;
  }

  if (!data.empty() && type() == TYPE::NOBITS) {
    LIEF_INFO("You inserted 0x{:x} bytes in section '{}' which has SHT_NOBITS type",
              data.size(), name());
  }

  if (datahandler_ == nullptr) {
    LIEF_DEBUG("Set 0x{:x} bytes in the cache of section '{}'", data.size(), name());
    content_c_ = data;
    size(data.size());
    return;
  }

  LIEF_DEBUG("Set 0x{:x} bytes in the data handler@0x{:x} of section '{}'",
             data.size(), file_offset(), name());


  auto res = datahandler_->get(file_offset(), size(), DataHandler::Node::SECTION);
  if (!res) {
    LIEF_ERR("Can't find the node. The section's content can't be updated");
    return;
  }

  DataHandler::Node& node = res.value();

  std::vector<uint8_t>& binary_content = datahandler_->content();
  datahandler_->reserve(node.offset(), data.size());

  if (node.size() < data.size()) {
    LIEF_INFO("You inserted 0x{:x} bytes in the section '{}' which is 0x{:x} wide",
              data.size(), name(), node.size());
  }

  auto max_offset = (int64_t)node.offset() + (int64_t)data.size();
  if (max_offset < 0 || max_offset > (int64_t)binary_content.size()) {
    LIEF_ERR("Write out of range");
    return;
  }

  size(data.size());

  std::copy(std::begin(data), std::end(data),
            std::begin(binary_content) + node.offset());

}


void Section::content(std::vector<uint8_t>&& data) {
  if (is_frame()) {
    return;
  }
  if (!data.empty() && type() == TYPE::NOBITS) {
    LIEF_INFO("You inserted 0x{:x} bytes in section '{}' which has SHT_NOBITS type",
              data.size(), name());
  }

  if (datahandler_ == nullptr) {
    LIEF_DEBUG("Set 0x{:x} bytes in the cache of section '{}'", data.size(), name());
    size(data.size());
    content_c_ = std::move(data);
    return;
  }

  LIEF_DEBUG("Set 0x{:x} bytes in the data handler@0x{:x} of section '{}'",
             data.size(), file_offset(), name());

  auto res = datahandler_->get(file_offset(), size(), DataHandler::Node::SECTION);
  if (!res) {
    LIEF_ERR("Can't find the node. The section's content can't be updated");
    return;
  }
  DataHandler::Node& node = res.value();

  std::vector<uint8_t>& binary_content = datahandler_->content();
  datahandler_->reserve(node.offset(), data.size());

  if (node.size() < data.size()) {
    LIEF_INFO("You inserted 0x{:x} bytes in the section '{}' which is 0x{:x} wide",
              data.size(), name(), node.size());
  }

  size(data.size());

  auto max_offset = (int64_t)node.offset() + (int64_t)data.size();
  if (max_offset < 0 || max_offset > (int64_t)binary_content.size()) {
    LIEF_ERR("Write out of range");
    return;
  }

  std::move(std::begin(data), std::end(data),
            std::begin(binary_content) + node.offset());
}

bool Section::has(Section::FLAGS flag) const {
  if ((flags_ & SHF_MASKPROC) == 0) {
    return (flags_ & (static_cast<uint64_t>(flag) & FLAG_MASK)) != 0;
  }

  uint64_t raw_flag = static_cast<uint64_t>(flag);
  size_t id = raw_flag >> uint8_t(FLAGS::_ID_SHIFT_);

  if (id > 0 && arch_ == ARCH::NONE) {
    LIEF_WARN("Missing architecture. Can't determine whether the flag is present");
    return false;
  }

  if (id == uint8_t(FLAGS::_XCORE_ID_) && arch_ != ARCH::XCORE) {
    return false;
  }

  if (id == uint8_t(FLAGS::_X86_64_ID_) && arch_ != ARCH::X86_64) {
    return false;
  }

  if (id == uint8_t(FLAGS::_HEX_ID_) && arch_ != ARCH::HEXAGON) {
    return false;
  }

  if (id == uint8_t(FLAGS::_MIPS_ID_) && arch_ != ARCH::MIPS) {
    return false;
  }

  if (id == uint8_t(FLAGS::_ARM_ID_) && arch_ != ARCH::ARM) {
    return false;
  }

  return (flags_ & (static_cast<uint64_t>(flag) & FLAG_MASK)) != 0;
}

void Section::add(Section::FLAGS flag) {
  flags(flags() | (static_cast<uint64_t>(flag) & FLAG_MASK));
}

void Section::remove(Section::FLAGS flag) {
  const auto raw_flag = static_cast<uint64_t>(flag) & FLAG_MASK;
  flags(flags() & (~ raw_flag));
}

Section& Section::clear(uint8_t value) {
  if (is_frame()) {
    return *this;
  }
  if (datahandler_ == nullptr) {
    std::fill(std::begin(content_c_), std::end(content_c_), value);
    return *this;
  }

  std::vector<uint8_t>& binary_content = datahandler_->content();
  auto res = datahandler_->get(file_offset(), size(), DataHandler::Node::SECTION);
  if (!res) {
    LIEF_ERR("Can't find the node. The section's content can't be cleared");
    return *this;
  }
  DataHandler::Node& node = res.value();

  std::fill_n(std::begin(binary_content) + node.offset(), size(), value);
  return *this;
}

void Section::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

span<uint8_t> Section::writable_content() {
  if (is_frame()) {
    return {};
  }
  span<const uint8_t> ref = static_cast<const Section*>(this)->content();
  return {const_cast<uint8_t*>(ref.data()), ref.size()};
}

std::unique_ptr<SpanStream> Section::stream() const {
  return std::make_unique<SpanStream>(content());
}

std::ostream& operator<<(std::ostream& os, const Section& section) {
  const auto& flags = section.flags_list();

  os << fmt::format("{} ({}) 0x{:08x}/0x{:08x} 0x{:04x} {} {}",
                    section.name(), section.type(), section.virtual_address(),
                    section.file_offset(), section.size(),
                    section.entropy(), fmt::to_string(flags));

  return os;
}


const char* to_string(Section::TYPE e) {
  #define ENTRY(X) std::pair(Section::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(SHT_NULL_),
    ENTRY(PROGBITS),
    ENTRY(SYMTAB),
    ENTRY(STRTAB),
    ENTRY(RELA),
    ENTRY(HASH),
    ENTRY(DYNAMIC),
    ENTRY(NOTE),
    ENTRY(NOBITS),
    ENTRY(REL),
    ENTRY(SHLIB),
    ENTRY(DYNSYM),
    ENTRY(INIT_ARRAY),
    ENTRY(FINI_ARRAY),
    ENTRY(PREINIT_ARRAY),
    ENTRY(GROUP),
    ENTRY(SYMTAB_SHNDX),
    ENTRY(RELR),

    ENTRY(ANDROID_REL),
    ENTRY(ANDROID_RELA),
    ENTRY(LLVM_ADDRSIG),
    ENTRY(ANDROID_RELR),
    ENTRY(GNU_ATTRIBUTES),
    ENTRY(GNU_HASH),
    ENTRY(GNU_VERDEF),
    ENTRY(GNU_VERNEED),
    ENTRY(GNU_VERSYM),

    ENTRY(ARM_EXIDX),
    ENTRY(ARM_PREEMPTMAP),
    ENTRY(ARM_ATTRIBUTES),
    ENTRY(ARM_DEBUGOVERLAY),
    ENTRY(ARM_OVERLAYSECTION),
    ENTRY(HEX_ORDERED),
    ENTRY(X86_64_UNWIND),

    ENTRY(MIPS_LIBLIST),
    ENTRY(MIPS_MSYM),
    ENTRY(MIPS_CONFLICT),
    ENTRY(MIPS_GPTAB),
    ENTRY(MIPS_UCODE),
    ENTRY(MIPS_DEBUG),
    ENTRY(MIPS_REGINFO),
    ENTRY(MIPS_PACKAGE),
    ENTRY(MIPS_PACKSYM),
    ENTRY(MIPS_RELD),
    ENTRY(MIPS_IFACE),
    ENTRY(MIPS_CONTENT),
    ENTRY(MIPS_OPTIONS),
    ENTRY(MIPS_SHDR),
    ENTRY(MIPS_FDESC),
    ENTRY(MIPS_EXTSYM),
    ENTRY(MIPS_DENSE),
    ENTRY(MIPS_PDESC),
    ENTRY(MIPS_LOCSYM),
    ENTRY(MIPS_AUXSYM),
    ENTRY(MIPS_OPTSYM),
    ENTRY(MIPS_LOCSTR),
    ENTRY(MIPS_LINE),
    ENTRY(MIPS_RFDESC),
    ENTRY(MIPS_DELTASYM),
    ENTRY(MIPS_DELTAINST),
    ENTRY(MIPS_DELTACLASS),
    ENTRY(MIPS_DWARF),
    ENTRY(MIPS_DELTADECL),
    ENTRY(MIPS_SYMBOL_LIB),
    ENTRY(MIPS_EVENTS),
    ENTRY(MIPS_TRANSLATE),
    ENTRY(MIPS_PIXIE),
    ENTRY(MIPS_XLATE),
    ENTRY(MIPS_XLATE_DEBUG),
    ENTRY(MIPS_WHIRL),
    ENTRY(MIPS_EH_REGION),
    ENTRY(MIPS_XLATE_OLD),
    ENTRY(MIPS_PDR_EXCEPTION),
    ENTRY(MIPS_ABIFLAGS),
    ENTRY(MIPS_XHASH),

    ENTRY(RISCV_ATTRIBUTES),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(Section::FLAGS e) {
  #define ENTRY(X) std::pair(Section::FLAGS::X, #X)
  STRING_MAP enums2str {
    ENTRY(NONE),
    ENTRY(WRITE),
    ENTRY(ALLOC),
    ENTRY(EXECINSTR),
    ENTRY(MERGE),
    ENTRY(STRINGS),
    ENTRY(INFO_LINK),
    ENTRY(LINK_ORDER),
    ENTRY(OS_NONCONFORMING),
    ENTRY(GROUP),
    ENTRY(TLS),
    ENTRY(COMPRESSED),
    ENTRY(GNU_RETAIN),
    ENTRY(EXCLUDE),
    ENTRY(XCORE_SHF_DP_SECTION),
    ENTRY(XCORE_SHF_CP_SECTION),
    ENTRY(X86_64_LARGE),
    ENTRY(HEX_GPREL),

    ENTRY(MIPS_NODUPES),
    ENTRY(MIPS_NAMES),
    ENTRY(MIPS_LOCAL),
    ENTRY(MIPS_NOSTRIP),
    ENTRY(MIPS_GPREL),
    ENTRY(MIPS_MERGE),
    ENTRY(MIPS_ADDR),
    ENTRY(MIPS_STRING),
    ENTRY(ARM_PURECODE),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}
