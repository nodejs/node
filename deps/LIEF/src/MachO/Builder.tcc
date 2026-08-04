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
#include "logging.hpp"
#include "LIEF/utils.hpp"

#include "LIEF/MachO/AtomInfo.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/BuildVersion.hpp"
#include "LIEF/MachO/Builder.hpp"
#include "LIEF/MachO/ChainedBindingInfo.hpp"
#include "LIEF/MachO/CodeSignature.hpp"
#include "LIEF/MachO/CodeSignatureDir.hpp"
#include "LIEF/MachO/DataInCode.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/DyldEnvironment.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DylibCommand.hpp"
#include "LIEF/MachO/DylinkerCommand.hpp"
#include "LIEF/MachO/DynamicSymbolCommand.hpp"
#include "LIEF/MachO/EnumToString.hpp"
#include "LIEF/MachO/FunctionStarts.hpp"
#include "LIEF/MachO/FunctionVariants.hpp"
#include "LIEF/MachO/FunctionVariantFixups.hpp"
#include "LIEF/MachO/LinkEdit.hpp"
#include "LIEF/MachO/LinkerOptHint.hpp"
#include "LIEF/MachO/MainCommand.hpp"
#include "LIEF/MachO/NoteCommand.hpp"
#include "LIEF/MachO/Routine.hpp"
#include "LIEF/MachO/RPathCommand.hpp"
#include "LIEF/MachO/RelocationFixup.hpp"
#include "LIEF/MachO/Section.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/SegmentSplitInfo.hpp"
#include "LIEF/MachO/SourceVersion.hpp"
#include "LIEF/MachO/SubFramework.hpp"
#include "LIEF/MachO/SubClient.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/SymbolCommand.hpp"
#include "LIEF/MachO/ThreadCommand.hpp"
#include "LIEF/MachO/EncryptionInfo.hpp"
#include "LIEF/MachO/TwoLevelHints.hpp"
#include "LIEF/MachO/VersionMin.hpp"

#include "MachO/Structures.hpp"
#include "MachO/exports_trie.hpp"
#include "MachO/ChainedFixup.hpp"
#include "MachO/ChainedBindingInfoList.hpp"

#include "internal_utils.hpp"

namespace LIEF {
namespace MachO {


template<class T>
size_t Builder::get_cmd_size(const LoadCommand& cmd) {
  if (const auto* dylib = cmd.cast<DylibCommand>()) {
    return align(sizeof(details::dylib_command) + dylib->name().size() + 1,
                 sizeof(typename T::uint));
  }

  if (const auto* linker = cmd.cast<DylinkerCommand>()) {
    return align(sizeof(details::dylinker_command) + linker->name().size() + 1,
                 sizeof(typename T::uint));
  }

  if (const auto* rpath = cmd.cast<RPathCommand>()) {
    return align(sizeof(details::rpath_command) + rpath->path().size() + 1,
                 sizeof(typename T::uint));
  }

  if (const auto* subframework = cmd.cast<SubFramework>()) {
    return align(sizeof(details::sub_framework_command) + subframework->umbrella().size() + 1,
                 sizeof(typename T::uint));
  }

  if (const auto* subclient = cmd.cast<SubClient>()) {
    return align(sizeof(details::sub_client_command) + subclient->client().size() + 1,
                 sizeof(typename T::uint));
  }

  if (const auto* dyldenv = cmd.cast<DyldEnvironment>()) {
    return align(sizeof(details::dylinker_command) + dyldenv->value().size() + 1,
                 sizeof(typename T::uint));
  }

  if (const auto* bversion = cmd.cast<BuildVersion>()) {
    return align(sizeof(details::build_version_command) +
                 bversion->tools().size() * sizeof(details::build_tool_version),
                 sizeof(typename T::uint));
  }
  return cmd.size();
}

template<typename T>
ok_error_t Builder::build_linkedit() {
  // NOTE(romain): the order in which the linkedit_data_command are placed
  // in the __LINKEDIT segment, needs to follow cctools / checkout.c / dyld_order()
  SegmentCommand* linkedit = binary_->get_segment("__LINKEDIT");
  if (linkedit == nullptr) {
    return ok();
  }
  linkedit_offset_ = linkedit->file_offset();
  if (auto* dyld = binary_->dyld_info()) {
    build<T>(*dyld);
  }
  if (auto* fixups = binary_->dyld_chained_fixups()) {
    build<T>(*fixups);
  }
  if (auto* exports_trie = binary_->dyld_exports_trie()) {
    build<T>(*exports_trie);
  }
  if (auto* func_variants = binary_->function_variants()) {
    build<T>(*func_variants);
  }
  if (auto* func_variant_fixups = binary_->function_variant_fixups()) {
    build<T>(*func_variant_fixups);
  }
  if (auto* split_info = binary_->segment_split_info()) {
    build<T>(*split_info);
  }
  if (auto* fstart = binary_->function_starts()) {
    build<T>(*fstart);
  }
  if (auto* data = binary_->data_in_code()) {
    build<T>(*data);
  }
  if (auto* atom_info = binary_->atom_info()) {
    build<T>(*atom_info);
  }
  if (auto* sig_dir = binary_->code_signature_dir()) {
    build<T>(*sig_dir);
  }
  if (auto* opt = binary_->linker_opt_hint()) {
    build<T>(*opt);
  }
  if (auto* sym = binary_->symbol_command()) {
    build<T>(*sym);
  }
  if (auto* dynsym = binary_->dynamic_symbol_command()) {
    build<T>(*dynsym);
  }
  if (auto* code_signature = binary_->code_signature()) {
    build<T>(*code_signature);
  }

  const uint64_t original_size = linkedit->file_size();
  const uint64_t new_size      = linkedit_.size();
  if (original_size < new_size) {
    LIEF_INFO("Delta __LINKEDIT data size: +{}", new_size - original_size);
  }
  else if (original_size > new_size) {
    LIEF_INFO("Delta __LINKEDIT data size: -{}", original_size - new_size);
  }
  else {
    LIEF_INFO("__LINKEDIT data built with the same size: {}", new_size);
  }
  return ok();
}

template<typename T>
ok_error_t Builder::build_segments() {
  using section_t  = typename T::section;
  using segment_t  = typename T::segment_command;
  using uint__     = typename T::uint;

  LIEF_DEBUG("[+] Rebuilding segments");
  Binary* binary = binaries_.back();
  for (SegmentCommand& segment : binary->segments()) {
    LIEF_DEBUG("{}", segment.name());
    segment_t segment_header;
    std::memset(&segment_header, 0, sizeof(segment_header));

    segment_header.cmd      = static_cast<uint32_t>(segment.command());
    segment_header.cmdsize  = static_cast<uint32_t>(segment.size());

    const std::string& seg_name = segment.name();
    const uint32_t segname_length = std::min<uint32_t>(seg_name.size() + 1,
                                                       sizeof(segment_header.segname));
    std::copy(seg_name.c_str(), seg_name.c_str() + segname_length,
              std::begin(segment_header.segname));
    if (LinkEdit::segmentof(segment) && config_.linkedit) {
      segment_header.vmsize   = static_cast<uint__>(align(linkedit_.size(), binary->page_size()));
      segment_header.filesize = static_cast<uint__>(linkedit_.size());
    } else {
      segment_header.vmsize = static_cast<uint__>(segment.virtual_size());
      segment_header.filesize = static_cast<uint__>(segment.file_size());
    }
    segment_header.vmaddr   = static_cast<uint__>(segment.virtual_address());
    segment_header.fileoff  = static_cast<uint__>(segment.file_offset());
    segment_header.maxprot  = static_cast<uint32_t>(segment.max_protection());
    segment_header.initprot = static_cast<uint32_t>(segment.init_protection());
    segment_header.nsects   = static_cast<uint32_t>(segment.numberof_sections());
    segment_header.flags    = static_cast<uint32_t>(segment.flags());
    LIEF_DEBUG("  - Command offset: 0x{:x}", segment.command_offset());

    span<const uint8_t> content = segment.content();
    if (content.size() != segment.file_size() && !LinkEdit::segmentof(segment)) {
      LIEF_ERR("{} content size and file_size are differents: 0x{:x} vs 0x{:x}",
               segment.name(), content.size(), segment.file_size());

      return make_error_code(lief_errors::build_error);
    }

    segment.original_data_.clear();
    std::move(reinterpret_cast<uint8_t*>(&segment_header),
              reinterpret_cast<uint8_t*>(&segment_header) + sizeof(segment_t),
              std::back_inserter(segment.original_data_));

    // --------
    // Sections
    // --------
    if (segment.sections().size() != segment.numberof_sections()) {
      LIEF_ERR("segment.sections().size() != segment.numberof_sections()");
      return make_error_code(lief_errors::build_error);
    }

    SegmentCommand::it_sections sections = segment.sections();
    for (uint32_t i = 0; i < segment.numberof_sections(); ++i) {
      const Section& section = sections[i];
      const std::string& sec_name = section.name();
      const std::string& segment_name = segment.name();
      LIEF_DEBUG("{}", to_string(section));
      section_t header;
      std::memset(&header, 0, sizeof(header));

      const auto segname_length = std::min<uint32_t>(segment_name.size() + 1, sizeof(header.segname));
      std::copy(segment_name.c_str(), segment_name.c_str() + segname_length,
                std::begin(header.segname));

      const auto secname_length = std::min<uint32_t>(sec_name.size() + 1, sizeof(header.sectname));
      std::copy(sec_name.c_str(), sec_name.c_str() + secname_length,
                std::begin(header.sectname));

      header.addr      = static_cast<uint__>(section.address());
      header.size      = static_cast<uint__>(section.size());
      header.offset    = static_cast<uint32_t>(section.offset());
      header.align     = static_cast<uint32_t>(section.alignment());
      header.reloff    = static_cast<uint32_t>(section.relocation_offset());
      header.nreloc    = static_cast<uint32_t>(section.numberof_relocations());
      header.flags     = static_cast<uint32_t>(section.raw_flags());
      header.reserved1 = static_cast<uint32_t>(section.reserved1());
      header.reserved2 = static_cast<uint32_t>(section.reserved2());

      if constexpr (std::is_same_v<section_t, details::section_64>) {
        reinterpret_cast<details::section_64*>(&header)->reserved3 = static_cast<uint32_t>(section.reserved3());
      }

      std::move(reinterpret_cast<uint8_t*>(&header),
                reinterpret_cast<uint8_t*>(&header) + sizeof(section_t),
                std::back_inserter(segment.original_data_));

    }
  }
  return ok();
} // build_segment


template<typename T>
ok_error_t Builder::build(DylibCommand& library) {
  LIEF_DEBUG("Build Dylib '{}'", library.name());

  const uint32_t original_size = library.original_data_.size();

  const uint32_t raw_size = sizeof(details::dylib_command) + library.name().size() + 1;
  const uint32_t size_needed =
    std::max<uint32_t>(align(raw_size, sizeof(typename T::uint)), original_size);
  const uint32_t padding = size_needed - raw_size;

  if (library.original_data_.size() < size_needed ||
      library.size() < size_needed)
  {
    LIEF_WARN("Not enough spaces to rebuild {}. Size required: 0x{:x} vs 0x{:x}",
              library.name(),  library.original_data_.size(), size_needed);
  }

  details::dylib_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::dylib_command));

