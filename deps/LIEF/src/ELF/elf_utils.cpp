#include "ELF/elf_utils.hpp"
#include "ELF/Structures.hpp"

#include "logging.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/ELF/Header.hpp"
#include "LIEF/ELF/Segment.hpp"

namespace LIEF::ELF {

optional<elf_file_info_t> get_info(BinaryStream& strm) {
  elf_file_info_t info{};
  auto header = strm.read<ELF::details::Elf64_Ehdr>();
  if (!header) {
    LIEF_ERR("Failed to read ELF Header");
    return nullopt();
  }

  const auto cls = static_cast<Header::CLASS>(header->e_ident[Header::ELI_CLASS]);
  const auto data =
      static_cast<Header::ELF_DATA>(header->e_ident[Header::ELI_DATA]);
  if (cls != Header::CLASS::ELF64 || data != Header::ELF_DATA::LSB) {
    LIEF_WARN("{} only supports 64-bit little-endian ELF images", __FUNCTION__);
    return nullopt();
  }

  strm.setpos(header->e_phoff);
  info.phnum = header->e_phnum;
  info.phdr_off = header->e_phoff;
  for (size_t i = 0; i < header->e_phnum; ++i) {
    auto phdr = strm.read<ELF::details::Elf64_Phdr>();
    if (!phdr) {
      LIEF_WARN("Failed to read Elf_Phdr[{}]", i);
      break;
    }
    static_assert((int)Segment::TYPE::LOAD == 1);
    static_assert((int)Segment::TYPE::PHDR == 6);
    if (phdr->p_type == (int)Segment::TYPE::PHDR) {
      info.phdr_off = phdr->p_vaddr;
    } else if (phdr->p_type == (int)Segment::TYPE::LOAD) {
      info.imagebase = std::min<uintptr_t>(info.imagebase, phdr->p_vaddr);
      info.end_address =
          std::max<uintptr_t>(info.end_address, phdr->p_vaddr + phdr->p_memsz);
    }
  }

  if (info.imagebase == static_cast<uintptr_t>(-1) ||
      info.end_address < info.imagebase)
  {
    LIEF_WARN("No loadable segment found while probing the ELF image");
    return nullopt();
  }

  return info;
}


}
