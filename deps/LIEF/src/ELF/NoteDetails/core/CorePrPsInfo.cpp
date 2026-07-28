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

#include "LIEF/ELF/hash.hpp"
#include "ELF/Structures.hpp"

#include "LIEF/ELF/NoteDetails/core/CorePrPsInfo.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/iostream.hpp"

#include "spdlog/fmt/fmt.h"

namespace LIEF {
namespace ELF {

template<class ELF_T>
inline result<CorePrPsInfo::info_t>
get_info_impl(const Note::description_t& description) {
  using Elf_Prpsinfo = typename ELF_T::Elf_Prpsinfo;
  CorePrPsInfo::info_t info;
  auto stream = SpanStream::from_vector(description);
  if (!stream) {
    return make_error_code(get_error(stream));
  }
  auto raw = stream->read<Elf_Prpsinfo>();
  if (!raw) {
    return make_error_code(get_error(raw));
  }

  info.state    = raw->pr_state;
  info.sname    = raw->pr_sname;
  info.zombie   = static_cast<bool>(raw->pr_zomb);
  info.nice     = raw->pr_nice;
  info.flag     = raw->pr_flag;
  info.uid      = raw->pr_uid;
  info.gid      = raw->pr_gid;
  info.pid      = raw->pr_pid;
  info.ppid     = raw->pr_ppid;
  info.pgrp     = raw->pr_pgrp;
  info.sid      = raw->pr_sid;
  info.filename = std::string(raw->pr_fname, sizeof(Elf_Prpsinfo::pr_fname));
  info.args     = std::string(raw->pr_psargs, sizeof(Elf_Prpsinfo::pr_psargs));

  return info;
}

template<class ELF_T> inline ok_error_t
write_info_impl(Note::description_t& description,
                const CorePrPsInfo::info_t& info)
{
  using Elf_Prpsinfo = typename ELF_T::Elf_Prpsinfo;
  Elf_Prpsinfo raw;
  std::memset(reinterpret_cast<void*>(&raw), 0, sizeof(Elf_Prpsinfo));


  raw.pr_state = info.state;
  raw.pr_sname = info.sname;
  raw.pr_zomb = static_cast<char>(info.zombie);
  raw.pr_nice = info.nice;
  raw.pr_flag = info.flag;
  raw.pr_uid = info.uid;
  raw.pr_gid = info.gid;
  raw.pr_pid = info.pid;
  raw.pr_ppid = info.ppid;
  raw.pr_pgrp = info.pgrp;
  raw.pr_sid = info.sid;

  std::string filename = info.filename;
  filename.resize(sizeof(Elf_Prpsinfo::pr_fname));

  std::string args = info.args;
  args.resize(sizeof(Elf_Prpsinfo::pr_psargs));

  std::move(filename.begin(), filename.end(), raw.pr_fname);
  std::move(args.begin(), args.end(), raw.pr_psargs);

  vector_iostream ios;
  ios.write(raw);
  ios.move(description);
  return ok_t();
}


result<CorePrPsInfo::info_t> CorePrPsInfo::info() const {
  return class_ == Header::CLASS::ELF32 ?
                   get_info_impl<details::ELF32>(description_) :
                   get_info_impl<details::ELF64>(description_);
}


void CorePrPsInfo::info(const info_t& info) {
  class_ == Header::CLASS::ELF32 ?
            write_info_impl<details::ELF32>(description_, info) :
            write_info_impl<details::ELF64>(description_, info);
}

void CorePrPsInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void CorePrPsInfo::dump(std::ostream& os) const {
  Note::dump(os);
  auto info_res = info();
  if (!info_res) {
    return;
  }
  os << '\n';
  os << fmt::format("  Path: {} (args: {})\n",
                    info_res->filename_stripped(), info_res->args_stripped())
     << fmt::format("  UID: {:04d} GID: {:04d} PID: {:04d}\n",
                    info_res->uid, info_res->gid, info_res->pid)
     << fmt::format("  PPID: {:04d} PGRP: {:04d} SID: {:04d}\n",
                    info_res->ppid, info_res->pgrp, info_res->sid)
     << fmt::format("  Flag: 0x{:04x} Nice: {} Zombie: {}\n",
                    info_res->flag, info_res->nice, info_res->zombie)
     << fmt::format("  State: 0x{:x} State Name: {}\n",
                    info_res->state, info_res->sname);
}

} // namespace ELF
} // namespace LIEF
