/* Copyright 2021 - 2025 R. Thomas
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

#include "LIEF/endianness_support.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {

template<>
void swap_endian<MachO::details::mach_header>(MachO::details::mach_header* hdr) {
  swap_endian(&hdr->magic);
  swap_endian(&hdr->cputype);
  swap_endian(&hdr->cpusubtype);
  swap_endian(&hdr->filetype);
  swap_endian(&hdr->ncmds);
  swap_endian(&hdr->sizeofcmds);
  swap_endian(&hdr->flags);
}

template<>
void swap_endian<MachO::details::mach_header_64>(MachO::details::mach_header_64* hdr) {
  swap_endian(&hdr->magic);
  swap_endian(&hdr->cputype);
  swap_endian(&hdr->cpusubtype);
  swap_endian(&hdr->filetype);
  swap_endian(&hdr->ncmds);
  swap_endian(&hdr->sizeofcmds);
  swap_endian(&hdr->flags);
  swap_endian(&hdr->reserved);
}

template<>
void swap_endian<MachO::details::load_command>(MachO::details::load_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
}

template <typename segment_command>
void swap_endian_seg(segment_command* cmd) {
  swap_endian(&cmd->cmd);
  swap_endian(&cmd->cmdsize);
  swap_endian(&cmd->vmaddr);
  swap_endian(&cmd->vmsize);
  swap_endian(&cmd->fileoff);
  swap_endian(&cmd->filesize);
  swap_endian(&cmd->maxprot);
  swap_endian(&cmd->initprot);
  swap_endian(&cmd->nsects);
  swap_endian(&cmd->flags);
}

template<>
void swap_endian<MachO::details::segment_command_32>(MachO::details::segment_command_32* hdr) {
  swap_endian_seg(hdr);
}

template<>
void swap_endian<MachO::details::segment_command_64>(MachO::details::segment_command_64* hdr) {
  swap_endian_seg(hdr);
}

template <typename section>
void swap_endian_sec(section* hdr) {
  swap_endian(&hdr->addr);
  swap_endian(&hdr->size);
  swap_endian(&hdr->offset);
  swap_endian(&hdr->align);
  swap_endian(&hdr->reloff);
  swap_endian(&hdr->nreloc);
  swap_endian(&hdr->flags);
  swap_endian(&hdr->reserved1);
  swap_endian(&hdr->reserved2);
}

template<>
void swap_endian<MachO::details::section_32>(MachO::details::section_32* hdr) {
  swap_endian_sec(hdr);
}

template<>
void swap_endian<MachO::details::section_64>(MachO::details::section_64* hdr) {
  swap_endian_sec(hdr);
  swap_endian(&hdr->reserved3);
}

template<>
void swap_endian<MachO::details::dylib_command>(MachO::details::dylib_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->name);
  swap_endian(&hdr->timestamp);
  swap_endian(&hdr->current_version);
  swap_endian(&hdr->compatibility_version);
}

template<>
void swap_endian<MachO::details::sub_framework_command>(MachO::details::sub_framework_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->umbrella);
}

template<>
void swap_endian<MachO::details::sub_client_command>(MachO::details::sub_client_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->client);
}

template<>
void swap_endian<MachO::details::sub_umbrella_command>(MachO::details::sub_umbrella_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->sub_umbrella);
}

template<>
void swap_endian<MachO::details::sub_library_command>(MachO::details::sub_library_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->sub_library);
}

template<>
void swap_endian<MachO::details::prebound_dylib_command>(MachO::details::prebound_dylib_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->name);
  swap_endian(&hdr->nmodules);
  swap_endian(&hdr->linked_modules);
}

template<>
void swap_endian<MachO::details::dylinker_command>(MachO::details::dylinker_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->name);
}

template<>
void swap_endian<MachO::details::thread_command>(MachO::details::thread_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->flavor);
  swap_endian(&hdr->count);
}

template <typename routine>
void swap_endian_routine(routine* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->init_address);
  swap_endian(&hdr->init_module);
  swap_endian(&hdr->reserved1);
  swap_endian(&hdr->reserved2);
  swap_endian(&hdr->reserved3);
  swap_endian(&hdr->reserved4);
  swap_endian(&hdr->reserved5);
  swap_endian(&hdr->reserved6);
}

template<>
void swap_endian<MachO::details::routines_command_32>(MachO::details::routines_command_32* hdr) {
  return swap_endian_routine(hdr);
}

template<>
void swap_endian<MachO::details::routines_command_64>(MachO::details::routines_command_64* hdr) {
  return swap_endian_routine(hdr);
}

template<>
void swap_endian<MachO::details::symtab_command>(MachO::details::symtab_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->symoff);
  swap_endian(&hdr->nsyms);
  swap_endian(&hdr->stroff);
  swap_endian(&hdr->strsize);
}

template<>
void swap_endian<MachO::details::dysymtab_command>(MachO::details::dysymtab_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->ilocalsym);
  swap_endian(&hdr->nlocalsym);
  swap_endian(&hdr->iextdefsym);
  swap_endian(&hdr->nextdefsym);
  swap_endian(&hdr->iundefsym);
  swap_endian(&hdr->nundefsym);
  swap_endian(&hdr->tocoff);
  swap_endian(&hdr->ntoc);
  swap_endian(&hdr->modtaboff);
  swap_endian(&hdr->nmodtab);
  swap_endian(&hdr->extrefsymoff);
  swap_endian(&hdr->nextrefsyms);
  swap_endian(&hdr->indirectsymoff);
  swap_endian(&hdr->nindirectsyms);
  swap_endian(&hdr->extreloff);
  swap_endian(&hdr->nextrel);
  swap_endian(&hdr->locreloff);
  swap_endian(&hdr->nlocrel);
}

template<>
void swap_endian<MachO::details::dylib_table_of_contents>(MachO::details::dylib_table_of_contents* hdr) {
  swap_endian(&hdr->symbol_index);
  swap_endian(&hdr->module_index);
}

template <typename dylib_module>
void swap_endian_module(dylib_module* hdr) {
  swap_endian(&hdr->module_name);
  swap_endian(&hdr->iextdefsym);
  swap_endian(&hdr->nextdefsym);
  swap_endian(&hdr->irefsym);
  swap_endian(&hdr->nrefsym);
  swap_endian(&hdr->ilocalsym);
  swap_endian(&hdr->nlocalsym);
  swap_endian(&hdr->iextrel);
  swap_endian(&hdr->nextrel);
  swap_endian(&hdr->iinit_iterm);
  swap_endian(&hdr->ninit_nterm);
  swap_endian(&hdr->objc_module_info_addr);
  swap_endian(&hdr->objc_module_info_size);
}

template<>
void swap_endian<MachO::details::dylib_module_64>(MachO::details::dylib_module_64* hdr) {
  return swap_endian_module(hdr);
}

template<>
void swap_endian<MachO::details::dylib_module_32>(MachO::details::dylib_module_32* hdr) {
  return swap_endian_module(hdr);
}

template<>
void swap_endian<MachO::details::dylib_reference>(MachO::details::dylib_reference* hdr) {
  swap_endian((uint32_t*)hdr);
}

template<>
void swap_endian<MachO::details::twolevel_hints_command>(MachO::details::twolevel_hints_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->offset);
  swap_endian(&hdr->nhints);
}

template<>
void swap_endian<MachO::details::twolevel_hint>(MachO::details::twolevel_hint* hdr) {
  swap_endian((uint32_t*)hdr);
}


template<>
void swap_endian<MachO::details::prebind_cksum_command>(MachO::details::prebind_cksum_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->cksum);
}

template<>
void swap_endian<MachO::details::uuid_command>(MachO::details::uuid_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
}

template<>
void swap_endian<MachO::details::rpath_command>(MachO::details::rpath_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->path);
}

template<>
void swap_endian<MachO::details::linkedit_data_command>(MachO::details::linkedit_data_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->dataoff);
  swap_endian(&hdr->datasize);
}

template<>
void swap_endian<MachO::details::data_in_code_entry>(MachO::details::data_in_code_entry* hdr) {
  swap_endian(&hdr->offset);
  swap_endian(&hdr->length);
  swap_endian(&hdr->kind);
}

template<>
void swap_endian<MachO::details::source_version_command>(MachO::details::source_version_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->version);
}

template<>
void swap_endian<MachO::details::encryption_info_command>(MachO::details::encryption_info_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->cryptoff);
  swap_endian(&hdr->cryptsize);
  swap_endian(&hdr->cryptid);
}

template<>
void swap_endian<MachO::details::version_min_command>(MachO::details::version_min_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->version);
  swap_endian(&hdr->sdk);
}

template<>
void swap_endian<MachO::details::build_version_command>(MachO::details::build_version_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->platform);
  swap_endian(&hdr->minos);
  swap_endian(&hdr->sdk);
  swap_endian(&hdr->ntools);
}

template<>
void swap_endian<MachO::details::build_tool_version>(MachO::details::build_tool_version* hdr) {
  swap_endian(&hdr->tool);
  swap_endian(&hdr->version);
}

template<>
void swap_endian<MachO::details::dyld_info_command>(MachO::details::dyld_info_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->rebase_off);
  swap_endian(&hdr->rebase_size);
  swap_endian(&hdr->bind_off);
  swap_endian(&hdr->bind_size);
  swap_endian(&hdr->weak_bind_off);
  swap_endian(&hdr->weak_bind_size);
  swap_endian(&hdr->lazy_bind_off);
  swap_endian(&hdr->lazy_bind_size);
  swap_endian(&hdr->export_off);
  swap_endian(&hdr->export_size);
}

template<>
void swap_endian<MachO::details::linker_option_command>(MachO::details::linker_option_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->count);
}

template<>
void swap_endian<MachO::details::symseg_command>(MachO::details::symseg_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->offset);
  swap_endian(&hdr->size);
}

template<>
void swap_endian<MachO::details::ident_command>(MachO::details::ident_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
}

template<>
void swap_endian<MachO::details::fvmfile_command>(MachO::details::fvmfile_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->name);
  swap_endian(&hdr->header_addr);
}

template<>
void swap_endian<MachO::details::entry_point_command>(MachO::details::entry_point_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->entryoff);
  swap_endian(&hdr->stacksize);
}

template<>
void swap_endian<MachO::details::fileset_entry_command>(MachO::details::fileset_entry_command* hdr) {
  swap_endian(&hdr->cmd);
  swap_endian(&hdr->cmdsize);
  swap_endian(&hdr->vmaddr);
  swap_endian(&hdr->fileoff);
  swap_endian(&hdr->entry_id);
  swap_endian(&hdr->reserved);
}

template<>
void swap_endian<MachO::details::relocation_info>(MachO::details::relocation_info* hdr) {
  struct mirror_t {
    int32_t A;
    uint32_t B;
  };
  auto* mirror = reinterpret_cast<mirror_t*>(hdr);
  swap_endian(&mirror->A);
  swap_endian(&mirror->B);
}

template<>
void swap_endian<MachO::details::scattered_relocation_info>(MachO::details::scattered_relocation_info* hdr) {
  struct mirror_t {
    int32_t A;
    uint32_t B;
  };
  auto* mirror = reinterpret_cast<mirror_t*>(hdr);
  swap_endian(&mirror->A);
}

template <typename nlist>
void swap_endian_nlist(nlist* hdr) {
  swap_endian(&hdr->n_strx);
  swap_endian(&hdr->n_type);
  swap_endian(&hdr->n_sect);
  swap_endian(&hdr->n_desc);
  swap_endian(&hdr->n_value);
}

template<>
void swap_endian<MachO::details::nlist_32>(MachO::details::nlist_32* hdr) {
  return swap_endian_nlist(hdr);
}

template<>
void swap_endian<MachO::details::nlist_64>(MachO::details::nlist_64* hdr) {
  return swap_endian_nlist(hdr);
}

template<>
void swap_endian<MachO::details::x86_thread_state64_t>(MachO::details::x86_thread_state64_t* hdr) {
  swap_endian(&hdr->rax);
  swap_endian(&hdr->rbx);
  swap_endian(&hdr->rcx);
  swap_endian(&hdr->rdx);
  swap_endian(&hdr->rdi);
  swap_endian(&hdr->rsi);
  swap_endian(&hdr->rbp);
  swap_endian(&hdr->rsp);
  swap_endian(&hdr->r8);
  swap_endian(&hdr->r9);
  swap_endian(&hdr->r10);
  swap_endian(&hdr->r11);
  swap_endian(&hdr->r12);
  swap_endian(&hdr->r13);
  swap_endian(&hdr->r14);
  swap_endian(&hdr->r15);
  swap_endian(&hdr->rip);
  swap_endian(&hdr->rflags);
  swap_endian(&hdr->cs);
  swap_endian(&hdr->fs);
  swap_endian(&hdr->gs);
}

template<>
void swap_endian<MachO::details::x86_thread_state_t>(MachO::details::x86_thread_state_t* hdr) {
  swap_endian(&hdr->eax);
  swap_endian(&hdr->ebx);
  swap_endian(&hdr->ecx);
  swap_endian(&hdr->edx);
  swap_endian(&hdr->edi);
  swap_endian(&hdr->esi);
  swap_endian(&hdr->ebp);
  swap_endian(&hdr->esp);
  swap_endian(&hdr->ss);
  swap_endian(&hdr->eflags);
  swap_endian(&hdr->eip);
  swap_endian(&hdr->cs);
  swap_endian(&hdr->ds);
  swap_endian(&hdr->es);
  swap_endian(&hdr->fs);
  swap_endian(&hdr->gs);
}

template<>
void swap_endian<MachO::details::arm_thread_state_t>(MachO::details::arm_thread_state_t* hdr) {
  swap_endian(&hdr->r0);
  swap_endian(&hdr->r1);
  swap_endian(&hdr->r2);
  swap_endian(&hdr->r3);
  swap_endian(&hdr->r4);
  swap_endian(&hdr->r5);
  swap_endian(&hdr->r6);
  swap_endian(&hdr->r7);
  swap_endian(&hdr->r8);
  swap_endian(&hdr->r9);
  swap_endian(&hdr->r10);
  swap_endian(&hdr->r11);
  swap_endian(&hdr->r12);
  swap_endian(&hdr->r13);
  swap_endian(&hdr->r14);
  swap_endian(&hdr->r15);
  swap_endian(&hdr->r16);
}

template<>
void swap_endian<MachO::details::arm_thread_state64_t>(MachO::details::arm_thread_state64_t* hdr) {
  for (size_t i = 0; i < 29; ++i) {
    swap_endian(&hdr->x[i]);
  }
  swap_endian(&hdr->fp);
  swap_endian(&hdr->lr);
  swap_endian(&hdr->sp);
  swap_endian(&hdr->pc);
  swap_endian(&hdr->cpsr);
}

template<>
void swap_endian<MachO::details::ppc_thread_state64_t>(MachO::details::ppc_thread_state64_t* hdr) {
  swap_endian(&hdr->srr0);
  swap_endian(&hdr->srr1);
  for (size_t i = 0; i < 32; ++i) {
    swap_endian(&hdr->r[i]);
  }
  swap_endian(&hdr->cr);
  swap_endian(&hdr->xer);
  swap_endian(&hdr->lr);
  swap_endian(&hdr->ctr);
  swap_endian(&hdr->vrsave);
}

template<>
void swap_endian<MachO::details::ppc_thread_state_t>(MachO::details::ppc_thread_state_t* hdr) {
  swap_endian(&hdr->srr0);
  swap_endian(&hdr->srr1);
  for (size_t i = 0; i < 32; ++i) {
    swap_endian(&hdr->r[i]);
  }
  swap_endian(&hdr->cr);
  swap_endian(&hdr->xer);
  swap_endian(&hdr->lr);
  swap_endian(&hdr->ctr);
  swap_endian(&hdr->mq);
  swap_endian(&hdr->vrsave);
}
}