  raw_cmd.cmd                   = static_cast<uint32_t>(library.command());
  raw_cmd.cmdsize               = static_cast<uint32_t>(size_needed);
  raw_cmd.name                  = static_cast<uint32_t>(sizeof(details::dylib_command));
  raw_cmd.timestamp             = static_cast<uint32_t>(library.timestamp());
  raw_cmd.current_version       = static_cast<uint32_t>(DylibCommand::version2int(library.current_version()));
  raw_cmd.compatibility_version = static_cast<uint32_t>(DylibCommand::version2int(library.compatibility_version()));

  library.size_ = size_needed;
  library.original_data_.clear();

  // Write Header
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd), reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            std::back_inserter(library.original_data_));

  // Write String
  const std::string& libname = library.name();
  std::move(std::begin(libname), std::end(libname),
            std::back_inserter(library.original_data_));
  library.original_data_.push_back(0);
  library.original_data_.insert(std::end(library.original_data_), padding, 0);
  return ok();
}


template <typename T>
ok_error_t Builder::build(DylinkerCommand& linker) {

  LIEF_DEBUG("Build dylinker '{}'", linker.name());

  const uint32_t original_size = linker.original_data_.size();
  const uint32_t raw_size = sizeof(details::dylinker_command) + linker.name().size() + 1;
  const uint32_t size_needed =
    std::max<uint32_t>(align(raw_size, sizeof(typename T::uint)), original_size);
  const uint32_t padding = size_needed - raw_size;

  if (linker.original_data_.size() < size_needed ||
      linker.size() < size_needed)
  {
    LIEF_WARN("Not enough spaces to rebuild {}. Size required: 0x{:x} vs 0x{:x}",
              linker.name(),  linker.original_data_.size(), size_needed);
  }

  details::dylinker_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(raw_cmd));

  raw_cmd.cmd     = static_cast<uint32_t>(linker.command());
  raw_cmd.cmdsize = static_cast<uint32_t>(size_needed);
  raw_cmd.name    = static_cast<uint32_t>(sizeof(details::dylinker_command));

  linker.size_ = size_needed;
  linker.original_data_.clear();

  // Write Header
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd), reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            std::back_inserter(linker.original_data_));

  // Write String
  const std::string& linkpath = linker.name();
  std::move(std::begin(linkpath), std::end(linkpath),
            std::back_inserter(linker.original_data_));

  linker.original_data_.push_back(0);
  linker.original_data_.insert(std::end(linker.original_data_), padding, 0);
  return ok();
}

template<class T>
ok_error_t Builder::build(VersionMin& version_min) {
  LIEF_DEBUG("Build '{}'", to_string(version_min.command()));
  const uint32_t raw_size = sizeof(details::version_min_command);
  const uint32_t size_needed = align(raw_size, sizeof(typename T::uint));
  const uint32_t padding = size_needed - raw_size;

  details::version_min_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::version_min_command));

  const VersionMin::version_t& version = version_min.version();
  const VersionMin::version_t& sdk     = version_min.sdk();

  raw_cmd.cmd     = static_cast<uint32_t>(version_min.command());
  raw_cmd.cmdsize = static_cast<uint32_t>(version_min.size());
  raw_cmd.version = static_cast<uint32_t>(version[0] << 16 | version[1] << 8 | version[2]);
  raw_cmd.sdk     = static_cast<uint32_t>(sdk[0] << 16 | sdk[1] << 8 | sdk[2]);

  version_min.size_ = sizeof(details::version_min_command);
  version_min.original_data_.clear();
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(details::version_min_command),
            std::back_inserter(version_min.original_data_));
  version_min.original_data_.insert(std::end(version_min.original_data_), padding, 0);
  return ok();
}


template<class T>
ok_error_t Builder::build(SourceVersion& source_version) {
  LIEF_DEBUG("Build '{}'", to_string(source_version.command()));
  const uint32_t raw_size = sizeof(details::source_version_command);
  const uint32_t size_needed = align(raw_size, sizeof(typename T::uint));
  const uint32_t padding = size_needed - raw_size;

  details::source_version_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::source_version_command));

  const SourceVersion::version_t& version = source_version.version();
  raw_cmd.cmd     = static_cast<uint32_t>(source_version.command());
  raw_cmd.cmdsize = static_cast<uint32_t>(source_version.size());
  raw_cmd.version = static_cast<uint64_t>(
      static_cast<uint64_t>(version[0]) << 40 |
      static_cast<uint64_t>(version[1]) << 30 |
      static_cast<uint64_t>(version[2]) << 20 |
      static_cast<uint64_t>(version[3]) << 10 |
      static_cast<uint64_t>(version[4]));

  source_version.size_ = sizeof(details::source_version_command);
  source_version.original_data_.clear();
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(details::source_version_command),
            std::back_inserter(source_version.original_data_));
  source_version.original_data_.insert(std::end(source_version.original_data_), padding, 0);
  return ok();
}


template<class T>
ok_error_t Builder::build(RPathCommand& rpath_cmd) {
  LIEF_DEBUG("Build '{}'", to_string(rpath_cmd.command()));

  const uint32_t original_size = rpath_cmd.original_data_.size();

  const uint32_t raw_size = sizeof(details::rpath_command) + rpath_cmd.path().size() + 1;
  const uint32_t size_needed =
    std::max<uint32_t>(align(raw_size, sizeof(typename T::uint)), original_size);
  const uint32_t padding = size_needed - raw_size;

  if (rpath_cmd.original_data_.size() < size_needed ||
      rpath_cmd.size() < size_needed)
  {
    LIEF_WARN("Not enough room left to rebuild {}."
              "required=0x{:x} available=0x{:x}",
              rpath_cmd.path(), size_needed, rpath_cmd.original_data_.size());
  }


  details::rpath_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::rpath_command));

  raw_cmd.cmd     = static_cast<uint32_t>(rpath_cmd.command());
  raw_cmd.cmdsize = static_cast<uint32_t>(size_needed);
  raw_cmd.path    = static_cast<uint32_t>(sizeof(details::rpath_command));

  rpath_cmd.size_ = size_needed;
  rpath_cmd.original_data_.clear();

  // Write Header
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            std::back_inserter(rpath_cmd.original_data_));

  // Write String
  const std::string& rpath = rpath_cmd.path();
  std::move(std::begin(rpath), std::end(rpath),
            std::back_inserter(rpath_cmd.original_data_));
  rpath_cmd.original_data_.push_back(0);
  rpath_cmd.original_data_.insert(std::end(rpath_cmd.original_data_), padding, 0);
  return ok();
}

template<class T>
ok_error_t Builder::build(Routine& routine) {
  using routine_t = typename T::routines_command;
  using uint__ = typename T::uint;
  LIEF_DEBUG("Build '{}'", to_string(routine.command()));

  routine_t raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(routine_t));

  raw_cmd.cmd       = static_cast<uint32_t>(routine.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(routine.size());

  raw_cmd.init_address = static_cast<uint__>(routine.init_address());
  raw_cmd.init_module  = static_cast<uint__>(routine.init_module());
  raw_cmd.reserved1    = static_cast<uint__>(routine.reserved1());
  raw_cmd.reserved2    = static_cast<uint__>(routine.reserved2());
  raw_cmd.reserved3    = static_cast<uint__>(routine.reserved3());
  raw_cmd.reserved4    = static_cast<uint__>(routine.reserved4());
  raw_cmd.reserved5    = static_cast<uint__>(routine.reserved5());
  raw_cmd.reserved6    = static_cast<uint__>(routine.reserved6());

  routine.size_ = sizeof(routine_t);
  routine.original_data_.clear();
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(routine_t),
            std::back_inserter(routine.original_data_));
  return ok();
}

