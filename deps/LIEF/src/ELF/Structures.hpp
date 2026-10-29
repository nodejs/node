/* Copyright 2021 - 2026 R. Thomas
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
#ifndef LIEF_ELF_STRUCTURES_H
#define LIEF_ELF_STRUCTURES_H
#include <cstdint>

namespace LIEF::ELF::details {

#include "structures.inc"

struct Elf64_Prpsinfo {
  char pr_state;
  char pr_sname;
  char pr_zomb;
  char pr_nice;
  uint32_t pr_pad;
  uint64_t pr_flag;
  uint32_t pr_uid;
  uint32_t pr_gid;
  int32_t pr_pid;
  int32_t pr_ppid;
  int32_t pr_pgrp;
  int32_t pr_sid;
  char pr_fname[16];
  char pr_psargs[80];
};

struct Elf32_Prpsinfo {
  char pr_state;
  char pr_sname;
  char pr_zomb;
  char pr_nice;
  uint32_t pr_flag;
  uint16_t pr_uid;
  uint16_t pr_gid;
  int32_t pr_pid;
  int32_t pr_ppid;
  int32_t pr_pgrp;
  int32_t pr_sid;
  char pr_fname[16];
  char pr_psargs[80];
};

class ELF32 {
  public:
  static constexpr auto r_info_shift = 8;
  using Elf_Addr = Elf32_Addr;
  using Elf_Off = Elf32_Off;
  using Elf_Half = Elf32_Half;
  using Elf_Word = Elf32_Word;
  using Elf_Sword = Elf32_Sword;
  // Equivalent
  using Elf_Xword = Elf32_Addr;
  using Elf_Sxword = Elf32_Sword;

  using uint = uint32_t;

  using Elf_Phdr = Elf32_Phdr;
  using Elf_Ehdr = Elf32_Ehdr;
  using Elf_Shdr = Elf32_Shdr;
  using Elf_Sym = Elf32_Sym;
  using Elf_Rel = Elf32_Rel;
  using Elf_Rela = Elf32_Rela;
  using Elf_Dyn = Elf32_Dyn;
  using Elf_Verneed = Elf32_Verneed;
  using Elf_Vernaux = Elf32_Vernaux;
  using Elf_Auxv = Elf32_Auxv;
  using Elf_Verdef = Elf32_Verdef;
  using Elf_Verdaux = Elf32_Verdaux;

  using Elf_Prpsinfo = Elf32_Prpsinfo;
  using Elf_FileEntry = Elf32_FileEntry;
  using Elf_Prstatus = Elf32_Prstatus;

  using Elf_timeval = Elf32_timeval;
};


class ELF64 {
  public:
  static constexpr auto r_info_shift = 32;
  using Elf_Addr = Elf64_Addr;
  using Elf_Off = Elf64_Off;
  using Elf_Half = Elf64_Half;
  using Elf_Word = Elf64_Word;
  using Elf_Sword = Elf64_Sword;

  using Elf_Xword = Elf64_Xword;
  using Elf_Sxword = Elf64_Sxword;

  using uint = uint64_t;

  using Elf_Phdr = Elf64_Phdr;
  using Elf_Ehdr = Elf64_Ehdr;
  using Elf_Shdr = Elf64_Shdr;
  using Elf_Sym = Elf64_Sym;
  using Elf_Rel = Elf64_Rel;
  using Elf_Rela = Elf64_Rela;
  using Elf_Dyn = Elf64_Dyn;
  using Elf_Verneed = Elf64_Verneed;
  using Elf_Vernaux = Elf64_Vernaux;
  using Elf_Auxv = Elf64_Auxv;
  using Elf_Verdef = Elf64_Verdef;
  using Elf_Verdaux = Elf64_Verdaux;

  using Elf_Prpsinfo = Elf64_Prpsinfo;
  using Elf_FileEntry = Elf64_FileEntry;
  using Elf_Prstatus = Elf64_Prstatus;

  using Elf_timeval = Elf64_timeval;
};

class ELF32_x32 : public ELF32 {};

class ELF32_arm64 : public ELF32 {};


} // namespace LIEF::ELF::details


#endif
