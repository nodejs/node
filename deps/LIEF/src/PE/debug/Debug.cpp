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
#include <sstream>
#include "LIEF/Visitor.hpp"
#include "LIEF/PE/debug/Debug.hpp"
#include "LIEF/PE/Section.hpp"
#include "PE/Structures.hpp"

#include "frozen.hpp"
#include "spdlog/fmt/fmt.h"
#include "overflow_check.hpp"


namespace LIEF::PE {

span<uint8_t> Debug::get_payload(Section& section, uint32_t /*rva*/,
                                 uint32_t offset, uint32_t size) {
  span<uint8_t> content = section.writable_content();
  if (size == 0 || content.empty() || content.size() < size) {
    return {};
  }

  int32_t rel_offset = (int32_t)offset - (int32_t)section.offset();
  if (rel_offset < 0) {
    return {};
  }

  if ((size_t)rel_offset >= content.size() || (rel_offset + size) > content.size())
  {
    return {};
  }

  if (check_overflow<uint64_t>((uint32_t)rel_offset, size, content.size())) {
    return {};
  }

  return content.subspan((uint32_t)rel_offset, size);
}

span<uint8_t> Debug::get_payload(Section& section, const details::pe_debug& hdr) {
  return get_payload(section, hdr.AddressOfRawData, hdr.PointerToRawData,
                     hdr.SizeOfData);
}

Debug::Debug(const details::pe_debug& debug_s, Section* sec) :
  type_{static_cast<TYPES>(debug_s.Type)},
  characteristics_{debug_s.Characteristics},
  timestamp_{debug_s.TimeDateStamp},
  major_version_{debug_s.MajorVersion},
  minor_version_{debug_s.MinorVersion},
  sizeof_data_{debug_s.SizeOfData},
  addressof_rawdata_{debug_s.AddressOfRawData},
  pointerto_rawdata_{debug_s.PointerToRawData},
  section_{sec} {}

span<uint8_t> Debug::payload() {
  if (section_ == nullptr) {
    return {};
  }
  return get_payload(*section_, *this);
}

void Debug::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::string Debug::to_string() const {
  std::ostringstream os;
  os << fmt::format("Characteristics:     {:#x}\n", characteristics())
     << fmt::format("Timestamp:           {:#x}\n", timestamp())
     << fmt::format("Major/Minor version: {}.{}\n", major_version(),
                    minor_version())
     << fmt::format("Type:                {}\n", PE::to_string(type()))
     << fmt::format("Size of data:        {:#x}\n", sizeof_data())
     << fmt::format("Address of rawdata:  {:#x}\n", addressof_rawdata())
     << fmt::format("Pointer to rawdata:  {:#x}", pointerto_rawdata());
  return os.str();
}

const char* to_string(Debug::TYPES e) {
#define ENTRY(X) std::pair(Debug::TYPES::X, #X)
  STRING_MAP enums2str{
      ENTRY(UNKNOWN),
      ENTRY(COFF),
      ENTRY(CODEVIEW),
      ENTRY(FPO),
      ENTRY(MISC),
      ENTRY(EXCEPTION),
      ENTRY(FIXUP),
      ENTRY(OMAP_TO_SRC),
      ENTRY(OMAP_FROM_SRC),
      ENTRY(BORLAND),
      ENTRY(RESERVED10),
      ENTRY(CLSID),
      ENTRY(VC_FEATURE),
      ENTRY(POGO),
      ENTRY(ILTCG),
      ENTRY(MPX),
      ENTRY(REPRO),
      ENTRY(PDBCHECKSUM),
      ENTRY(EX_DLLCHARACTERISTICS),
  };
#undef ENTRY

  if (const auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