template<class T>
ok_error_t Builder::build(MainCommand& main_cmd) {
  LIEF_DEBUG("Build '{}'", to_string(main_cmd.command()));
  const uint32_t raw_size = sizeof(details::entry_point_command);
  const uint32_t size_needed = align(raw_size, sizeof(typename T::uint));
  const uint32_t padding = size_needed - raw_size;

  details::entry_point_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::entry_point_command));

  raw_cmd.cmd       = static_cast<uint32_t>(main_cmd.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(main_cmd.size());
  raw_cmd.entryoff  = static_cast<uint64_t>(main_cmd.entrypoint());
  raw_cmd.stacksize = static_cast<uint64_t>(main_cmd.stack_size());

  main_cmd.size_ = sizeof(details::entry_point_command);
  main_cmd.original_data_.clear();
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(details::entry_point_command),
            std::back_inserter(main_cmd.original_data_));
  main_cmd.original_data_.insert(std::end(main_cmd.original_data_), padding, 0);
  return ok();
}

template<class T>
ok_error_t Builder::build(NoteCommand& note) {
  LIEF_DEBUG("Build '{}'", to_string(note.command()));
  details::note_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::note_command));

  raw_cmd.cmd = static_cast<uint32_t>(note.command());
  raw_cmd.cmdsize = static_cast<uint32_t>(note.size());

  raw_cmd.offset = static_cast<uint32_t>(note.note_offset());
  raw_cmd.size = static_cast<uint32_t>(note.note_size());

  span<const char> owner = note.owner();
  std::copy(owner.begin(), owner.end(), std::begin(raw_cmd.data_owner));

  note.size_ = sizeof(details::note_command);

  std::fill(note.original_data_.begin(), note.original_data_.end(), 0);

  std::copy(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            reinterpret_cast<uint8_t*>(note.original_data_.data()));

  return ok();
}

template<class T>
ok_error_t Builder::build(DyldInfo& dyld_info) {
  LIEF_DEBUG("Build '{}'", to_string(dyld_info.command()));

  // /!\ Force to update relocation cache that is used by the following functions
  // TODO(romain): This looks like a hack
  binary_->relocations();

  details::dyld_info_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::dyld_info_command));
  {
    LIEF_DEBUG("linkedit_.size(): {:x}", linkedit_.size());
    raw_cmd.rebase_off = linkedit_.size();
    {
      dyld_info.update_rebase_info(linkedit_);
    }
    raw_cmd.rebase_size = linkedit_.size() - raw_cmd.rebase_off;
    if (raw_cmd.rebase_size > 0) {
      raw_cmd.rebase_off += linkedit_offset_;
    }

    LIEF_DEBUG("LC_DYLD_INFO.rebase_off : 0x{:06x} -> 0x{:06x}",
               dyld_info.rebase().first, raw_cmd.rebase_off);

    LIEF_DEBUG("LC_DYLD_INFO.rebase_size: 0x{:06x} -> 0x{:06x}",
               dyld_info.rebase().second, raw_cmd.rebase_size);
  }
  {
    dyld_info.update_binding_info(linkedit_, raw_cmd);
    if (raw_cmd.bind_size > 0) {
      raw_cmd.bind_off += linkedit_offset_;

      LIEF_DEBUG("LC_DYLD_INFO.bind_off : 0x{:06x} -> 0x{:06x}",
                 dyld_info.bind().first, raw_cmd.bind_off);

      LIEF_DEBUG("LC_DYLD_INFO.bind_size: 0x{:06x} -> 0x{:06x}",
                 dyld_info.bind().second, raw_cmd.bind_size);
    }

    if (raw_cmd.weak_bind_size > 0) {
      raw_cmd.weak_bind_off += linkedit_offset_;

      LIEF_DEBUG("LC_DYLD_INFO.weak_bind_off : 0x{:06x} -> 0x{:06x}",
                 dyld_info.weak_bind().first, raw_cmd.weak_bind_off);

      LIEF_DEBUG("LC_DYLD_INFO.weak_bind_size: 0x{:06x} -> 0x{:06x}",
                 dyld_info.weak_bind().second, raw_cmd.weak_bind_size);
    }

    if (raw_cmd.lazy_bind_size > 0) {
      raw_cmd.lazy_bind_off += linkedit_offset_;

      LIEF_DEBUG("LC_DYLD_INFO.lazy_bind_off : 0x{:06x} -> 0x{:06x}",
                 dyld_info.lazy_bind().first, raw_cmd.lazy_bind_off);

      LIEF_DEBUG("LC_DYLD_INFO.lazy_bind_size: 0x{:06x} -> 0x{:06x}",
                 dyld_info.lazy_bind().second, raw_cmd.lazy_bind_size);
    }
  }
  {
    raw_cmd.export_off = linkedit_.size();
    {
      dyld_info.update_export_trie(linkedit_);
    }
    raw_cmd.export_size = linkedit_.size() - raw_cmd.export_off;
    if (raw_cmd.export_size > 0) {
      raw_cmd.export_off += linkedit_offset_;
    }
    LIEF_DEBUG("LC_DYLD_INFO.exports_off : 0x{:06x} -> 0x{:06x}",
               dyld_info.export_info().first, raw_cmd.export_off);

    LIEF_DEBUG("LC_DYLD_INFO.exports_size: 0x{:06x} -> 0x{:06x}",
               dyld_info.export_info().second, raw_cmd.export_size);
  }

  raw_cmd.cmd     = static_cast<uint32_t>(dyld_info.command());
  raw_cmd.cmdsize = static_cast<uint32_t>(dyld_info.size());

  dyld_info.size_ = sizeof(details::dyld_info_command);
  dyld_info.original_data_.clear();
  dyld_info.original_data_.resize(dyld_info.size_);
  memcpy(dyld_info.original_data_.data(), &raw_cmd, sizeof(details::dyld_info_command));
  return ok();
}


