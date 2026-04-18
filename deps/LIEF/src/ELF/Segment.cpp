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

#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/EnumToString.hpp"
#include "LIEF/ELF/Section.hpp"

#include "ELF/DataHandler/Handler.hpp"
#include "ELF/Structures.hpp"

namespace LIEF {
namespace ELF {

static constexpr auto PT_LOPROC = 0x70000000;
static constexpr auto PT_HIPROC = 0x7fffffff;

static constexpr auto PT_LOOS = 0x60000000;
static constexpr auto PT_HIOS = 0x6fffffff;

Segment::TYPE Segment::type_from(uint64_t value, ARCH arch, Header::OS_ABI os) {
  using OS_ABI = Header::OS_ABI;
  if (PT_LOPROC <= value && value <= PT_HIPROC) {
    if (arch == ARCH::NONE) {
      LIEF_WARN("Segment type 0x{:08x} requires to know the architecture", value);
      return TYPE::UNKNOWN;
    }
    switch (arch) {
      case ARCH::ARM:
        return TYPE(value | PT_ARM);

      case ARCH::AARCH64:
        return TYPE(value | PT_AARCH64);

      case ARCH::MIPS:
        return TYPE(value | PT_MIPS);

      case ARCH::RISCV:
        return TYPE(value | PT_RISCV);

      case ARCH::IA_64:
        return TYPE(value | PT_IA_64);

      default:
        {
          LIEF_WARN("Segment type 0x{:08x} is unknown for the architecture {}",
                     value, to_string(arch));
          return TYPE::UNKNOWN;
        }
    }
  }

  // OS-specific type
  if (PT_LOOS <= value && value <= PT_HIOS) {
    if (os == OS_ABI::HPUX) {
      return TYPE(value | PT_HPUX);
    }
  }
  return TYPE(value);
}

Segment::Segment(const Segment& other) :
  Object{other},
  type_{other.type_},
  arch_{other.arch_},
  flags_{other.flags_},
  file_offset_{other.file_offset_},
  virtual_address_{other.virtual_address_},
  physical_address_{other.physical_address_},
  size_{other.size_},
  virtual_size_{other.virtual_size_},
  alignment_{other.alignment_},
  handler_size_{other.handler_size_},
  content_c_{other.content_c_}
{}

template<class T>
Segment::Segment(const T& header, ARCH arch, Header::OS_ABI os) :
  type_{type_from(header.p_type, arch, os)},
  arch_(arch),
  flags_{header.p_flags},
  file_offset_{header.p_offset},
  virtual_address_{header.p_vaddr},
  physical_address_{header.p_paddr},
  size_{header.p_filesz},
  virtual_size_{header.p_memsz},
  alignment_{header.p_align},
  handler_size_{header.p_filesz}
{}

template Segment::Segment(const details::Elf32_Phdr& header, ARCH, Header::OS_ABI);
template Segment::Segment(const details::Elf64_Phdr& header, ARCH, Header::OS_ABI);

void Segment::swap(Segment& other) {
  std::swap(type_,             other.type_);
  std::swap(arch_,             other.arch_);
  std::swap(flags_,            other.flags_);
  std::swap(file_offset_,      other.file_offset_);
  std::swap(virtual_address_,  other.virtual_address_);
  std::swap(physical_address_, other.physical_address_);
  std::swap(size_,             other.size_);
  std::swap(virtual_size_,     other.virtual_size_);
  std::swap(alignment_,        other.alignment_);
  std::swap(handler_size_,     other.handler_size_);
  std::swap(sections_,         other.sections_);
  std::swap(datahandler_,      other.datahandler_);
  std::swap(content_c_,        other.content_c_);
}

Segment& Segment::operator=(Segment other) {
  swap(other);
  return *this;
}

result<Segment> Segment::from_raw(const uint8_t* ptr, size_t size) {

  if (size != sizeof(details::Elf32_Phdr) &&
      size != sizeof(details::Elf64_Phdr))
  {
    LIEF_ERR("The size of the provided data does not match a valid header size");
    return make_error_code(lief_errors::corrupted);
  }

  if (size == sizeof(details::Elf32_Phdr)) {
    return Segment(*reinterpret_cast<const details::Elf32_Phdr*>(ptr));
  }

  if (size == sizeof(details::Elf64_Phdr)) {
    return Segment(*reinterpret_cast<const details::Elf64_Phdr*>(ptr));
  }

  return make_error_code(lief_errors::not_implemented);
}

span<const uint8_t> Segment::content() const {
  if (datahandler_ == nullptr) {
    LIEF_DEBUG("Get content of segment {}@0x{:x} from cache",
               to_string(type()), virtual_address());
    return content_c_;
  }

  auto res = datahandler_->get(file_offset(), handler_size(), DataHandler::Node::SEGMENT);
  if (!res) {
    LIEF_ERR("Can't find the node. The segment's content can't be accessed");
    return {};
  }
  DataHandler::Node& node = res.value();

  // Create a span based on our values
  const std::vector<uint8_t>& binary_content = datahandler_->content();
  const size_t size = binary_content.size();
  if (node.offset() >= size) {
    LIEF_ERR("Can't access content of segment {}:0x{:x}",
             to_string(type()), virtual_address());
    return {};
  }

  const uint8_t* ptr = binary_content.data() + node.offset();

  /* node.size() overflow */
  if (node.offset() + node.size() < node.offset()) {
    return {};
  }

  if ((node.offset() + node.size()) >= size) {
    if ((node.offset() + handler_size()) <= size) {
      return {ptr, static_cast<size_t>(handler_size())};
    }
    LIEF_ERR("Can't access content of segment {}:0x{:x}",
             to_string(type()), virtual_address());
    return {};
  }

  return {ptr, static_cast<size_t>(node.size())};
}

size_t Segment::get_content_size() const {
  if (datahandler_ == nullptr) {
    return content_c_.size();
  }
  auto res = datahandler_->get(file_offset(), handler_size(), DataHandler::Node::SEGMENT);
  if (!res) {
    LIEF_ERR("Can't find the node");
    return 0;
  }
  DataHandler::Node& node = res.value();
  return node.size();
}

template<typename T>
T Segment::get_content_value(size_t offset) const {
  T ret;
  if (datahandler_ == nullptr) {
    LIEF_DEBUG("Get content of segment {}@0x{:x} from cache",
               to_string(type()), virtual_address());
    memcpy(&ret, content_c_.data() + offset, sizeof(T));
  } else {
    auto res = datahandler_->get(file_offset(), handler_size(), DataHandler::Node::SEGMENT);
    if (!res) {
      LIEF_ERR("Can't find the node associated with this segment");
      memset(&ret, 0, sizeof(T));
      return ret;
    }
    const std::vector<uint8_t>& binary_content = datahandler_->content();
    DataHandler::Node& node = res.value();
    memcpy(&ret, binary_content.data() + node.offset() + offset, sizeof(T));
  }
  return ret;
}

template unsigned short Segment::get_content_value<unsigned short>(size_t offset) const;
template unsigned int Segment::get_content_value<unsigned int>(size_t offset) const;
template unsigned long Segment::get_content_value<unsigned long>(size_t offset) const;
template unsigned long long Segment::get_content_value<unsigned long long>(size_t offset) const;

template<typename T>
void Segment::set_content_value(size_t offset, T value) {
  if (datahandler_ == nullptr) {
    LIEF_DEBUG("Set content of segment {}@0x{:x}:0x{:x} in cache (0x{:x} bytes)",
        to_string(type()), virtual_address(), offset, sizeof(T));
    if (offset + sizeof(T) > content_c_.size()) {
      content_c_.resize(offset + sizeof(T));
      physical_size(offset + sizeof(T));
    }
    memcpy(content_c_.data() + offset, &value, sizeof(T));
  } else {
    auto res = datahandler_->get(file_offset(), handler_size(), DataHandler::Node::SEGMENT);
    if (!res) {
      LIEF_ERR("Can't find the node associated with this segment. The content can't be updated");
      return;
    }
    DataHandler::Node& node = res.value();
    std::vector<uint8_t>& binary_content = datahandler_->content();

    if (offset + sizeof(T) > binary_content.size()) {
      datahandler_->reserve(node.offset(), offset + sizeof(T));
    }
    physical_size(node.size());
    memcpy(binary_content.data() + node.offset() + offset, &value, sizeof(T));
  }
}
template void Segment::set_content_value<unsigned short>(size_t offset, unsigned short value);
template void Segment::set_content_value<unsigned int>(size_t offset, unsigned int value);
template void Segment::set_content_value<unsigned long>(size_t offset, unsigned long value);
template void Segment::set_content_value<unsigned long long>(size_t offset, unsigned long long value);

bool Segment::has(const Section& section) const {
  auto it_section = std::find_if(std::begin(sections_), std::end(sections_),
      [&section] (const Section* s) {
        return *s == section;
      });
  return it_section != std::end(sections_);
}

bool Segment::has(const std::string& name) const {
  auto it_section = std::find_if(std::begin(sections_), std::end(sections_),
      [&name] (const Section* s) {
        return s->name() == name;
      });
  return it_section != std::end(sections_);
}

void Segment::add(Segment::FLAGS flag) {
  flags(flags() | flag);
}

void Segment::remove(Segment::FLAGS flag) {
  flags(flags() & ~flag);
}

void Segment::file_offset(uint64_t file_offset) {
  if (datahandler_ != nullptr) {
    auto res = datahandler_->get(this->file_offset(), handler_size(), DataHandler::Node::SEGMENT);
    if (res) {
      res->get().offset(file_offset);
    } else {
      LIEF_ERR("Can't find the node. The file offset can't be updated");
      return;
    }
  }
  file_offset_ = file_offset;
}

void Segment::physical_size(uint64_t physical_size) {
  if (datahandler_ != nullptr) {
    auto node = datahandler_->get(file_offset(), handler_size(), DataHandler::Node::SEGMENT);
    if (node) {
      node->get().size(physical_size);
      handler_size_ = physical_size;
    } else {
      LIEF_ERR("Can't find the node. The physical size can't be updated");
    }
  }
  size_ = physical_size;
}

void Segment::content(std::vector<uint8_t> content) {
  if (datahandler_ == nullptr) {
    LIEF_DEBUG("Set content of segment {}@0x{:x} in cache (0x{:x} bytes)",
               to_string(type()), virtual_address(), content.size());
    physical_size(content.size());
    content_c_ = std::move(content);
    return;
  }

  LIEF_DEBUG("Set content of segment {}@0x{:x} in data handler @0x{:x} (0x{:x} bytes)",
             to_string(type()), virtual_address(), file_offset(), content.size());

  auto res = datahandler_->get(file_offset(), handler_size(), DataHandler::Node::SEGMENT);
  if (!res) {
    LIEF_ERR("Can't find the node for updating content");
    return;
  }
  DataHandler::Node& node = res.value();

  std::vector<uint8_t>& binary_content = datahandler_->content();
  datahandler_->reserve(node.offset(), content.size());

  if (node.size() < content.size()) {
      LIEF_INFO("You inserted 0x{:x} bytes in the segment {}@0x{:x} which is 0x{:x} wide",
                content.size(), to_string(type()), virtual_size(), node.size());
  }

  auto max_offset = (int64_t)node.offset() + (int64_t)content.size();
  if (max_offset < 0 || max_offset > (int64_t)binary_content.size()) {
    LIEF_ERR("Write out of range");
    return;
  }

  physical_size(node.size());

  std::move(std::begin(content), std::end(content),
            std::begin(binary_content) + node.offset());
}

void Segment::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::unique_ptr<SpanStream> Segment::stream() const {
  return std::make_unique<SpanStream>(content());
}

span<uint8_t> Segment::writable_content() {
  span<const uint8_t> ref = static_cast<const Segment*>(this)->content();
  return {const_cast<uint8_t*>(ref.data()), ref.size()};
}

uint64_t Segment::handler_size() const {
  if (handler_size_ > 0) {
    return handler_size_;
  }
  return physical_size();
}

std::ostream& operator<<(std::ostream& os, const Segment& segment) {
  std::string flags = "---";

  if (segment.has(Segment::FLAGS::R)) {
    flags[0] = 'r';
  }

  if (segment.has(Segment::FLAGS::W)) {
    flags[1] = 'w';
  }

  if (segment.has(Segment::FLAGS::X)) {
    flags[2] = 'x';
  }

  std::string segment_ty = to_string(segment.type());
  if (segment_ty == "UNKNOWN") {
    segment_ty = fmt::format("UNKNOWN[0x{:08x}]", (uint32_t)segment.type());
  }

  os << fmt::format("{} 0x{:08x}/0x{:06x} 0x{:06x} 0x{:04x}/0x{:04x} {} {}",
                    segment_ty, segment.virtual_address(),
                    segment.file_offset(), segment.physical_address(),
                    segment.physical_size(), segment.virtual_size(),
                    segment.alignment(), flags);
  return os;
}

const char* to_string(Segment::TYPE e) {
  #define ENTRY(X) std::pair(Segment::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(PT_NULL_),
    ENTRY(LOAD),
    ENTRY(DYNAMIC),
    ENTRY(INTERP),
    ENTRY(NOTE),
    ENTRY(SHLIB),
    ENTRY(PHDR),
    ENTRY(TLS),
    ENTRY(GNU_EH_FRAME),
    ENTRY(GNU_STACK),
    ENTRY(GNU_PROPERTY),
    ENTRY(GNU_RELRO),
    ENTRY(PAX_FLAGS),
    ENTRY(ARM_ARCHEXT),
    ENTRY(ARM_EXIDX),
    ENTRY(AARCH64_MEMTAG_MTE),
    ENTRY(MIPS_REGINFO),
    ENTRY(MIPS_RTPROC),
    ENTRY(MIPS_OPTIONS),
    ENTRY(MIPS_ABIFLAGS),
    ENTRY(RISCV_ATTRIBUTES),
    ENTRY(IA_64_EXT),
    ENTRY(IA_64_UNWIND),
    ENTRY(HP_TLS),
    ENTRY(HP_CORE_NONE),
    ENTRY(HP_CORE_VERSION),
    ENTRY(HP_CORE_KERNEL),
    ENTRY(HP_CORE_COMM),
    ENTRY(HP_CORE_PROC),
    ENTRY(HP_CORE_LOADABLE),
    ENTRY(HP_CORE_STACK),
    ENTRY(HP_CORE_SHM),
    ENTRY(HP_CORE_MMF),
    ENTRY(HP_PARALLEL),
    ENTRY(HP_FASTBIND),
    ENTRY(HP_OPT_ANNOT),
    ENTRY(HP_HSL_ANNOT),
    ENTRY(HP_STACK),
    ENTRY(HP_CORE_UTSNAME),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(Segment::FLAGS e) {
  switch (e) {
    case Segment::FLAGS::NONE: return "NONE";
    case Segment::FLAGS::R: return "R";
    case Segment::FLAGS::W: return "W";
    case Segment::FLAGS::X: return "X";
  }
  return "UNKNOWN";
}
}
}
