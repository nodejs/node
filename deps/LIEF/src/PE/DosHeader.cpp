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
#include "LIEF/Visitor.hpp"

#include "LIEF/PE/DosHeader.hpp"
#include "PE/Structures.hpp"

#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/ranges.h"

namespace LIEF {
namespace PE {

DosHeader::DosHeader(const details::pe_dos_header& header) :
  magic_{header.Magic},
  used_bytes_in_last_page_{header.UsedBytesInTheLastPage},
  file_sz_in_pages_{header.FileSizeInPages},
  nb_relocations_{header.NumberOfRelocationItems},
  header_sz_in_paragraphs_{header.HeaderSizeInParagraphs},
  min_extra_paragraphs_{header.MinimumExtraParagraphs},
  max_extra_paragraphs_{header.MaximumExtraParagraphs},
  init_relative_ss_{header.InitialRelativeSS},
  init_sp_{header.InitialSP},
  checksum_{header.Checksum},
  init_ip_{header.InitialIP},
  init_rel_cs_{header.InitialRelativeCS},
  addr_reloc_table_{header.AddressOfRelocationTable},
  overlay_number_{header.OverlayNumber},
  oem_id_{header.OEMid},
  oem_info_{header.OEMinfo},
  addr_new_exe_header_{header.AddressOfNewExeHeader}
{
  std::copy(std::begin(header.Reserved), std::end(header.Reserved),
            std::begin(reserved_));

  std::copy(std::begin(header.Reserved2), std::end(header.Reserved2),
            std::begin(reserved2_));
}

void DosHeader::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}


DosHeader DosHeader::create(PE_TYPE type) {
  DosHeader hdr;
  hdr.magic(DosHeader::MAGIC);
  hdr.header_size_in_paragraphs(4);
  hdr.addressof_relocation_table(0x40);
  hdr.file_size_in_pages(3);
  hdr.maximum_extra_paragraphs(0xff);
  hdr.initial_sp(0xb8);
  hdr.used_bytes_in_last_page(0x90);
  hdr.addressof_new_exeheader(type == PE_TYPE::PE32 ? 0xe8 : 0xf0);
  return hdr;
}

std::ostream& operator<<(std::ostream& os, const DosHeader& entry) {
  os << fmt::format("Magic:                       0x{:x}\n", entry.magic())
     << fmt::format("Used bytes in last page:     0x{:x}\n", entry.used_bytes_in_last_page())
     << fmt::format("File size in pages:          0x{:x}\n", entry.file_size_in_pages())
     << fmt::format("Number of relocation:        0x{:x}\n", entry.numberof_relocation())
     << fmt::format("Header size in paragraphs:   0x{:x}\n", entry.header_size_in_paragraphs())
     << fmt::format("Min extra paragraphs:        0x{:x}\n", entry.minimum_extra_paragraphs())
     << fmt::format("Max extra paragraphs:        0x{:x}\n", entry.maximum_extra_paragraphs())
     << fmt::format("Initial relative SS:         0x{:x}\n", entry.initial_relative_ss())
     << fmt::format("Initial SP:                  0x{:x}\n", entry.initial_sp())
     << fmt::format("Checksum:                    0x{:x}\n", entry.checksum())
     << fmt::format("Initial IP:                  0x{:x}\n", entry.initial_ip())
     << fmt::format("Initial relative CS:         0x{:x}\n", entry.initial_relative_cs())
     << fmt::format("Address of relocation table: 0x{:x}\n", entry.addressof_relocation_table())
     << fmt::format("Overlay number:              0x{:x}\n", entry.overlay_number())
     << fmt::format("OEM id:                      0x{:x}\n", entry.oem_id())
     << fmt::format("OEM info:                    0x{:x}\n", entry.oem_info())
     << fmt::format("Address of new exe-header:   0x{:x}\n", entry.addressof_new_exeheader())
     << fmt::format("Reserved1:                   {}\n", entry.reserved())
     << fmt::format("Reserved2:                   {}\n", entry.reserved2());
  return os;
}

}
}
