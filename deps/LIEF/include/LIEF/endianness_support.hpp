/* Copyright 2021 - 2025 R. Thomas
 * Copyright 2021 - 2025 Quarkslab
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
#ifndef LIEF_ENDIANNESS_SUPPORT_H
#define LIEF_ENDIANNESS_SUPPORT_H
#include <cstdint>
#include "LIEF/config.h"
#include "LIEF/visibility.h"

#define LIEF_ENDIAN_SUPPORT(X) \
  template<> \
  LIEF_API void swap_endian<X>(X* hdr)

namespace LIEF {

namespace ELF {
namespace details {
struct Elf32_Auxv;
struct Elf32_Dyn;
struct Elf32_Ehdr;
struct Elf32_FileEntry;
struct Elf32_Phdr;
struct Elf32_Rel;
struct Elf32_Rela;
struct Elf32_Shdr;
struct Elf32_Sym;
struct Elf32_Verdaux;
struct Elf32_Verdef;
struct Elf32_Vernaux;
struct Elf32_Verneed;
struct Elf64_Auxv;
struct Elf64_Dyn;
struct Elf64_Ehdr;
struct Elf64_FileEntry;
struct Elf64_Phdr;
struct Elf64_Rel;
struct Elf64_Rela;
struct Elf64_Shdr;
struct Elf64_Sym;
struct Elf64_Verdaux;
struct Elf64_Verdef;
struct Elf64_Vernaux;
struct Elf64_Verneed;
}
}

namespace MachO {
namespace details {
struct arm_thread_state64_t;
struct arm_thread_state_t;
struct build_tool_version;
struct build_version_command;
struct data_in_code_entry;
struct dyld_info_command;
struct dylib_command;
struct dylib_module_32;
struct dylib_module_64;
struct dylib_reference;
struct dylib_table_of_contents;
struct dylinker_command;
struct dysymtab_command;
struct encryption_info_command;
struct entry_point_command;
struct fileset_entry_command;
struct fvmfile_command;
struct ident_command;
struct linkedit_data_command;
struct linker_option_command;
struct load_command;
struct mach_header;
struct mach_header_64;
struct nlist_32;
struct nlist_64;
struct prebind_cksum_command;
struct prebound_dylib_command;
struct relocation_info;
struct routines_command_32;
struct routines_command_64;
struct rpath_command;
struct scattered_relocation_info;
struct section_32;
struct section_64;
struct segment_command_32;
struct segment_command_64;
struct source_version_command;
struct sub_client_command;
struct sub_framework_command;
struct sub_library_command;
struct sub_umbrella_command;
struct symseg_command;
struct symtab_command;
struct thread_command;
struct twolevel_hint;
struct twolevel_hints_command;
struct uuid_command;
struct version_min_command;
struct x86_thread_state64_t;
struct x86_thread_state_t;
struct ppc_thread_state_t;
struct ppc_thread_state64_t;
}
}

template<typename T>
void swap_endian(T*) {
}

template<typename T>
T get_swapped_endian(const T& other) {
  T tmp = other;
  swap_endian(&tmp);
  return tmp;
}

LIEF_ENDIAN_SUPPORT(char);
LIEF_ENDIAN_SUPPORT(char16_t);

LIEF_ENDIAN_SUPPORT(uint8_t);
LIEF_ENDIAN_SUPPORT(uint16_t);
LIEF_ENDIAN_SUPPORT(uint32_t);
LIEF_ENDIAN_SUPPORT(uint64_t);

LIEF_ENDIAN_SUPPORT(int8_t);
LIEF_ENDIAN_SUPPORT(int16_t);
LIEF_ENDIAN_SUPPORT(int32_t);
LIEF_ENDIAN_SUPPORT(int64_t);

#if defined (LIEF_ELF_SUPPORT)
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Auxv);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Dyn);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Ehdr);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_FileEntry);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Phdr);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Rel);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Rela);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Shdr);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Sym);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Verdaux);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Verdef);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Vernaux);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf32_Verneed);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Auxv);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Dyn);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Ehdr);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_FileEntry);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Phdr);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Rel);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Rela);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Shdr);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Sym);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Verdaux);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Verdef);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Vernaux);
LIEF_ENDIAN_SUPPORT(ELF::details::Elf64_Verneed);
#endif

#if defined (LIEF_MACHO_SUPPORT)
LIEF_ENDIAN_SUPPORT(MachO::details::arm_thread_state64_t);
LIEF_ENDIAN_SUPPORT(MachO::details::arm_thread_state_t);
LIEF_ENDIAN_SUPPORT(MachO::details::build_tool_version);
LIEF_ENDIAN_SUPPORT(MachO::details::build_version_command);
LIEF_ENDIAN_SUPPORT(MachO::details::data_in_code_entry);
LIEF_ENDIAN_SUPPORT(MachO::details::dyld_info_command);
LIEF_ENDIAN_SUPPORT(MachO::details::dylib_command);
LIEF_ENDIAN_SUPPORT(MachO::details::dylib_module_32);
LIEF_ENDIAN_SUPPORT(MachO::details::dylib_module_64);
LIEF_ENDIAN_SUPPORT(MachO::details::dylib_reference);
LIEF_ENDIAN_SUPPORT(MachO::details::dylib_table_of_contents);
LIEF_ENDIAN_SUPPORT(MachO::details::dylinker_command);
LIEF_ENDIAN_SUPPORT(MachO::details::dysymtab_command);
LIEF_ENDIAN_SUPPORT(MachO::details::encryption_info_command);
LIEF_ENDIAN_SUPPORT(MachO::details::entry_point_command);
LIEF_ENDIAN_SUPPORT(MachO::details::fileset_entry_command);
LIEF_ENDIAN_SUPPORT(MachO::details::fvmfile_command);
LIEF_ENDIAN_SUPPORT(MachO::details::ident_command);
LIEF_ENDIAN_SUPPORT(MachO::details::linkedit_data_command);
LIEF_ENDIAN_SUPPORT(MachO::details::linker_option_command);
LIEF_ENDIAN_SUPPORT(MachO::details::load_command);
LIEF_ENDIAN_SUPPORT(MachO::details::mach_header);
LIEF_ENDIAN_SUPPORT(MachO::details::mach_header_64);
LIEF_ENDIAN_SUPPORT(MachO::details::nlist_32);
LIEF_ENDIAN_SUPPORT(MachO::details::nlist_64);
LIEF_ENDIAN_SUPPORT(MachO::details::prebind_cksum_command);
LIEF_ENDIAN_SUPPORT(MachO::details::prebound_dylib_command);
LIEF_ENDIAN_SUPPORT(MachO::details::relocation_info);
LIEF_ENDIAN_SUPPORT(MachO::details::routines_command_32);
LIEF_ENDIAN_SUPPORT(MachO::details::routines_command_64);
LIEF_ENDIAN_SUPPORT(MachO::details::rpath_command);
LIEF_ENDIAN_SUPPORT(MachO::details::scattered_relocation_info);
LIEF_ENDIAN_SUPPORT(MachO::details::section_32);
LIEF_ENDIAN_SUPPORT(MachO::details::section_64);
LIEF_ENDIAN_SUPPORT(MachO::details::segment_command_32);
LIEF_ENDIAN_SUPPORT(MachO::details::segment_command_64);
LIEF_ENDIAN_SUPPORT(MachO::details::source_version_command);
LIEF_ENDIAN_SUPPORT(MachO::details::sub_client_command);
LIEF_ENDIAN_SUPPORT(MachO::details::sub_framework_command);
LIEF_ENDIAN_SUPPORT(MachO::details::sub_library_command);
LIEF_ENDIAN_SUPPORT(MachO::details::sub_umbrella_command);
LIEF_ENDIAN_SUPPORT(MachO::details::symseg_command);
LIEF_ENDIAN_SUPPORT(MachO::details::symtab_command);
LIEF_ENDIAN_SUPPORT(MachO::details::thread_command);
LIEF_ENDIAN_SUPPORT(MachO::details::twolevel_hint);
LIEF_ENDIAN_SUPPORT(MachO::details::twolevel_hints_command);
LIEF_ENDIAN_SUPPORT(MachO::details::uuid_command);
LIEF_ENDIAN_SUPPORT(MachO::details::version_min_command);
LIEF_ENDIAN_SUPPORT(MachO::details::x86_thread_state64_t);
LIEF_ENDIAN_SUPPORT(MachO::details::x86_thread_state_t);
LIEF_ENDIAN_SUPPORT(MachO::details::ppc_thread_state64_t);
LIEF_ENDIAN_SUPPORT(MachO::details::ppc_thread_state_t);
#endif

}

#endif // LIEF_CONVERT_H
