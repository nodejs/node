//===------------- Linux VDSO Implementation --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "src/__support/OSUtil/linux/vdso.h"
#include "hdr/elf_proxy.h"
#include "hdr/link_macros.h"
#include "hdr/sys_auxv_macros.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/linux/auxv.h"
#include "src/__support/threads/callonce.h"
#include "src/__support/threads/linux/futex_word.h"

// TODO: This is a temporary workaround to avoid including elf.h
// Include our own headers for ElfW and friends once we have them.
namespace LIBC_NAMESPACE_DECL {

namespace vdso {

Symbol::VDSOArray Symbol::global_cache{};
CallOnceFlag Symbol::once_flag = callonce_impl::NOT_CALLED;

namespace {

// version search procedure specified by
// https://refspecs.linuxfoundation.org/LSB_1.3.0/gLSB/gLSB/symversion.html#SYMVERTBL
cpp::string_view find_version(ElfW(Verdef) * verdef, ElfW(Half) * versym,
                              const char *strtab, size_t idx) {
#ifndef VER_FLG_BASE
  constexpr ElfW(Half) VER_FLG_BASE = 0x1;
#endif
  if (!versym)
    return "";
  ElfW(Half) identifier = versym[idx] & 0x7FFF;
  // iterate through all version definitions

  for (ElfW(Verdef) *def = verdef; def != nullptr;
       def = reinterpret_cast<ElfW(Verdef) *>(reinterpret_cast<uintptr_t>(def) +
                                              def->vd_next)) {
    // skip if this is a file-level version
    if (def->vd_flags & VER_FLG_BASE)
      continue;
    // check if the version identifier matches. Highest bit is used to determine
    // whether the symbol is local. Only lower 15 bits are used for version
    // identifier.
    if ((def->vd_ndx & 0x7FFF) == identifier) {
      ElfW(Verdaux) *aux = reinterpret_cast<ElfW(Verdaux) *>(
          reinterpret_cast<uintptr_t>(def) + def->vd_aux);
      return strtab + aux->vda_name;
    }
  }
  return "";
}

size_t shdr_get_symbol_count(ElfW(Shdr) * vdso_shdr, size_t e_shnum) {
  if (!vdso_shdr)
    return 0;
  // iterate all sections until we locate the dynamic symbol section
  for (size_t i = 0; i < e_shnum; ++i) {
    // dynamic symbol section is a table section
    // therefore, the number of entries can be computed as the ratio
    // of the section size to the size of a single entry
    if (vdso_shdr[i].sh_type == SHT_DYNSYM)
      return vdso_shdr[i].sh_size / vdso_shdr[i].sh_entsize;
  }
  return 0;
}

struct VDSOSymbolTable {
  const char *strtab;
  ElfW(Sym) * symtab;
  // The following can be nullptr if the vDSO does not have versioning
  ElfW(Versym) * versym;
  ElfW(Verdef) * verdef;

  void populate_symbol_cache(Symbol::VDSOArray &symbol_table,
                             size_t symbol_count, ElfW(Addr) vdso_addr) {
    for (size_t i = 0, e = symbol_table.size(); i < e; ++i) {
      Symbol sym = i;
      cpp::string_view name = sym.name();
      cpp::string_view version = sym.version();
      if (name.empty())
        continue;

      for (size_t j = 0; j < symbol_count; ++j) {
        if (name == strtab + symtab[j].st_name) {
          // we find a symbol with desired name
          // now we need to check if it has the right version
          if (versym && verdef &&
              version != find_version(verdef, versym, strtab, j))
            continue;

          // put the symbol address into the symbol table
          symbol_table[i] =
              reinterpret_cast<void *>(vdso_addr + symtab[j].st_value);
        }
      }
    }
  }
};

struct PhdrInfo {
  ElfW(Addr) vdso_addr;
  ElfW(Dyn) * vdso_dyn;
  static cpp::optional<PhdrInfo> from(ElfW(Phdr) * vdso_phdr, size_t e_phnum,
                                      uintptr_t vdso_ehdr_addr) {
    constexpr ElfW(Addr) INVALID_ADDR = static_cast<ElfW(Addr)>(-1);
    ElfW(Addr) vdso_addr = INVALID_ADDR;
    ElfW(Dyn) *vdso_dyn = nullptr;
    if (!vdso_phdr)
      return cpp::nullopt;
    // iterate through all the program headers until we get the desired pieces
    for (size_t i = 0; i < e_phnum; ++i) {
      if (vdso_phdr[i].p_type == PT_DYNAMIC)
        vdso_dyn = reinterpret_cast<ElfW(Dyn) *>(vdso_ehdr_addr +
                                                 vdso_phdr[i].p_offset);

      if (vdso_phdr[i].p_type == PT_LOAD)
        vdso_addr =
            vdso_ehdr_addr + vdso_phdr[i].p_offset - vdso_phdr[i].p_vaddr;

      if (vdso_addr && vdso_dyn)
        return PhdrInfo{vdso_addr, vdso_dyn};
    }

    return cpp::nullopt;
  }