template<class T>
ok_error_t Builder::build(FunctionStarts& function_starts) {
  LIEF_DEBUG("Build '{}'", to_string(function_starts.command()));
  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));
  raw_cmd.dataoff = linkedit_.size();

  uint64_t prev_address = 0;
  for (uint64_t address : function_starts.functions()) {
    uint64_t delta = address - prev_address;
    linkedit_.write_uleb128(delta);
    prev_address = address;
  }
  linkedit_.align(sizeof(typename T::uint));

  raw_cmd.cmd       = static_cast<uint32_t>(function_starts.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(function_starts.size());
  raw_cmd.datasize  = linkedit_.size() - raw_cmd.dataoff;
  raw_cmd.dataoff   += linkedit_offset_;


  LIEF_DEBUG("LC_FUNCTION_STARTS.offset: 0x{:06x} -> 0x{:x}",
             function_starts.data_offset(), raw_cmd.dataoff);
  LIEF_DEBUG("LC_FUNCTION_STARTS.size:   0x{:06x} -> 0x{:x}",
             function_starts.data_size(), raw_cmd.datasize);

  function_starts.size_ = sizeof(details::linkedit_data_command);
  function_starts.original_data_.clear();
  function_starts.original_data_.resize(function_starts.size_);
  memcpy(function_starts.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class MACHO_T>
inline ok_error_t write_symbol(vector_iostream& nlist_table, Symbol& sym,
                        std::unordered_map<std::string, size_t>& offset_name_map) {
  using nlist_t = typename MACHO_T::nlist;
  const std::string& name = sym.name();
  const auto it_name = offset_name_map.find(name);

  if (it_name == std::end(offset_name_map)) {
    LIEF_WARN("Can't find name offset for symbol {}", sym.name());
    return make_error_code(lief_errors::not_found);
  }

  nlist_t nl;
  nl.n_strx  = static_cast<uint32_t>(it_name->second);
  nl.n_type  = static_cast<uint8_t>(sym.raw_type());
  nl.n_sect  = static_cast<uint32_t>(sym.numberof_sections());
  nl.n_desc  = static_cast<uint16_t>(sym.description());
  nl.n_value = static_cast<typename MACHO_T::uint>(sym.value());

  nlist_table.write(nl);
  return ok();
}


template<class T>
ok_error_t Builder::build(SymbolCommand& symbol_command) {
  //template <typename A>
  //void SymbolTableAtom<A>::encode()
  //{
  //  // Note: We lay out the symbol table so that the strings for the stabs (local) symbols are at the
  //  // end of the string pool.  The stabs strings are not used when calculated the UUID for the image.
  //  // If the stabs strings were not last, the string offsets for all other symbols may very which would alter the UUID.
  //
  //  // reserve space for local symbols
  // +---------------------+
  // |                     |
  // |  symtab_command     |
  // |                     |
  // +---------------------+
  // |                     |
  // |  n_list             |
  // |                     |
  // +---------------------+
  // |                     |
  // | string table        |
  // |                     |
  // +---------------------+

  using nlist_t = typename T::nlist;

  std::vector<Symbol*> local_syms;
  std::vector<Symbol*> ext_syms;
  std::vector<Symbol*> undef_syms;
  std::vector<Symbol*> other_syms;
  std::vector<Symbol*> all_syms;
  std::map<Symbol*, uint32_t> indirect_symbols;

  std::vector<uint8_t> strtab;
  std::vector<uint8_t> raw_nlist_table;
  std::unordered_map<std::string, size_t> offset_name_map;

  details::symtab_command symtab;
  std::memset(&symtab, 0, sizeof(details::symtab_command));
  DynamicSymbolCommand* dynsym = binary_->dynamic_symbol_command();

  /* 1. Fille the n_list table */ {
    for (Symbol& s : binary_->symbols()) {
      if (s.origin() != Symbol::ORIGIN::SYMTAB) {
        continue;
      }
      all_syms.push_back(&s);
      switch (s.category()) {
        case Symbol::CATEGORY::NONE:      other_syms.push_back(&s); break;
        case Symbol::CATEGORY::LOCAL:     local_syms.push_back(&s); break;
        case Symbol::CATEGORY::EXTERNAL:  ext_syms.push_back(&s);   break;
        case Symbol::CATEGORY::UNDEFINED: undef_syms.push_back(&s); break;

        case Symbol::CATEGORY::INDIRECT_ABS:
        case Symbol::CATEGORY::INDIRECT_LOCAL:
        case Symbol::CATEGORY::INDIRECT_ABS_LOCAL:
          {
            break;
          }
      }
    }

    size_t offset_counter = 1;
    std::vector<std::string> string_table_opt = optimize(all_syms,
                                                  [] (Symbol *const &sym) { return sym->name(); },
                                                  offset_counter, &offset_name_map);
    all_syms.clear();

    // 0 index is reserved
    vector_iostream raw_symbol_names;
    raw_symbol_names.write<uint8_t>(0);
    for (const std::string& name : string_table_opt) {
      raw_symbol_names.write(name);
    }
    raw_symbol_names.align(8);
    strtab = raw_symbol_names.raw();
  }

  /* 2. Fille the n_list table */ {
    const size_t nb_symbols = local_syms.size() + ext_syms.size() +
                              undef_syms.size() + other_syms.size();
    vector_iostream nlist_table;
    nlist_table.reserve(nb_symbols * sizeof(nlist_t));

    size_t isym = 0;
    /* Local Symbols */ {
      if (dynsym != nullptr) {
        dynsym->idx_local_symbol(isym);
      }
      for (Symbol* sym : local_syms) {
        indirect_symbols[sym] = isym;
        write_symbol<T>(nlist_table, *sym, offset_name_map);
        ++isym;
      }
      if (dynsym != nullptr) {
        dynsym->nb_local_symbols(local_syms.size());
      }
    }

    /* External Symbols */ {
      if (dynsym != nullptr) {
        dynsym->idx_external_define_symbol(isym);
      }
      for (Symbol* sym : ext_syms)   {
        indirect_symbols[sym] = isym;
        write_symbol<T>(nlist_table, *sym, offset_name_map);
        ++isym;
      }
      if (dynsym != nullptr) {
        dynsym->nb_external_define_symbols(ext_syms.size());
      }
    }
    /* Undefined Symbols */ {
      if (dynsym != nullptr) {
        dynsym->idx_undefined_symbol(isym);
      }
      for (Symbol* sym : undef_syms) {
        indirect_symbols[sym] = isym;
        write_symbol<T>(nlist_table, *sym, offset_name_map);
        ++isym;
      }
      if (dynsym != nullptr) {
        dynsym->nb_undefined_symbols(undef_syms.size());
      }
    }

    /* The other symbols [...] */ {
      for (Symbol* sym : other_syms) {
        indirect_symbols[sym] = isym;
        write_symbol<T>(nlist_table, *sym, offset_name_map);
        ++isym;
      }
    }
    nlist_table.align(binary_->is64_ ? 8 : 4);

    raw_nlist_table = nlist_table.raw();
    symtab.symoff = linkedit_offset_ + linkedit_.size();
    symtab.nsyms  = nb_symbols;
    LIEF_DEBUG("LC_SYMTAB.nlist:      0x{:06x} -> 0x{:x}",
               symbol_command.symbol_offset(), symtab.symoff);
    LIEF_DEBUG("LC_SYMTAB.nb_symbols: 0x{:06x} -> 0x{:x}",
               symbol_command.numberof_symbols(), symtab.nsyms);
    linkedit_.write(std::move(raw_nlist_table));
  }
  /*
   * Two Level Hints
   */
  if (auto* two = binary_->two_level_hints()) {
    build<T>(*two);
  }
  /*
   * Indirect symbol table
   */
  if (dynsym != nullptr) {
    LIEF_DEBUG("LC_DYSYMTAB.indirectsymoff: 0x{:06x} -> 0x{:x}",
               dynsym->indirect_symbol_offset(), linkedit_offset_ + linkedit_.size());
    dynsym->indirect_symbol_offset(linkedit_offset_ + linkedit_.size());
    size_t count = 0;
    for (Symbol* sym : dynsym->indirect_symbols_) {
      if (sym->category() == Symbol::CATEGORY::INDIRECT_ABS) {
        linkedit_.write(details::INDIRECT_SYMBOL_ABS);
        ++count;
        continue;
      }

      if (sym->category() == Symbol::CATEGORY::INDIRECT_LOCAL) {
        linkedit_.write(details::INDIRECT_SYMBOL_LOCAL);
        ++count;
        continue;
      }

      if (sym->category() == Symbol::CATEGORY::INDIRECT_ABS_LOCAL) {
        linkedit_.write(details::INDIRECT_SYMBOL_LOCAL | details::INDIRECT_SYMBOL_ABS);
        ++count;
        continue;
      }

      auto it_idx = indirect_symbols.find(sym);
      if (it_idx != std::end(indirect_symbols)) {
        linkedit_.write(it_idx->second);
        ++count;
      } else {
        LIEF_ERR("Can't find the symbol index");
      }
    }
    LIEF_DEBUG("LC_DYSYMTAB.nindirectsyms:    0x{:06x} -> 0x{:x}",
               dynsym->nb_indirect_symbols(), count);
    dynsym->nb_indirect_symbols(count);
  }

  symtab.stroff  = linkedit_offset_ + linkedit_.size();
  symtab.strsize = strtab.size();
  LIEF_DEBUG("LC_SYMTAB.strtab.offset: 0x{:06x} -> 0x{:x}",
             symbol_command.strings_offset(), symtab.stroff);
  LIEF_DEBUG("LC_SYMTAB.strtab.size:   0x{:06x} -> 0x{:x}",
             symbol_command.strings_size(), symtab.strsize);

  linkedit_.write(std::move(strtab));

  symtab.cmd     = static_cast<uint32_t>(symbol_command.command());
  symtab.cmdsize = static_cast<uint32_t>(symbol_command.size());

  symbol_command.original_data_.clear();
  symbol_command.original_data_.resize(sizeof(details::symtab_command));

  std::memcpy(symbol_command.original_data_.data(), &symtab, sizeof(details::symtab_command));

  return ok();
}


template<class T>
ok_error_t Builder::build(DynamicSymbolCommand& symbol_command) {
  details::dysymtab_command rawcmd;
  std::memset(&rawcmd, 0, sizeof(details::dysymtab_command));

  rawcmd.cmd            = static_cast<uint32_t>(symbol_command.command());
  rawcmd.cmdsize        = static_cast<uint32_t>(symbol_command.size());

  rawcmd.ilocalsym      = static_cast<uint32_t>(symbol_command.idx_local_symbol());
  rawcmd.nlocalsym      = static_cast<uint32_t>(symbol_command.nb_local_symbols());

  rawcmd.iextdefsym     = static_cast<uint32_t>(symbol_command.idx_external_define_symbol());
  rawcmd.nextdefsym     = static_cast<uint32_t>(symbol_command.nb_external_define_symbols());

  rawcmd.iundefsym      = static_cast<uint32_t>(symbol_command.idx_undefined_symbol());
  rawcmd.nundefsym      = static_cast<uint32_t>(symbol_command.nb_undefined_symbols());

  rawcmd.indirectsymoff = static_cast<uint32_t>(symbol_command.indirect_symbol_offset());
  rawcmd.nindirectsyms  = static_cast<uint32_t>(symbol_command.nb_indirect_symbols());

  rawcmd.tocoff         = static_cast<uint32_t>(symbol_command.toc_offset());
  rawcmd.ntoc           = static_cast<uint32_t>(symbol_command.nb_toc());
  rawcmd.modtaboff      = static_cast<uint32_t>(symbol_command.module_table_offset());
  rawcmd.nmodtab        = static_cast<uint32_t>(symbol_command.nb_module_table());
  rawcmd.extrefsymoff   = static_cast<uint32_t>(symbol_command.external_reference_symbol_offset());
  rawcmd.nextrefsyms    = static_cast<uint32_t>(symbol_command.nb_external_reference_symbols());
  rawcmd.extreloff      = static_cast<uint32_t>(symbol_command.external_relocation_offset());
  rawcmd.nextrel        = static_cast<uint32_t>(symbol_command.nb_external_relocations());
  rawcmd.locreloff      = static_cast<uint32_t>(symbol_command.local_relocation_offset());
  rawcmd.nlocrel        = static_cast<uint32_t>(symbol_command.nb_local_relocations());

  symbol_command.original_data_.clear();
  symbol_command.original_data_.resize(sizeof(details::dysymtab_command));
  memcpy(symbol_command.original_data_.data(), &rawcmd, sizeof(details::dysymtab_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(DataInCode& datacode) {
  LIEF_DEBUG("Build '{}'", to_string(datacode.command()));

  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));

  span<const uint8_t> raw_content = datacode.content();

  raw_cmd.cmd       = static_cast<uint32_t>(datacode.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(datacode.size());
  raw_cmd.dataoff   = linkedit_.size();

  for (const DataCodeEntry& entry : datacode.entries()) {
    details::data_in_code_entry e;
    e.offset = entry.offset();
    e.length = entry.length();
    e.kind   = static_cast<decltype(e.kind)>(entry.type());
    linkedit_.write(e);
  }
  raw_cmd.datasize = linkedit_.size() - raw_cmd.dataoff;
  raw_cmd.dataoff  += linkedit_offset_;

  LIEF_DEBUG("LC_DATA_IN_CODE.offset: 0x{:06x} -> 0x{:x}",
             datacode.data_offset(), raw_cmd.dataoff);
  LIEF_DEBUG("LC_DATA_IN_CODE.size:   0x{:06x} -> 0x{:x}",
             datacode.data_size(), raw_content.size());

  datacode.size_ = sizeof(details::linkedit_data_command);
  datacode.original_data_.clear();
  datacode.original_data_.resize(datacode.size_);

  memcpy(datacode.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(CodeSignature& code_signature) {
  LIEF_DEBUG("Build '{}'", to_string(code_signature.command()));

  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));

  span<const uint8_t> sp = code_signature.content();

  /*
   * The code signature Payload requires to be aligned on 16-bits
   */
  linkedit_.align(16);

  raw_cmd.cmd       = static_cast<uint32_t>(code_signature.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(code_signature.size());
  raw_cmd.dataoff   = linkedit_offset_ + linkedit_.size();
  raw_cmd.datasize  = sp.size();

  LIEF_DEBUG("LC_CODE_SIGNATURE.offset: 0x{:06x} -> 0x{:x}",
             code_signature.data_offset(), raw_cmd.dataoff);
  LIEF_DEBUG("LC_CODE_SIGNATURE.size:   0x{:06x} -> 0x{:x}",
             code_signature.data_size(), raw_cmd.datasize);
  linkedit_.write(sp.data(), sp.size());

  code_signature.size_ = sizeof(details::linkedit_data_command);
  code_signature.original_data_.clear();
  code_signature.original_data_.resize(code_signature.size_);

  memcpy(code_signature.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(SegmentSplitInfo& ssi) {
  LIEF_DEBUG("Build '{}'", to_string(ssi.command()));

  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));

  span<const uint8_t> raw_content = ssi.content();

  LIEF_DEBUG("LC_SEGMENT_SPLIT_INFO.offset: 0x{:06x} -> 0x{:x}",
             ssi.data_offset(), linkedit_offset_ + linkedit_.size());
  LIEF_DEBUG("LC_SEGMENT_SPLIT_INFO.size:   0x{:06x} -> 0x{:x}",
             ssi.data_size(), raw_content.size());

  raw_cmd.cmd       = static_cast<uint32_t>(ssi.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(ssi.size());
  raw_cmd.dataoff   = linkedit_offset_ + linkedit_.size();
  raw_cmd.datasize  = raw_content.size();

  linkedit_.write(raw_content.data(), raw_content.size());

  ssi.size_ = sizeof(details::linkedit_data_command);
  ssi.original_data_.clear();
  ssi.original_data_.resize(ssi.size_);

  memcpy(ssi.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));

  return ok();
}

template<class T>
ok_error_t Builder::build(SubFramework& sf) {
  LIEF_DEBUG("Build '{}'", to_string(sf.command()));
  details::sub_framework_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::sub_framework_command));

  const uint32_t original_size = sf.original_data_.size();

  const uint32_t raw_size = sizeof(details::sub_framework_command) + sf.umbrella().size() + 1;
  const auto size_needed =
    std::max<uint32_t>(align(raw_size, sizeof(typename T::uint)), original_size);

  const uint32_t padding = size_needed - raw_size;

  if (sf.original_data_.size() < size_needed || sf.size() < size_needed) {
    LIEF_WARN("Not enough spaces to rebuild '{}'. Size required: 0x{:x} vs 0x{:x}",
              sf.umbrella(),  size_needed, sf.original_data_.size());
  }

  raw_cmd.cmd      = static_cast<uint32_t>(sf.command());
  raw_cmd.cmdsize  = static_cast<uint32_t>(size_needed);
  raw_cmd.umbrella = static_cast<uint32_t>(sizeof(details::sub_framework_command));

  sf.size_ = size_needed;
  sf.original_data_.clear();

  // Write Header
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            std::back_inserter(sf.original_data_));

  // Write String
  const std::string& um = sf.umbrella();
  std::move(std::begin(um), std::end(um),
            std::back_inserter(sf.original_data_));
  sf.original_data_.push_back(0);
  sf.original_data_.insert(std::end(sf.original_data_), padding, 0);
  return ok();
}

template<class T>
ok_error_t Builder::build(SubClient& sc) {
  LIEF_DEBUG("Build '{}'", to_string(sc.command()));
  details::sub_client_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::sub_client_command));

  const uint32_t original_size = sc.original_data_.size();

  const uint32_t raw_size = sizeof(details::sub_client_command) + sc.client().size() + 1;
  const uint32_t size_needed =
    std::max<uint32_t>(align(raw_size, sizeof(typename T::uint)), original_size);
  const uint32_t padding = size_needed - raw_size;

  if (sc.original_data_.size() < size_needed || sc.size() < size_needed) {
    LIEF_WARN("Not enough spaces to rebuild '{}'. Size required: 0x{:x} vs 0x{:x}",
              sc.client(),  size_needed, sc.original_data_.size());
  }

  raw_cmd.cmd      = static_cast<uint32_t>(sc.command());
  raw_cmd.cmdsize  = static_cast<uint32_t>(size_needed);
  raw_cmd.client   = static_cast<uint32_t>(sizeof(details::sub_client_command));

  sc.size_ = size_needed;
  sc.original_data_.clear();

  // Write Header
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            std::back_inserter(sc.original_data_));

  // Write String
  const std::string& um = sc.client();
  std::move(std::begin(um), std::end(um),
            std::back_inserter(sc.original_data_));
  sc.original_data_.push_back(0);
  sc.original_data_.insert(std::end(sc.original_data_), padding, 0);
  return ok();
}

