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
#include "LIEF/endianness_support.hpp"
#include "ELF/Structures.hpp"

namespace LIEF {

template <typename Elf_Ehdr>
void swap_endian_ehdr(Elf_Ehdr* hdr) {
  swap_endian(&hdr->e_type);
  swap_endian(&hdr->e_machine);
  swap_endian(&hdr->e_version);
  swap_endian(&hdr->e_entry);
  swap_endian(&hdr->e_phoff);
  swap_endian(&hdr->e_shoff);
  swap_endian(&hdr->e_flags);
  swap_endian(&hdr->e_ehsize);
  swap_endian(&hdr->e_phentsize);
  swap_endian(&hdr->e_phnum);
  swap_endian(&hdr->e_shentsize);
  swap_endian(&hdr->e_shnum);
  swap_endian(&hdr->e_shstrndx);
}

template<>
void swap_endian<ELF::details::Elf32_Ehdr>(ELF::details::Elf32_Ehdr* hdr) {
  swap_endian_ehdr(hdr);
}

template<>
void swap_endian<ELF::details::Elf64_Ehdr>(ELF::details::Elf64_Ehdr *hdr) {
  swap_endian_ehdr(hdr);
}

template <typename Elf_Shdr>
void swap_endian_shdr(Elf_Shdr *shdr) {
  swap_endian(&shdr->sh_name);
  swap_endian(&shdr->sh_type);
  swap_endian(&shdr->sh_flags);
  swap_endian(&shdr->sh_addr);
  swap_endian(&shdr->sh_offset);
  swap_endian(&shdr->sh_size);
  swap_endian(&shdr->sh_link);
  swap_endian(&shdr->sh_info);
  swap_endian(&shdr->sh_addralign);
  swap_endian(&shdr->sh_entsize);
}

template<>
void swap_endian<ELF::details::Elf32_Shdr>(ELF::details::Elf32_Shdr *shdr) {
  swap_endian_shdr(shdr);
}

template<>
void swap_endian<ELF::details::Elf64_Shdr>(ELF::details::Elf64_Shdr *shdr) {
  swap_endian_shdr(shdr);
}

template <typename Elf_Phdr>
void swap_endian_phdr(Elf_Phdr *phdr) {
  swap_endian(&phdr->p_type);
  swap_endian(&phdr->p_offset);
  swap_endian(&phdr->p_vaddr);
  swap_endian(&phdr->p_paddr);
  swap_endian(&phdr->p_filesz);
  swap_endian(&phdr->p_memsz);
  swap_endian(&phdr->p_flags);
  swap_endian(&phdr->p_align);
}

template<>
void swap_endian<ELF::details::Elf32_Phdr>(ELF::details::Elf32_Phdr *phdr) {
  swap_endian_phdr(phdr);
}

template<>
void swap_endian<ELF::details::Elf64_Phdr>(ELF::details::Elf64_Phdr *phdr) {
  swap_endian_phdr(phdr);
}


template <typename Elf_Sym>
void swap_endian_sym(Elf_Sym *sym) {
  swap_endian(&sym->st_name);
  swap_endian(&sym->st_value);
  swap_endian(&sym->st_size);
  swap_endian(&sym->st_info);
  swap_endian(&sym->st_other);
  swap_endian(&sym->st_shndx);
}

template<>
void swap_endian<ELF::details::Elf32_Sym>(ELF::details::Elf32_Sym *sym) {
  swap_endian_sym(sym);
}

template<>
void swap_endian<ELF::details::Elf64_Sym>(ELF::details::Elf64_Sym *sym) {
  swap_endian_sym(sym);
}

template <typename REL_T>
void swap_endian_rel(REL_T *rel) {
  swap_endian(&rel->r_offset);
  swap_endian(&rel->r_info);
}

template <typename RELA_T>
void swap_endian_rela(RELA_T *rel) {
  swap_endian(&rel->r_offset);
  swap_endian(&rel->r_info);
  swap_endian(&rel->r_addend);
}

template<>
void swap_endian<ELF::details::Elf32_Rel>(ELF::details::Elf32_Rel *rel) {
  swap_endian_rel(rel);
}

template<>
void swap_endian<ELF::details::Elf64_Rel>(ELF::details::Elf64_Rel *rel) {
  swap_endian_rel(rel);
}

template<>
void swap_endian<ELF::details::Elf32_Rela>(ELF::details::Elf32_Rela *rel) {
  swap_endian_rela(rel);
}

template<>
void swap_endian<ELF::details::Elf64_Rela>(ELF::details::Elf64_Rela *rel) {
  swap_endian_rela(rel);
}


/** ELF Dynamic Symbol */
template <typename Elf_Dyn>
void swap_endian_dyn(Elf_Dyn *dyn) {
  swap_endian(&dyn->d_tag);
  swap_endian(&dyn->d_un.d_val);
}

template<>
void swap_endian<ELF::details::Elf32_Dyn>(ELF::details::Elf32_Dyn *dyn) {
  swap_endian_dyn(dyn);
}

template<>
void swap_endian<ELF::details::Elf64_Dyn>(ELF::details::Elf64_Dyn *dyn) {
  swap_endian_dyn(dyn);
}


/** ELF Verneed */
template <typename Elf_Verneed>
void swap_endian_verneed(Elf_Verneed *ver) {
  swap_endian(&ver->vn_version);
  swap_endian(&ver->vn_cnt);
  swap_endian(&ver->vn_file);
  swap_endian(&ver->vn_aux);
  swap_endian(&ver->vn_next);
}

template<>
void swap_endian<ELF::details::Elf32_Verneed>(ELF::details::Elf32_Verneed *ver) {
  swap_endian_verneed(ver);
}

template<>
void swap_endian<ELF::details::Elf64_Verneed>(ELF::details::Elf64_Verneed *ver) {
  swap_endian_verneed(ver);
}


/** ELF Vernaux */
template <typename Elf_Vernaux>
void swap_endian_vernaux(Elf_Vernaux *ver) {
  swap_endian(&ver->vna_hash);
  swap_endian(&ver->vna_flags);
  swap_endian(&ver->vna_other);
  swap_endian(&ver->vna_name);
  swap_endian(&ver->vna_next);
}

template<>
void swap_endian<ELF::details::Elf32_Vernaux>(ELF::details::Elf32_Vernaux *ver) {
  swap_endian_vernaux(ver);
}

template<>
void swap_endian<ELF::details::Elf64_Vernaux>(ELF::details::Elf64_Vernaux *ver) {
  swap_endian_vernaux(ver);
}

/** ELF Symbol Version Definition */
template <typename Elf_Verdef>
void swap_endian_verdef(Elf_Verdef *ver) {
  swap_endian(&ver->vd_version);
  swap_endian(&ver->vd_flags);
  swap_endian(&ver->vd_ndx);
  swap_endian(&ver->vd_cnt);
  swap_endian(&ver->vd_hash);
  swap_endian(&ver->vd_aux);
  swap_endian(&ver->vd_next);
}

template<>
void swap_endian<ELF::details::Elf32_Verdef>(ELF::details::Elf32_Verdef *ver) {
  swap_endian_verdef(ver);
}

template<>
void swap_endian<ELF::details::Elf64_Verdef>(ELF::details::Elf64_Verdef *ver) {
  swap_endian_verdef(ver);
}

template <typename Elf_Verdaux>
void swap_endian_verdaux(Elf_Verdaux *ver) {
  swap_endian(&ver->vda_name);
  swap_endian(&ver->vda_next);
}

template<>
void swap_endian<ELF::details::Elf32_Verdaux>(ELF::details::Elf32_Verdaux *ver) {
  swap_endian_verdaux(ver);
}

template<>
void swap_endian<ELF::details::Elf64_Verdaux>(ELF::details::Elf64_Verdaux *ver) {
  swap_endian_verdaux(ver);
}

template <typename Elf_Auxv>
void swap_endian_auxv(Elf_Auxv *auxv) {
  swap_endian(&auxv->a_type);
  swap_endian(&auxv->a_un.a_val);
}

template<>
void swap_endian<ELF::details::Elf32_Auxv>(ELF::details::Elf32_Auxv *auxv) {
  swap_endian_auxv(auxv);
}

template<>
void swap_endian<ELF::details::Elf64_Auxv>(ELF::details::Elf64_Auxv *auxv) {
  swap_endian_auxv(auxv);
}

template <typename Elf_FileEntry>
void swap_endian_fileentry(Elf_FileEntry *entry) {
  swap_endian(&entry->start);
  swap_endian(&entry->end);
  swap_endian(&entry->file_ofs);
}

template<>
void swap_endian<ELF::details::Elf32_FileEntry>(ELF::details::Elf32_FileEntry *entry) {
  swap_endian_fileentry(entry);
}

template<>
void swap_endian<ELF::details::Elf64_FileEntry>(ELF::details::Elf64_FileEntry *entry) {
  swap_endian_fileentry(entry);
}

}
