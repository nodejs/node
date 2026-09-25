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
#ifndef LIEF_ELF_STRUCTURES_H
#define LIEF_ELF_STRUCTURES_H

#include <cstring>

#include "LIEF/types.hpp"
#include "LIEF/ELF/enums.hpp"

namespace LIEF {
namespace ELF {

namespace details {

#include "structures.inc"

struct Elf64_Prpsinfo
{
  char     pr_state;
  char     pr_sname;
  char     pr_zomb;
  char     pr_nice;
  uint32_t pr_pad;
  uint64_t pr_flag;
  uint32_t pr_uid;
  uint32_t pr_gid;
  int32_t  pr_pid;
  int32_t  pr_ppid;
  int32_t  pr_pgrp;
  int32_t  pr_sid;
  char     pr_fname[16];
  char     pr_psargs[80];
};

struct Elf32_Prpsinfo
{
  char     pr_state;
  char     pr_sname;
  char     pr_zomb;
  char     pr_nice;
  uint32_t pr_flag;
  uint16_t pr_uid;
  uint16_t pr_gid;
  int32_t  pr_pid;
  int32_t  pr_ppid;
  int32_t  pr_pgrp;
  int32_t  pr_sid;
  char     pr_fname[16];
  char     pr_psargs[80];
};

class ELF32 {
  public:
  static constexpr auto r_info_shift = 8;
  typedef Elf32_Addr    Elf_Addr;
  typedef Elf32_Off     Elf_Off;
  typedef Elf32_Half    Elf_Half;
  typedef Elf32_Word    Elf_Word;
  typedef Elf32_Sword   Elf_Sword;
  // Equivalent
  typedef Elf32_Addr    Elf_Xword;
  typedef Elf32_Sword   Elf_Sxword;

  typedef uint32_t      uint;

  typedef Elf32_Phdr    Elf_Phdr;
  typedef Elf32_Ehdr    Elf_Ehdr;
  typedef Elf32_Shdr    Elf_Shdr;
  typedef Elf32_Sym     Elf_Sym;
  typedef Elf32_Rel     Elf_Rel;
  typedef Elf32_Rela    Elf_Rela;
  typedef Elf32_Dyn     Elf_Dyn;
  typedef Elf32_Verneed Elf_Verneed;
  typedef Elf32_Vernaux Elf_Vernaux;
  typedef Elf32_Auxv    Elf_Auxv;
  typedef Elf32_Verdef  Elf_Verdef;
  typedef Elf32_Verdaux Elf_Verdaux;

  typedef Elf32_Prpsinfo  Elf_Prpsinfo;
  typedef Elf32_FileEntry Elf_FileEntry;
  typedef Elf32_Prstatus  Elf_Prstatus;

  typedef Elf32_timeval   Elf_timeval;
};


class ELF64 {
  public:
  static constexpr auto r_info_shift = 32;
  typedef Elf64_Addr    Elf_Addr;
  typedef Elf64_Off     Elf_Off;
  typedef Elf64_Half    Elf_Half;
  typedef Elf64_Word    Elf_Word;
  typedef Elf64_Sword   Elf_Sword;

  typedef Elf64_Xword   Elf_Xword;
  typedef Elf64_Sxword  Elf_Sxword;

  typedef uint64_t      uint;

  typedef Elf64_Phdr    Elf_Phdr;
  typedef Elf64_Ehdr    Elf_Ehdr;
  typedef Elf64_Shdr    Elf_Shdr;
  typedef Elf64_Sym     Elf_Sym;
  typedef Elf64_Rel     Elf_Rel;
  typedef Elf64_Rela    Elf_Rela;
  typedef Elf64_Dyn     Elf_Dyn;
  typedef Elf64_Verneed Elf_Verneed;
  typedef Elf64_Vernaux Elf_Vernaux;
  typedef Elf64_Auxv    Elf_Auxv;
  typedef Elf64_Verdef  Elf_Verdef;
  typedef Elf64_Verdaux Elf_Verdaux;

  typedef Elf64_Prpsinfo  Elf_Prpsinfo;
  typedef Elf64_FileEntry Elf_FileEntry;
  typedef Elf64_Prstatus  Elf_Prstatus;

  typedef Elf64_timeval   Elf_timeval;
};

class ELF32_x32 : public ELF32 {
};

class ELF32_arm64 : public ELF32 {
};


} /* end namespace details */
} /* end namespace ELF */
} /* end namespace LIEF */
#endif