template<class T>
ok_error_t Builder::build(DyldEnvironment& de) {
  details::dylinker_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::dylinker_command));

  const uint32_t original_size = de.original_data_.size();

  const uint32_t raw_size = sizeof(details::dylinker_command) + de.value().size() + 1;
  const uint32_t size_needed =
    std::max<uint32_t>(align(raw_size, sizeof(typename T::uint)), original_size);
  const uint32_t padding = size_needed - raw_size;

  if (de.original_data_.size() < size_needed ||
      de.size() < size_needed) {
    LIEF_WARN("Not enough spaces to rebuild {}. Size required: 0x{:x} vs 0x{:x}",
              de.value(),  de.original_data_.size(), size_needed);
  }

  raw_cmd.cmd      = static_cast<uint32_t>(de.command());
  raw_cmd.cmdsize  = static_cast<uint32_t>(size_needed);
  raw_cmd.name     = static_cast<uint32_t>(sizeof(details::dylinker_command));

  de.size_ = size_needed;
  de.original_data_.clear();

  // Write Header
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            std::back_inserter(de.original_data_));

  // Write String
  const std::string& value = de.value();
  std::move(std::begin(value), std::end(value),
            std::back_inserter(de.original_data_));

  de.original_data_.push_back(0);
  de.original_data_.insert(std::end(de.original_data_), padding, 0);
  return ok();
}

template<class T>
ok_error_t Builder::build(ThreadCommand& tc) {
  details::thread_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::thread_command));

  const span<const uint8_t> state = tc.state();

  const uint32_t raw_size = sizeof(details::thread_command) + state.size();
  const uint32_t size_needed = align(raw_size, sizeof(typename T::uint));
  const uint32_t padding = size_needed - raw_size;

  if (tc.original_data_.size() < size_needed || tc.size() < size_needed) {
    LIEF_WARN("Not enough spaces to rebuild 'ThreadCommand'. Size required: 0x{:x} vs 0x{:x}",
              tc.original_data_.size(), size_needed);
  }

  const uint32_t state_size_needed = tc.count() * sizeof(uint32_t);
  if (state.size() < state_size_needed) {

    LIEF_WARN("Not enough spaces to rebuild 'ThreadCommand'. Size required: 0x{:x} vs 0x{:x}",
              state.size(), state_size_needed);
  }


  raw_cmd.cmd      = static_cast<uint32_t>(tc.command());
  raw_cmd.cmdsize  = static_cast<uint32_t>(size_needed);
  raw_cmd.flavor   = static_cast<uint32_t>(tc.flavor());
  raw_cmd.count    = static_cast<uint32_t>(tc.count());

  tc.size_ = size_needed;
  tc.original_data_.clear();

  // Write Header
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            std::back_inserter(tc.original_data_));

  // Write state
  std::move(std::begin(state), std::end(state),
            std::back_inserter(tc.original_data_));

  tc.original_data_.push_back(0);
  tc.original_data_.insert(std::end(tc.original_data_), padding, 0);
  return ok();
}