  cpp::optional<VDSOSymbolTable> populate_symbol_table() {
    const char *strtab = nullptr;
    ElfW(Sym) *symtab = nullptr;
    ElfW(Versym) *versym = nullptr;
    ElfW(Verdef) *verdef = nullptr;
    for (ElfW(Dyn) *d = vdso_dyn; d->d_tag != DT_NULL; ++d) {
      switch (d->d_tag) {
      case DT_STRTAB:
        strtab = reinterpret_cast<const char *>(vdso_addr + d->d_un.d_ptr);
        break;
      case DT_SYMTAB:
        symtab = reinterpret_cast<ElfW(Sym) *>(vdso_addr + d->d_un.d_ptr);
        break;
      case DT_VERSYM:
        versym = reinterpret_cast<uint16_t *>(vdso_addr + d->d_un.d_ptr);
        break;
      case DT_VERDEF:
        verdef = reinterpret_cast<ElfW(Verdef) *>(vdso_addr + d->d_un.d_ptr);
        break;
      }
      if (strtab && symtab && versym && verdef)
        break;
    }
    if (strtab == nullptr || symtab == nullptr)
      return cpp::nullopt;

    return VDSOSymbolTable{strtab, symtab, versym, verdef};
  }
};
} // namespace

void Symbol::initialize_vdso_global_cache() {
  // first clear the symbol table
  for (auto &i : global_cache)
    i = nullptr;

  cpp::optional<unsigned long> auxv_res = auxv::get(AT_SYSINFO_EHDR);
  uintptr_t vdso_ehdr_addr = auxv_res ? static_cast<uintptr_t>(*auxv_res) : 0;
  // Get the memory address of the vDSO ELF header.
  auto vdso_ehdr = reinterpret_cast<ElfW(Ehdr) *>(vdso_ehdr_addr);
  // leave the table unpopulated if we don't have vDSO
  if (vdso_ehdr == nullptr)
    return;

  // locate the section header inside the elf using the section header
  // offset
  auto vdso_shdr =
      reinterpret_cast<ElfW(Shdr) *>(vdso_ehdr_addr + vdso_ehdr->e_shoff);
  size_t symbol_count = shdr_get_symbol_count(vdso_shdr, vdso_ehdr->e_shnum);

  // early return if no symbol is found
  if (symbol_count == 0)
    return;

  // We need to find both the loadable segment and the dynamic linking of
  // the vDSO. compute vdso_phdr as the program header using the program
  // header offset
  ElfW(Phdr) *vdso_phdr =
      reinterpret_cast<ElfW(Phdr) *>(vdso_ehdr_addr + vdso_ehdr->e_phoff);
  cpp::optional<PhdrInfo> phdr_info =
      PhdrInfo::from(vdso_phdr, vdso_ehdr->e_phnum, vdso_ehdr_addr);
  // early return if either the dynamic linking or the loadable segment is
  // not found
  if (!phdr_info.has_value())
    return;

  // now, locate several more tables inside the dynmaic linking section
  cpp::optional<VDSOSymbolTable> vdso_symbol_table =
      phdr_info->populate_symbol_table();

  // early return if we can't find any required fields of the symbol table
  if (!vdso_symbol_table.has_value())
    return;

  // finally, populate the global symbol table cache
  vdso_symbol_table->populate_symbol_cache(global_cache, symbol_count,
                                           phdr_info->vdso_addr);
}
} // namespace vdso
} // namespace LIBC_NAMESPACE_DECL