template<class T>
ok_error_t Builder::build(EncryptionInfo& info) {
  details::encryption_info_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::encryption_info_command));

  raw_cmd.cmd       = static_cast<uint32_t>(info.command());
  raw_cmd.cmdsize   = info.size();
  raw_cmd.cryptoff  = info.crypt_offset();
  raw_cmd.cryptsize = info.crypt_size();
  raw_cmd.cryptid   = info.crypt_id();

  std::fill(info.original_data_.begin(), info.original_data_.end(), 0);

  std::copy(reinterpret_cast<uint8_t*>(&raw_cmd),
            reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            reinterpret_cast<uint8_t*>(info.original_data_.data()));

  return ok();
}

template <typename T>
ok_error_t Builder::update_fixups(DyldChainedFixups& command) {
  for (const auto& seg_info : command.chained_starts_in_segments()) {
    for (const std::unique_ptr<Relocation>& reloc : seg_info.segment.relocations_) {
      if (!RelocationFixup::classof(*reloc)) {
        LIEF_WARN("Weird: a relocation in {} is not RelocationFixup", seg_info.segment.name());
        continue;
      }
      const auto& fixup = static_cast<const RelocationFixup&>(*reloc);
      span<uint8_t> sdata = seg_info.segment.writable_content();
      const uint64_t seg_off = seg_info.segment.file_offset();

      LIEF_DEBUG("0x{:010x}: 0x{:010x} Offset: 0x{:x}", fixup.address(), fixup.target(), fixup.offset_);
      const uint64_t rel_offset = fixup.offset_ - seg_off;
      switch (fixup.rtypes_) {
        case RelocationFixup::REBASE_TYPES::ARM64E_AUTH_REBASE:
          {
            auto& raw_fixup = *reinterpret_cast<details::dyld_chained_ptr_arm64e*>(sdata.data() + rel_offset);
            raw_fixup.auth_rebase = *fixup.arm64_auth_rebase_;
            break;
          }

        case RelocationFixup::REBASE_TYPES::ARM64E_REBASE:
          {
            auto& raw_fixup = *reinterpret_cast<details::dyld_chained_ptr_arm64e*>(sdata.data() + rel_offset);
            raw_fixup.rebase = *fixup.arm64_rebase_;
            break;
          }

        case RelocationFixup::REBASE_TYPES::PTR64_REBASE:
          {
            auto& raw_fixup = *reinterpret_cast<details::dyld_chained_ptr_generic64*>(sdata.data() + rel_offset);
            raw_fixup.rebase = *fixup.p64_rebase_;
            break;
          }

        case RelocationFixup::REBASE_TYPES::PTR32_REBASE:
          {
            auto& raw_fixup = *reinterpret_cast<details::dyld_chained_ptr_generic32*>(sdata.data() + rel_offset);
            raw_fixup.rebase = *fixup.p32_rebase_;
            break;
          }

        case RelocationFixup::REBASE_TYPES::SEGMENTED:
          {
            [[maybe_unused]] auto& raw_fixup = *reinterpret_cast<details::dyld_chained_ptr_arm64e_segmented_rebase*>(sdata.data() + rel_offset);
            LIEF_ERR("dyld_chained_ptr_arm64e_segmented_rebase is not supported ({})", __LINE__);
            break;
          }

        case RelocationFixup::REBASE_TYPES::AUTH_SEGMENTED:
          {
            [[maybe_unused]] auto& raw_fixup = *reinterpret_cast<details::dyld_chained_ptr_arm64e_auth_segmented_rebase*>(sdata.data() + rel_offset);
            LIEF_ERR("dyld_chained_ptr_arm64e_auth_segmented_rebase is not supported ({})", __LINE__);
            break;
          }

        case RelocationFixup::REBASE_TYPES::UNKNOWN:
          {
            break;
          }
      }
    }
  }
  return ok();
}


template<typename T>
ok_error_t Builder::build(DyldChainedFixups& fixups) {
  using pin_t = typename T::uint;
  /*
   * Note(romain): Most of the logic that constructs this command
   *               is located in ld64/src/ld/LinkEdit.hpp - ChainedInfoAtom<A>::encode()
   *               The following code is highly inspired from the Apple ld64 code
   */
  LIEF_DEBUG("[->] Writing DyldChainedFixups");

  // First, we have to re-write the fixups
  if (!update_fixups<T>(fixups)) {
    LIEF_WARN("Error while re-writing chained fixups");
  }

  vector_iostream lnk_data;

  details::dyld_chained_fixups_header header;
  header.fixups_version = 0;
  header.starts_offset  = align(sizeof(details::dyld_chained_fixups_header), 8);
  header.imports_offset = 0;
  header.symbols_offset = 0;
  header.imports_count  = fixups.internal_bindings_.size();
  header.imports_format = static_cast<uint32_t>(fixups.imports_format());
  header.symbols_format = 0;

  lnk_data.reserve(fixups.data_size());
  lnk_data.write(header);
  lnk_data.align(8);

  const size_t segs_header_off = lnk_data.size();
  if (fixups.starts_offset() > 0 && segs_header_off != fixups.starts_offset()) {
    LIEF_INFO("segs_header_off could be wrong!");
  }
  auto starts_in_segment = fixups.chained_starts_in_segments();
  lnk_data.write<uint32_t>(starts_in_segment.size());
  const size_t segs_info_off = lnk_data.size();
  for (size_t i = 0; i < starts_in_segment.size(); ++i) {
    // Write empty offsets that will be filled later
    lnk_data.write<uint32_t>(0);
  }
  size_t seg_idx = 0;
  uint64_t text_start_address = 0;
  uint64_t max_rebase_address = 0;

  for (const auto& seg_info : starts_in_segment) {
    if (seg_info.segment.name() == "__TEXT") {
      text_start_address = seg_info.segment.virtual_address();
    }
    else if (seg_info.segment.name() == "__LINKEDIT") {
      uint64_t base_address = text_start_address;
      if (seg_info.pointer_format == DYLD_CHAINED_PTR_FORMAT::PTR_32 && text_start_address == 0x4000) {
        base_address = 0;
      }
      max_rebase_address = align(seg_info.segment.virtual_address() - base_address, 0x00100000);
    }
  }

  // -----


  for (const DyldChainedFixups::chained_starts_in_segment& seg_info : starts_in_segment) {
    if (seg_info.page_count() == 0) {
      ++seg_idx;
      continue;
    }
    uint16_t start_bytes_per_page = sizeof(uint16_t);
    if (seg_info.pointer_format == DYLD_CHAINED_PTR_FORMAT::PTR_32) {
      // TODO(romain): Would be worth implementing this case as it seems only
      //               used on x86 dyld?
      LIEF_ERR("DYLD_CHAINED_PTR_FORMAT::PTR_32 is not supported yet");
      return make_error_code(lief_errors::not_supported);
    }
    details::dyld_chained_starts_in_segment seg;
    const size_t page_count = seg_info.page_count();
    seg.size              = sizeof(details::dyld_chained_starts_in_segment) + page_count * start_bytes_per_page;
    seg.page_size         = seg_info.page_size;
    seg.pointer_format    = static_cast<uint16_t>(seg_info.pointer_format);
    seg.segment_offset    = seg_info.segment.virtual_address() - text_start_address;
    seg.page_count        = page_count;
    seg.max_valid_pointer = seg_info.pointer_format == DYLD_CHAINED_PTR_FORMAT::PTR_32 ?
                                                       max_rebase_address : 0;

    // According to the linker documentation, dyld_chained_starts_in_segment
    // must be 64-bit aligned.
    lnk_data.align(8);
    auto* seg_info_offsets = reinterpret_cast<uint32_t*>(lnk_data.raw().data() + segs_info_off);
    seg_info_offsets[seg_idx] = lnk_data.size() - segs_header_off;
    LIEF_DEBUG("0x{:06x} seg_info_offsets[{}] = 0x{:016x}",
               segs_info_off + sizeof(uint32_t) * seg_idx,
               seg_idx, seg_info_offsets[seg_idx]);
    lnk_data.write(seg);
    for (uint16_t off : seg_info.page_start) {
      lnk_data.write(off);
    }
    // Skip DYLD_CHAINED_PTR_32
    // {
    //  ...
    // }
    ++seg_idx;
  }


  // Now build the imports and symbol table
  vector_iostream string_pool;

  vector_iostream imports;
  vector_iostream imports_addend;
  vector_iostream imports_addend64;

  std::unordered_map<std::string, size_t> offset_name_map;
  string_pool.write<uint8_t>(0);
  size_t offset_counter = string_pool.tellp();
  std::vector<std::string> string_table_optimized = optimize(fixups.internal_bindings_,
                                  [] (const std::unique_ptr<ChainedBindingInfoList>& bnd) {
                                    if (const Symbol* s = bnd->symbol()) {
                                      return s->name();
                                    }
                                    return std::string();
                                  }, offset_counter, &offset_name_map);
  string_pool.reserve(string_table_optimized.size() * 10);
  for (const std::string& name : string_table_optimized) {
    string_pool.write(name);
  }

  const DYLD_CHAINED_FORMAT fmt = fixups.imports_format();
  const size_t nb_bindings = fixups.internal_bindings_.size();
  switch (fmt) {
    case DYLD_CHAINED_FORMAT::IMPORT:
         imports.reserve(nb_bindings * sizeof(details::dyld_chained_import)); break;
    case DYLD_CHAINED_FORMAT::IMPORT_ADDEND:
         imports.reserve(nb_bindings * sizeof(details::dyld_chained_import_addend)); break;
    case DYLD_CHAINED_FORMAT::IMPORT_ADDEND64:
         imports.reserve(nb_bindings * sizeof(details::dyld_chained_import_addend64)); break;
  }

  for (const std::unique_ptr<ChainedBindingInfoList>& info : fixups.internal_bindings_) {
    uint32_t name_offset = 0;

    const std::string& name = info->symbol()->name();
    auto it_name_off = offset_name_map.find(name);

    if (it_name_off != std::end(offset_name_map)) {
      name_offset = it_name_off->second;
    } else {
      LIEF_WARN("Can't find symbol: '{}'", name);
      name_offset = 0;
    }
    switch (fmt) {
      case DYLD_CHAINED_FORMAT::IMPORT:
        {
          details::dyld_chained_import import;
          import.lib_ordinal = info->library_ordinal();
          import.weak_import = info->is_weak_import();
          import.name_offset = name_offset;
          imports.write(import);
          break;
        }
      case DYLD_CHAINED_FORMAT::IMPORT_ADDEND:
        {
          details::dyld_chained_import_addend import;
          import.lib_ordinal = info->library_ordinal();
          import.weak_import = info->is_weak_import();
          import.name_offset = name_offset;
          import.addend      = info->addend();
          imports_addend.write(import);
          break;
        }
      case DYLD_CHAINED_FORMAT::IMPORT_ADDEND64:
        {
          details::dyld_chained_import_addend64 import;
          import.lib_ordinal = info->library_ordinal();
          import.weak_import = info->is_weak_import();
          import.name_offset = name_offset;
          import.addend      = info->addend();
          imports_addend64.write(import);
          break;
        }
    }

    for (ChainedBindingInfo* binding : info->elements_) {
      const uint64_t rel_offset = binding->offset_ - binding->segment()->file_offset();
      uint8_t* data_ptr = binding->segment_->writable_content().data() + rel_offset;
      LIEF_DEBUG("Write binding (offset=0x{:010x}): 0x{:016x} {} in {} offset=0x{:010x}",
                 binding->offset_, binding->address(), binding->symbol()->name(),
                 binding->segment_->name(),
                 binding->segment()->file_offset() + rel_offset);

      // Rewrite the raw chained binding
      switch (binding->btypes_) {
        case ChainedBindingInfo::BIND_TYPES::ARM64E_BIND:
          {
            auto& raw_bind = *reinterpret_cast<details::dyld_chained_ptr_arm64e*>(data_ptr);
            raw_bind.bind = *binding->arm64_bind_;
            break;
          }
        case ChainedBindingInfo::BIND_TYPES::ARM64E_AUTH_BIND:
          {
            auto& raw_bind = *reinterpret_cast<details::dyld_chained_ptr_arm64e*>(data_ptr);
            raw_bind.auth_bind = *binding->arm64_auth_bind_;
            break;
          }
        case ChainedBindingInfo::BIND_TYPES::ARM64E_BIND24:
          {
            auto& raw_bind = *reinterpret_cast<details::dyld_chained_ptr_arm64e*>(data_ptr);
            raw_bind.bind24 = *binding->arm64_bind24_;
            break;
          }
        case ChainedBindingInfo::BIND_TYPES::ARM64E_AUTH_BIND24:
          {
            auto& raw_bind = *reinterpret_cast<details::dyld_chained_ptr_arm64e*>(data_ptr);
            raw_bind.auth_bind24 = *binding->arm64_auth_bind24_;
            break;
          }
        case ChainedBindingInfo::BIND_TYPES::PTR64_BIND:
          {
            auto& raw_bind = *reinterpret_cast<details::dyld_chained_ptr_generic64*>(data_ptr);
            raw_bind.bind = *binding->p64_bind_;
            break;
          }
        case ChainedBindingInfo::BIND_TYPES::PTR32_BIND:
          {
            auto& raw_bind = *reinterpret_cast<details::dyld_chained_ptr_generic32*>(data_ptr);
            raw_bind.bind = *binding->p32_bind_;
            break;
          }
        case ChainedBindingInfo::BIND_TYPES::UNKNOWN: break;
      }
    }
  }

  // Re-access the header to update the offsets
  // NOTE/WARN: We need to 'reset' the pointer as the underlying buffer (std::vector) might
  // be relocated.
  auto* hdr = reinterpret_cast<details::dyld_chained_fixups_header*>(lnk_data.raw().data());
  switch (fmt) {
    case DYLD_CHAINED_FORMAT::IMPORT:
      {
        lnk_data.align(4);
        hdr = reinterpret_cast<details::dyld_chained_fixups_header*>(lnk_data.raw().data());
        hdr->imports_offset = lnk_data.size();
        lnk_data.write(imports);
        break;
      }
    case DYLD_CHAINED_FORMAT::IMPORT_ADDEND:
      {
        lnk_data.align(4);
        hdr = reinterpret_cast<details::dyld_chained_fixups_header*>(lnk_data.raw().data());
        hdr->imports_offset = lnk_data.size();
        lnk_data.write(imports_addend);
        break;
      }
    case DYLD_CHAINED_FORMAT::IMPORT_ADDEND64:
      {
        lnk_data.align(8);
        hdr = reinterpret_cast<details::dyld_chained_fixups_header*>(lnk_data.raw().data());
        hdr->imports_offset = lnk_data.size();
        lnk_data.write(imports_addend64);
        break;
      }
  }

  hdr = reinterpret_cast<details::dyld_chained_fixups_header*>(lnk_data.raw().data());
  hdr->symbols_offset = lnk_data.size();
  lnk_data
    .write(string_pool.raw())
    .align(sizeof(pin_t));

  const std::vector<uint8_t>& raw = lnk_data.raw();
  LIEF_DEBUG("__chainfixups.old_size: 0x{:06x}", fixups.data_size());
  LIEF_DEBUG("__chainfixups.new_size: 0x{:06x}", raw.size());

  if (fixups.data_size() > 0 && fixups.data_size() < raw.size()) {
    LIEF_WARN("New chained fixups size is larger than the original one");
  }

  LIEF_DEBUG("LC_DYLD_CHAINED_FIXUPS.offset: 0x{:06x} -> 0x{:x}",
             fixups.data_offset(), linkedit_offset_ + linkedit_.size());
  LIEF_DEBUG("LC_DYLD_CHAINED_FIXUPS.size:   0x{:06x} -> 0x{:x}",
             fixups.data_size(), lnk_data.size());

  // Write back the 'linkedit' structure
  details::linkedit_data_command raw_cmd;

  raw_cmd.cmd       = static_cast<uint32_t>(fixups.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(fixups.size());
  raw_cmd.dataoff   = linkedit_offset_ + linkedit_.size();
  raw_cmd.datasize  = lnk_data.size();

  linkedit_.write(std::move(lnk_data.raw()));

  fixups.size_ = sizeof(details::linkedit_data_command);
  fixups.original_data_.clear();
  fixups.original_data_.resize(fixups.size_);

  memcpy(fixups.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<typename T>
ok_error_t Builder::build(DyldExportsTrie& exports) {
  using pin_t = typename T::uint;
  std::vector<uint8_t> raw = create_trie(exports.export_info_, sizeof(pin_t));

  if (exports.data_size() > 0 && raw.size() > exports.content_.size()) {
    const uint64_t delta = raw.size() - exports.content_.size();
    LIEF_INFO("The export trie is larger than the original "
              "LC_DYLD_EXPORTS_TRIE (+0x{:x} bytes)", delta);
  }

  LIEF_DEBUG("LC_DYLD_EXPORTS_TRIE.offset: 0x{:06x} -> 0x{:x}",
             exports.data_offset(), linkedit_offset_ + linkedit_.size());
  LIEF_DEBUG("LC_DYLD_EXPORTS_TRIE.size:   0x{:06x} -> 0x{:x}",
             exports.data_size(), raw.size());

  // Write back the 'linkedit' structure
  details::linkedit_data_command raw_cmd;

  raw_cmd.cmd       = static_cast<uint32_t>(exports.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(exports.size());
  raw_cmd.dataoff   = linkedit_offset_ + linkedit_.size();
  raw_cmd.datasize  = raw.size();

  linkedit_.write(std::move(raw));

  exports.size_ = sizeof(details::linkedit_data_command);
  exports.original_data_.clear();
  exports.original_data_.resize(exports.size_);

  memcpy(exports.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(BuildVersion& bv) {
  details::build_version_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::build_version_command));

  const BuildVersion::tools_list_t& tools = bv.tools();

  const uint32_t raw_size    = sizeof(details::build_version_command) + tools.size() * sizeof(details::build_tool_version);
  const uint32_t size_needed = align(raw_size, sizeof(typename T::uint));
  const uint32_t padding     = size_needed - raw_size;

  if (bv.original_data_.size() < size_needed || bv.size() < size_needed) {
    LIEF_WARN("Not enough spaces to rebuild 'BuildVersion'. Size required: 0x{:x} vs 0x{:x}",
               bv.original_data_.size(), size_needed);
  }

  const BuildVersion::version_t& minos    = bv.minos();
  const BuildVersion::version_t& sdk      = bv.sdk();

  raw_cmd.cmd      = static_cast<uint32_t>(bv.command());
  raw_cmd.cmdsize  = static_cast<uint32_t>(size_needed);

  raw_cmd.minos    = static_cast<uint32_t>(minos[0] << 16 | minos[1] << 8 | minos[2]);
  raw_cmd.sdk      = static_cast<uint32_t>(sdk[0] << 16 | sdk[1] << 8 | sdk[2]);
  raw_cmd.platform = static_cast<uint32_t>(bv.platform());
  raw_cmd.ntools   = tools.size();
  //raw_cmd.name     = static_cast<uint32_t>(sizeof(build_version_command));
  std::vector<uint8_t> raw_tools(raw_cmd.ntools * sizeof(details::build_tool_version), 0);
  auto* tools_array = reinterpret_cast<details::build_tool_version*>(raw_tools.data());
  for (size_t i = 0; i < tools.size(); ++i) {
    BuildToolVersion::version_t version = tools[i].version();
    tools_array[i].tool    = static_cast<uint32_t>(tools[i].tool());
    tools_array[i].version = static_cast<uint32_t>(version[0] << 16 | version[1] << 8 | version[2]);
  }

  bv.size_ = size_needed;
  bv.original_data_.clear();

  // Write Header
  std::move(reinterpret_cast<uint8_t*>(&raw_cmd), reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(raw_cmd),
            std::back_inserter(bv.original_data_));

  std::move(std::begin(raw_tools), std::end(raw_tools), std::back_inserter(bv.original_data_));

  bv.original_data_.insert(std::end(bv.original_data_), padding, 0);
  return ok();
}

template<class T>
ok_error_t Builder::build(CodeSignatureDir& sig) {
  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));

  span<const uint8_t> sp = sig.content();

  raw_cmd.cmd       = static_cast<uint32_t>(sig.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(sig.size());
  raw_cmd.dataoff   = linkedit_offset_ + linkedit_.size();
  raw_cmd.datasize  = sp.size();

  LIEF_DEBUG("LC_DYLIB_CODE_SIGN_DRS.offset: 0x{:06x} -> 0x{:x}",
             sig.data_offset(), raw_cmd.dataoff);
  LIEF_DEBUG("LC_DYLIB_CODE_SIGN_DRS.size:   0x{:06x} -> 0x{:x}",
             sig.data_size(), raw_cmd.datasize);

  linkedit_.write(sp.data(), sp.size());

  sig.size_ = sizeof(details::linkedit_data_command);
  sig.original_data_.clear();
  sig.original_data_.resize(sig.size_);

  memcpy(sig.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(LinkerOptHint& opt) {
  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));

  span<const uint8_t> sp = opt.content();

  raw_cmd.cmd       = static_cast<uint32_t>(opt.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(opt.size());
  raw_cmd.dataoff   = linkedit_offset_ + linkedit_.size();
  raw_cmd.datasize  = sp.size();

  LIEF_DEBUG("LC_LINKER_OPTIMIZATION_HINT.offset: 0x{:06x} -> 0x{:x}",
             opt.data_offset(), raw_cmd.dataoff);
  LIEF_DEBUG("LC_LINKER_OPTIMIZATION_HINT.size:   0x{:06x} -> 0x{:x}",
             opt.data_size(), raw_cmd.datasize);

  linkedit_.write(sp.data(), sp.size());

  opt.size_ = sizeof(details::linkedit_data_command);
  opt.original_data_.clear();
  opt.original_data_.resize(opt.size_);

  memcpy(opt.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(AtomInfo& atom) {
  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));

  span<const uint8_t> sp = atom.content();

  raw_cmd.cmd       = static_cast<uint32_t>(atom.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(atom.size());
  raw_cmd.dataoff   = linkedit_offset_ + linkedit_.size();
  raw_cmd.datasize  = sp.size();

  LIEF_DEBUG("LC_ATOM_INFO.offset: 0x{:06x} -> 0x{:x}",
             atom.data_offset(), raw_cmd.dataoff);
  LIEF_DEBUG("LC_ATOM_INFO.size:   0x{:06x} -> 0x{:x}",
             atom.data_size(), raw_cmd.datasize);

  linkedit_.write(sp.data(), sp.size());

  atom.size_ = sizeof(details::linkedit_data_command);
  atom.original_data_.clear();
  atom.original_data_.resize(atom.size_);

  memcpy(atom.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(TwoLevelHints& two) {
  return make_error_code(lief_errors::not_implemented);
  details::twolevel_hints_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));
  const auto it_hints = two.hints();

  raw_cmd.cmd       = static_cast<uint32_t>(two.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(two.size());

  raw_cmd.offset  = linkedit_offset_ + linkedit_.size();
  raw_cmd.nhints  = it_hints.size();

  LIEF_DEBUG("LC_TWOLEVEL_HINTS.offset: 0x{:06x} -> 0x{:x}",
             two.offset(), raw_cmd.offset);
  LIEF_DEBUG("LC_TWOLEVEL_HINTS.nhints: 0x{:06x} -> 0x{:x}",
             two.original_nb_hints(), raw_cmd.nhints);

  for (uint32_t value : it_hints) {
    linkedit_.write(value);
  }

  two.size_ = sizeof(details::linkedit_data_command);
  two.original_data_.clear();
  two.original_data_.resize(two.size_);

  memcpy(two.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(FunctionVariants& func_variants) {
  LIEF_DEBUG("Build '{}'", to_string(func_variants.command()));
  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));
  raw_cmd.dataoff = linkedit_.size();

  span<const uint8_t> sp = func_variants.content();

  // TODO(romain): We need to reconstruct the data in depth
  linkedit_.write(sp);

  raw_cmd.cmd       = static_cast<uint32_t>(func_variants.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(func_variants.size());
  raw_cmd.datasize  = linkedit_.size() - raw_cmd.dataoff;
  raw_cmd.dataoff   += linkedit_offset_;

  LIEF_DEBUG("LC_FUNCTION_VARIANTS.offset: 0x{:06x} -> 0x{:x}",
             func_variants.data_offset(), raw_cmd.dataoff);
  LIEF_DEBUG("LC_FUNCTION_VARIANTS.size:   0x{:06x} -> 0x{:x}",
             func_variants.data_size(), raw_cmd.datasize);

  func_variants.size_ = sizeof(details::linkedit_data_command);
  func_variants.original_data_.clear();
  func_variants.original_data_.resize(func_variants.size_);
  memcpy(func_variants.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}

template<class T>
ok_error_t Builder::build(FunctionVariantFixups& func_variant_fixups) {
  LIEF_DEBUG("Build '{}'", to_string(func_variant_fixups.command()));
  details::linkedit_data_command raw_cmd;
  std::memset(&raw_cmd, 0, sizeof(details::linkedit_data_command));
  raw_cmd.dataoff = linkedit_.size();

  // TODO(romain): We need to reconstruct the data in depth
  linkedit_.write(func_variant_fixups.content());

  linkedit_.align(sizeof(typename T::uint));

  raw_cmd.cmd       = static_cast<uint32_t>(func_variant_fixups.command());
  raw_cmd.cmdsize   = static_cast<uint32_t>(func_variant_fixups.size());
  raw_cmd.datasize  = linkedit_.size() - raw_cmd.dataoff;
  raw_cmd.dataoff   += linkedit_offset_;


  LIEF_DEBUG("LC_FUNCTION_VARIANT_FIXUPS.offset: 0x{:06x} -> 0x{:x}",
             func_variant_fixups.data_offset(), raw_cmd.dataoff);
  LIEF_DEBUG("LC_FUNCTION_VARIANT_FIXUPS.size:   0x{:06x} -> 0x{:x}",
             func_variant_fixups.data_size(), raw_cmd.datasize);

  func_variant_fixups.size_ = sizeof(details::linkedit_data_command);
  func_variant_fixups.original_data_.clear();
  func_variant_fixups.original_data_.resize(func_variant_fixups.size_);
  memcpy(func_variant_fixups.original_data_.data(), &raw_cmd, sizeof(details::linkedit_data_command));
  return ok();
}
template<class MACHO_T>
ok_error_t Builder::build_header() {
  using header_t = typename MACHO_T::header;

  header_t header;
  std::memset(&header, 0, sizeof(header_t));

  const Header& binary_header = binary_->header();

  header.magic      = static_cast<uint32_t>(binary_header.magic());
  header.cputype    = static_cast<uint32_t>(binary_header.cpu_type());
  header.cpusubtype = static_cast<uint32_t>(binary_header.cpu_subtype());
  header.filetype   = static_cast<uint32_t>(binary_header.file_type());
  header.ncmds      = static_cast<uint32_t>(binary_header.nb_cmds());
  header.sizeofcmds = static_cast<uint32_t>(binary_header.sizeof_cmds());
  header.flags      = static_cast<uint32_t>(binary_header.flags());

  if constexpr (std::is_same_v<header_t, details::mach_header_64>) {
    header.reserved = static_cast<uint32_t>(binary_header.reserved());
  }

  LIEF_DEBUG("Writing header at: 0 (size: 0x{:04x})", sizeof(header));

  raw_
    .seekp(0)
    .write(header);

  return ok();

}
}
}
