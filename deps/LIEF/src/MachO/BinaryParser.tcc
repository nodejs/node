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

#include <memory>

#include "logging.hpp"
#include "internal_utils.hpp"

#include "MachO/ChainedFixup.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/BinaryStream/MemoryStream.hpp"

#include "LIEF/MachO/AtomInfo.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/ChainedPointerAnalysis.hpp"
#include "LIEF/MachO/BinaryParser.hpp"
#include "LIEF/MachO/BuildVersion.hpp"
#include "LIEF/MachO/ChainedBindingInfo.hpp"
#include "LIEF/MachO/CodeSignature.hpp"
#include "LIEF/MachO/CodeSignatureDir.hpp"
#include "LIEF/MachO/DataInCode.hpp"
#include "LIEF/MachO/DyldBindingInfo.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/DyldEnvironment.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DylibCommand.hpp"
#include "LIEF/MachO/DylinkerCommand.hpp"
#include "LIEF/MachO/DynamicSymbolCommand.hpp"
#include "LIEF/MachO/EncryptionInfo.hpp"
#include "LIEF/MachO/FilesetCommand.hpp"
#include "LIEF/MachO/FunctionStarts.hpp"
#include "LIEF/MachO/FunctionVariants.hpp"
#include "LIEF/MachO/FunctionVariantFixups.hpp"
#include "LIEF/MachO/IndirectBindingInfo.hpp"
#include "LIEF/MachO/LinkEdit.hpp"
#include "LIEF/MachO/LinkerOptHint.hpp"
#include "LIEF/MachO/MainCommand.hpp"
#include "LIEF/MachO/NoteCommand.hpp"
#include "LIEF/MachO/RPathCommand.hpp"
#include "LIEF/MachO/Relocation.hpp"
#include "LIEF/MachO/RelocationDyld.hpp"
#include "LIEF/MachO/RelocationFixup.hpp"
#include "LIEF/MachO/RelocationObject.hpp"
#include "LIEF/MachO/Routine.hpp"
#include "LIEF/MachO/Section.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/SegmentSplitInfo.hpp"
#include "LIEF/MachO/SourceVersion.hpp"
#include "LIEF/MachO/SubClient.hpp"
#include "LIEF/MachO/SubFramework.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/SymbolCommand.hpp"
#include "LIEF/MachO/ThreadCommand.hpp"
#include "LIEF/MachO/TwoLevelHints.hpp"
#include "LIEF/MachO/UUIDCommand.hpp"
#include "LIEF/MachO/UnknownCommand.hpp"
#include "LIEF/MachO/VersionMin.hpp"

#include "MachO/Structures.hpp"
#include "MachO/ChainedFixup.hpp"
#include "MachO/ChainedBindingInfoList.hpp"

#include "Object.tcc"


namespace LIEF {
namespace MachO {

static constexpr uint8_t BYTE_BITS = std::numeric_limits<uint8_t>::digits;
static_assert(BYTE_BITS == 8, "The number of bits in a byte is not 8");

namespace {
struct ThreadedBindData {
  std::string symbol_name;
  int64_t addend          = 0;
  int64_t library_ordinal = 0;
  uint8_t symbol_flags    = 0;
  uint8_t type            = 0;
};
}

template<class MACHO_T>
ok_error_t BinaryParser::parse() {
  parse_header<MACHO_T>();
  if (binary_->header().nb_cmds() > 0) {
    parse_load_commands<MACHO_T>();
  }

  /*
   * We must perform this post-processing BEFORE parsing
   * the exports trie as it could create new symbols and break the DynamicSymbolCommand's indexes
   */
  if (SymbolCommand* symtab = binary_->symbol_command()) {
    post_process<MACHO_T>(*symtab);
  }

  if (DynamicSymbolCommand* dynsym = binary_->dynamic_symbol_command()) {
    post_process<MACHO_T>(*dynsym);
  }

  for (Section& section : binary_->sections()) {
    parse_relocations<MACHO_T>(section);
  }

  if (binary_->has_dyld_info()) {

    if (config_.parse_dyld_exports) {
      parse_dyldinfo_export();
    }

    if (config_.parse_dyld_bindings) {
      parse_dyldinfo_binds<MACHO_T>();
    }

    if (config_.parse_dyld_rebases) {
      parse_dyldinfo_rebases<MACHO_T>();
    }
  }

  if (config_.parse_dyld_exports && binary_->has_dyld_exports_trie()) {
    parse_dyld_exports();
  }

  if (SegmentCommand* seg = binary_->get_segment("__LINKEDIT")) {
    LinkEdit& linkedit = *static_cast<LinkEdit*>(seg);

    // Backtrack the objects in the segment to keep span consistent
    if (DyldInfo* dyld = binary_->dyld_info()) {
      linkedit.dyld_ = dyld;
    }

    if (DyldChainedFixups* fixups = binary_->dyld_chained_fixups()) {
      linkedit.chained_fixups_ = fixups;
    }
  }


  if (DyldChainedFixups* fixups = binary_->dyld_chained_fixups()) {
    LIEF_DEBUG("[+] Parsing LC_DYLD_CHAINED_FIXUPS payload");
    SpanStream stream = fixups->content_;
    stream.set_endian_swap(stream_->should_swap());
    chained_fixups_ = fixups;
    auto is_ok = parse_chained_payload<MACHO_T>(stream);
    if (!is_ok) {
      LIEF_WARN("Error while parsing the payload of LC_DYLD_CHAINED_FIXUPS");
    }
  }

  /*
   * Create the slices for the LinkEdit commands
   */
  if (FunctionStarts* fstart = binary_->function_starts()) {
    post_process<MACHO_T>(*fstart);
  }
  if (DataInCode* data_code = binary_->data_in_code()) {
    post_process<MACHO_T>(*data_code);
  }
  if (SegmentSplitInfo* split = binary_->segment_split_info()) {
    post_process<MACHO_T>(*split);
  }
  if (TwoLevelHints* two = binary_->two_level_hints()) {
    post_process<MACHO_T>(*two);
  }
  if (CodeSignature* sig = binary_->code_signature()) {
    post_process<MACHO_T>(*sig);
  }
  if (CodeSignatureDir* sig = binary_->code_signature_dir()) {
    post_process<MACHO_T>(*sig);
  }
  if (LinkerOptHint* opt = binary_->linker_opt_hint()) {
    post_process<MACHO_T>(*opt);
  }
  if (AtomInfo* info = binary_->atom_info()) {
    post_process<MACHO_T>(*info);
  }
  if (FunctionVariants* variants = binary_->function_variants()) {
    post_process<MACHO_T>(*variants);
  }
  if (FunctionVariantFixups* fixups = binary_->function_variant_fixups()) {
    post_process<MACHO_T>(*fixups);
  }

  if (binary_->dyld_info() == nullptr &&
      binary_->dyld_chained_fixups() == nullptr)
  {
    infer_indirect_bindings<MACHO_T>();
  }

  if (config_.parse_overlay) {
    parse_overlay();
  }

  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::parse_header() {
  using header_t = typename MACHO_T::header;
  auto hdr = stream_->read<header_t>();
  if (!hdr) {
    LIEF_ERR("Can't read the Mach-O header");
    return make_error_code(lief_errors::parsing_error);
  }
  binary_->header_ = std::move(*hdr);
  LIEF_DEBUG("Arch:     {}", to_string(binary_->header_.cpu_type()));
  LIEF_DEBUG("Commands: #{}", binary_->header().nb_cmds());
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::parse_load_commands() {
  using segment_command_t = typename MACHO_T::segment_command;
  using section_t         = typename MACHO_T::section;

  LIEF_DEBUG("[+] Building Load commands");

  const Header& header = binary_->header();
  uint64_t loadcommands_offset = stream_->pos();

  if ((loadcommands_offset + header.sizeof_cmds()) > stream_->size()) {
    LIEF_ERR("Load commands are corrupted");
    return make_error_code(lief_errors::corrupted);
  }

  size_t nbcmds = header.nb_cmds();

  if (header.nb_cmds() > BinaryParser::MAX_COMMANDS) {
    nbcmds = BinaryParser::MAX_COMMANDS;
    LIEF_WARN("Only the first #{:d} will be parsed", nbcmds);
  }

  // Base address of the MachO file that is
  // set as soon as we can
  uint32_t low_fileoff = -1U;
  std::set<LoadCommand::TYPE> not_parsed;
  int64_t imagebase = -1;

  for (size_t i = 0; i < nbcmds; ++i) {
    const auto command = stream_->peek<details::load_command>(loadcommands_offset);
    if (!command) {
      break;
    }

    std::unique_ptr<LoadCommand> load_command;
    const auto cmd_type = static_cast<LoadCommand::TYPE>(command->cmd);

    LIEF_DEBUG("Parsing command #{:02d}: {} (0x{:04x})",
               i, to_string(cmd_type), (uint64_t)cmd_type);

    switch (cmd_type) {

      // ===============
      // Segment command
      // ===============
      case LoadCommand::TYPE::SEGMENT_64:
      case LoadCommand::TYPE::SEGMENT:
        {
          /*
           * DO NOT FORGET TO UPDATE SegmentCommand::classof
           */
          uint64_t local_offset = loadcommands_offset;
          const auto segment_cmd = stream_->peek<segment_command_t>(loadcommands_offset);
          if (!segment_cmd) {
            LIEF_ERR("Can't parse segment command #{}", i);
            break;
          }

          if (std::string(segment_cmd->segname, 10) == "__LINKEDIT") {
            load_command = std::make_unique<LinkEdit>(*segment_cmd);
          } else {
            load_command = std::make_unique<SegmentCommand>(*segment_cmd);
          }

          local_offset += sizeof(segment_command_t);

          auto* segment = load_command->as<SegmentCommand>();
          segment->index_ = binary_->segments_.size();
          binary_->segments_.push_back(segment);

          if (Binary::can_cache_segment(*segment)) {
            binary_->offset_seg_[segment->file_offset()] = segment;
          }

          if (segment->name() == "__TEXT" && imagebase < 0) {
            imagebase = segment->virtual_address();
          }

          if (MemoryStream::classof(*stream_)) {
            // Link the memory stream with our
            // binary object so that is can translate virtual address to offset
            static_cast<MemoryStream&>(*stream_).binary(*binary_);
          }

          if (segment->file_size() > 0) {
            if (MemoryStream::classof(*stream_)) {
              auto& memstream = static_cast<MemoryStream&>(*stream_);
              uintptr_t segment_va = segment->virtual_address();
              if (imagebase >= 0 && segment_va >= static_cast<uint64_t>(imagebase)) {
                segment_va -= static_cast<uint64_t>(imagebase);
              }

              uintptr_t address = memstream.base_address() + segment_va;
              const auto* p = reinterpret_cast<const uint8_t*>(address);
              segment->data_ = {p, p + segment->file_size()};
            } else {
              if (!stream_->peek_data(segment->data_,
                                      segment->file_offset(),
                                      segment->file_size(),
                                      segment->virtual_address()))
              {
                LIEF_ERR("Segment {}: content corrupted!", segment->name());
              }
            }
          }

          // --------
          // Sections
          // --------
          for (size_t j = 0; j < segment->numberof_sections(); ++j) {
            const auto section_header = stream_->peek<section_t>(local_offset);
            if (!section_header) {
              LIEF_ERR("Can't parse section in {}#{}",
                       load_command->as<SegmentCommand>()->name(), i);
              break;
            }
            auto section = std::make_unique<Section>(*section_header);

            binary_->sections_.push_back(section.get());
            if (section->size_ > 0 &&
                section->type() != Section::TYPE::ZEROFILL &&
                section->type() != Section::TYPE::THREAD_LOCAL_ZEROFILL &&
                section->offset_ < low_fileoff)
            {
              low_fileoff = section->offset_;
            }
            section->segment_ = segment;
            segment->sections_.push_back(std::move(section));
            local_offset += sizeof(section_t);
          }
          if (segment->numberof_sections() == 0 &&
              segment->file_offset() != 0 &&
              segment->file_size() != 0 &&
              segment->file_offset() < low_fileoff)
          {
            low_fileoff = segment->file_offset();
          }
          break;
        }


      // =============
      // DyLib Command
      // =============
      case LoadCommand::TYPE::LOAD_WEAK_DYLIB:
      case LoadCommand::TYPE::ID_DYLIB:
      case LoadCommand::TYPE::LOAD_DYLIB:
      case LoadCommand::TYPE::REEXPORT_DYLIB:
      case LoadCommand::TYPE::LOAD_UPWARD_DYLIB:
      case LoadCommand::TYPE::LAZY_LOAD_DYLIB:
        {
          /*
           * DO NOT FORGET TO UPDATE DylibCommand::classof
           */
          const auto cmd = stream_->peek<details::dylib_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't read dylib_command");
            break;
          }

          load_command = std::make_unique<DylibCommand>(*cmd);
          const uint32_t str_name_offset = cmd->name;
          auto name = stream_->peek_string_at(loadcommands_offset + str_name_offset);
          if (!name) {
            LIEF_ERR("Can't read Dylib string value");
            break;
          }

          auto* lib = load_command->as<DylibCommand>();
          lib->name(*name);
          binary_->libraries_.push_back(lib);
          if (cmd_type != LoadCommand::TYPE::ID_DYLIB) {
            binding_libs_.push_back(lib);
          }
          break;
        }

      // =============
      // RPath Command
      // =============
      case LoadCommand::TYPE::RPATH:
        {
          /*
           * DO NOT FORGET TO UPDATE RPathCommand::classof
           */
          const auto cmd = stream_->peek<details::rpath_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't read rpath_command");
            break;
          }

          load_command = std::make_unique<RPathCommand>(*cmd);
          const uint32_t str_path_offset = cmd->path;
          auto path = stream_->peek_string_at(loadcommands_offset + str_path_offset);
          if (!path) {
            LIEF_ERR("Can't read rpath_command.path");
            break;
          }

          load_command->as<RPathCommand>()->path(*path);
          break;
        }

      // ====
      // UUID
      // ====
      case LoadCommand::TYPE::UUID:
        {
          /*
           * DO NOT FORGET TO UPDATE UUIDCommand::classof
           */
          const auto cmd = stream_->peek<details::uuid_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't read uuid_command");
            break;
          }
          load_command = std::make_unique<UUIDCommand>(*cmd);
          break;
        }

      // ==============
      // Dynamic Linker
      // ==============
      case LoadCommand::TYPE::LOAD_DYLINKER:
      case LoadCommand::TYPE::ID_DYLINKER:
        {
          /*
           * DO NOT FORGET TO UPDATE DylinkerCommand::classof
           */
          const auto cmd = stream_->peek<details::dylinker_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't read dylinker_command");
            break;
          }

          load_command = std::make_unique<DylinkerCommand>(*cmd);

          const uint32_t linker_name_offset = cmd->name;
          auto name = stream_->peek_string_at(loadcommands_offset + linker_name_offset);
          if (!name) {
            LIEF_ERR("Can't read dylinker_command.name");
            break;
          }
          load_command->as<DylinkerCommand>()->name(*name);
          break;
        }

      // ======
      // Thread
      // ======
      case LoadCommand::TYPE::THREAD:
      case LoadCommand::TYPE::UNIXTHREAD:
        {
          /*
           * DO NOT FORGET TO UPDATE ThreadCommand::classof
           */
          const auto cmd = stream_->peek<details::thread_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't read thread_command");
            break;
          }
          load_command = std::make_unique<ThreadCommand>(*cmd);

          auto* thread = load_command->as<ThreadCommand>();
          thread->architecture_ = binary_->header().cpu_type();
          LIEF_DEBUG("FLAVOR: {} | COUNT: {}", cmd->flavor, cmd->count);
          const uint64_t state_offset = loadcommands_offset + sizeof(details::thread_command);
          const Header::CPU_TYPE arch = binary_->header().cpu_type();
          switch (arch) {
            case Header::CPU_TYPE::X86:
              {
                if (!stream_->peek_data(thread->state_, state_offset,
                      sizeof(details::x86_thread_state_t)))
                {
                  LIEF_ERR("Can't read the state data");
                }
                break;
              }

            case Header::CPU_TYPE::X86_64:
              {
                if (!stream_->peek_data(thread->state_, state_offset,
                      sizeof(details::x86_thread_state64_t)))
                {
                  LIEF_ERR("Can't read the state data");
                }
                break;
              }

            case Header::CPU_TYPE::ARM:
              {
                if (!stream_->peek_data(thread->state_, state_offset,
                      sizeof(details::arm_thread_state_t)))
                {
                  LIEF_ERR("Can't read the state data");
                }
                break;
              }

            case Header::CPU_TYPE::ARM64:
              {
                if (!stream_->peek_data(thread->state_, state_offset,
                      sizeof(details::arm_thread_state64_t)))
                {
                  LIEF_ERR("Can't read the state data");
                }
                break;
              }

            case Header::CPU_TYPE::POWERPC:
              {
                if (!stream_->peek_data(thread->state_, state_offset,
                     sizeof(details::ppc_thread_state_t)))
                {
                  LIEF_ERR("Can't read the state data");
                }
                break;
              }

            case Header::CPU_TYPE::POWERPC64:
              {
                if (!stream_->peek_data(thread->state_, state_offset,
                     sizeof(details::ppc_thread_state64_t)))
                {
                  LIEF_ERR("Can't read the state data");
                }
                break;
              }

            default:
              {
                static std::set<int32_t> ARCH_ERR;
                if (ARCH_ERR.insert((int32_t)arch).second) {
                  LIEF_ERR("Unknown architecture ({})", (int32_t)arch);
                }
              }
          }
          break;
        }

      // ===============
      // Routine command
      // ===============
      case LoadCommand::TYPE::ROUTINES_64:
        {
          const auto cmd = stream_->peek<details::routines_command_64>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't read routines_command_64");
            break;
          }
          load_command = std::make_unique<Routine>(*cmd);
          break;
        }

      case LoadCommand::TYPE::ROUTINES:
        {
          const auto cmd = stream_->peek<details::routines_command_32>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't read routines_command_32");
            break;
          }
          load_command = std::make_unique<Routine>(*cmd);
          break;
        }

      // =============
      // Symbols table
      // =============
      case LoadCommand::TYPE::SYMTAB:
        {
          /*
           * DO NOT FORGET TO UPDATE SymbolCommand::classof
           */
          LIEF_DEBUG("[+] Parsing symbols");

          const auto cmd = stream_->peek<details::symtab_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't read symtab_command");
            break;
          }


          LIEF_DEBUG("LC_SYMTAB.symoff:  0x{:016x}", cmd->symoff);
          LIEF_DEBUG("LC_SYMTAB.nsyms:   0x{:016x}", cmd->nsyms);
          LIEF_DEBUG("LC_SYMTAB.stroff:  0x{:016x}", cmd->stroff);
          LIEF_DEBUG("LC_SYMTAB.strsize: 0x{:016x}", cmd->strsize);

          load_command = std::make_unique<SymbolCommand>(*cmd);
          break;
        }

      // ===============
      // Dynamic Symbols
      // ===============
      case LoadCommand::TYPE::DYSYMTAB:
        {
          /*
           * DO NOT FORGET TO UPDATE DynamicSymbolCommand::classof
           */
          LIEF_DEBUG("[+] Parsing dynamic symbols");
          const auto cmd = stream_->peek<details::dysymtab_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse dysymtab_command");
            break;
          }
          load_command = std::make_unique<DynamicSymbolCommand>(*cmd);
          break;
        }

      // ===============
      // Dyd Info
      // ===============
      case LoadCommand::TYPE::DYLD_INFO:
      case LoadCommand::TYPE::DYLD_INFO_ONLY:
        {
          /*
           * DO NOT FORGET TO UPDATE DyldInfo::classof
           */
          LIEF_DEBUG("[+] Parsing dyld information");
          const auto cmd = stream_->peek<details::dyld_info_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse dyld_info_command");
            break;
          }
          load_command = std::make_unique<DyldInfo>(*cmd);
          load_command->as<DyldInfo>()->binary_ = binary_.get();
          break;
        }

      // ===============
      // Source Version
      // ===============
      case LoadCommand::TYPE::SOURCE_VERSION:
        {
          /*
           * DO NOT FORGET TO UPDATE SourceVersion::classof
           */
          LIEF_DEBUG("[+] Parsing LC_SOURCE_VERSION");

          const auto cmd = stream_->peek<details::source_version_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse source_version_command");
            break;
          }
          load_command = std::make_unique<SourceVersion>(*cmd);
          LIEF_DEBUG("Version: 0x{:x}", cmd->version);
          break;
        }

      case LoadCommand::TYPE::VERSION_MIN_MACOSX:
      case LoadCommand::TYPE::VERSION_MIN_IPHONEOS:
      case LoadCommand::TYPE::VERSION_MIN_TVOS:
      case LoadCommand::TYPE::VERSION_MIN_WATCHOS:
        {
          /*
           * DO NOT FORGET TO UPDATE VersionMin::classof
           */
          LIEF_DEBUG("[+] Parsing {}", to_string(cmd_type));

          const auto cmd = stream_->peek<details::version_min_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse version_min_command");
            break;
          }
          LIEF_DEBUG("Version: 0x{:x} | SDK: 0x{:x}", cmd->version, cmd->sdk);

          load_command = std::make_unique<VersionMin>(*cmd);
          break;
        }


      case LoadCommand::TYPE::BUILD_VERSION:
        {
          /*
           * DO NOT FORGET TO UPDATE BuildVersion::classof
           */
          LIEF_DEBUG("[+] Parsing {}", to_string(cmd_type));

          const auto cmd = stream_->peek<details::build_version_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse build_version_command");
            break;
          }

          load_command = std::make_unique<BuildVersion>(*cmd);
          auto* build_version = load_command->as<BuildVersion>();
          for (size_t i = 0; i < cmd->ntools; ++i) {
            const uint64_t cmd_offset = loadcommands_offset + sizeof(details::build_version_command) +
                                        i * sizeof(details::build_tool_version);

            auto tool_struct = stream_->peek<details::build_tool_version>(cmd_offset);
            if (!tool_struct) {
              break;
            }
            build_version->tools_.emplace_back(*tool_struct);
          }
          break;
        }

      // =================
      // Code Signature
      // =================
      case LoadCommand::TYPE::DYLIB_CODE_SIGN_DRS:
        {
          /*
           * DO NOT FORGET TO UPDATE CodeSignatureDir::classof
           */
          if (auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset)) {
            load_command = std::make_unique<CodeSignatureDir>(*cmd);
          } else {
            LIEF_ERR("Can't parse linkedit_data_command for LC_DYLIB_CODE_SIGN_DRS");
          }
          break;
        }

      case LoadCommand::TYPE::CODE_SIGNATURE:
        {
          /*
           * DO NOT FORGET TO UPDATE CodeSignature::classof
           */
          if (auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset)) {
            load_command = std::make_unique<CodeSignature>(*cmd);
          } else {
            LIEF_ERR("Can't parse linkedit_data_command for LC_CODE_SIGNATURE");
          }
          break;
        }

      // ==============
      // Data in Code
      // ==============
      case LoadCommand::TYPE::DATA_IN_CODE:
        {
          /*
           * DO NOT FORGET TO UPDATE DataInCode::classof
           */
          const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse linkedit_data_command for LC_DATA_IN_CODE");
            break;
          }
          load_command = std::make_unique<DataInCode>(*cmd);
          break;
        }


      // =======
      // LC_MAIN
      // =======
      case LoadCommand::TYPE::MAIN:
        {
          /*
           * DO NOT FORGET TO UPDATE MainCommand::classof
           */
          LIEF_DEBUG("[+] Parsing LC_MAIN");
          const auto cmd = stream_->peek<details::entry_point_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse entry_point_command");
            break;
          }
          load_command = std::make_unique<MainCommand>(*cmd);
          break;
        }

      // ==================
      // LC_FUNCTION_STARTS
      // ==================
      case LoadCommand::TYPE::FUNCTION_STARTS:
        {
          /*
           * DO NOT FORGET TO UPDATE FunctionStarts::classof
           */
          LIEF_DEBUG("[+] Parsing LC_FUNCTION_STARTS");
          const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse linkedit_data_command for LC_FUNCTION_STARTS");
            break;
          }
          load_command = std::make_unique<FunctionStarts>(*cmd);
          break;
        }


      // ==================
      // LC_ATOM_INFO
      // ==================
      case LoadCommand::TYPE::ATOM_INFO:
        {
          /*
           * DO NOT FORGET TO UPDATE AtomInfo::classof
           */
          LIEF_DEBUG("[+] Parsing LC_ATOM_INFO");
          const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse linkedit_data_command for LC_ATOM_INFO");
            break;
          }
          load_command = std::make_unique<AtomInfo>(*cmd);
          break;
        }

      case LoadCommand::TYPE::SEGMENT_SPLIT_INFO:
        {
          /*
           * DO NOT FORGET TO UPDATE SegmentSplitInfo::classof
           */
          LIEF_DEBUG("[+] Parsing LC_SEGMENT_SPLIT_INFO");
          const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse linkedit_data_command for LC_SEGMENT_SPLIT_INFO");
            break;
          }
          load_command = std::make_unique<SegmentSplitInfo>(*cmd);
          break;

        }

      case LoadCommand::TYPE::SUB_FRAMEWORK:
        {
          /*
           * DO NOT FORGET TO UPDATE SubFramework::classof
           */
          const auto cmd = stream_->peek<details::sub_framework_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse sub_framework_command");
            break;
          }
          auto u = stream_->peek_string_at(loadcommands_offset + cmd->umbrella);
          if (!u) {
            LIEF_ERR("Can't read umbrella string");
            break;
          }
          auto sf = std::make_unique<SubFramework>(*cmd);
          sf->umbrella(*u);
          load_command = std::move(sf);
          break;
        }

      case LoadCommand::TYPE::SUB_CLIENT:
        {
          /*
           * DO NOT FORGET TO UPDATE SubClient::classof
           */
          const auto cmd = stream_->peek<details::sub_client_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse sub_client_command");
            break;
          }
          auto u = stream_->peek_string_at(loadcommands_offset + cmd->client);
          if (!u) {
            LIEF_ERR("Can't read client name string");
            break;
          }
          auto sf = std::make_unique<SubClient>(*cmd);
          sf->client(*u);
          load_command = std::move(sf);
          break;
        }


      case LoadCommand::TYPE::DYLD_ENVIRONMENT:
        {
          /*
           * DO NOT FORGET TO UPDATE DyldEnvironment::classof
           */
          const auto cmd = stream_->peek<details::dylinker_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse dylinker_command");
            break;
          }

          auto value = stream_->peek_string_at(loadcommands_offset + cmd->name);
          if (!value) {
            LIEF_ERR("Can't read dylinker_command.name");
            break;
          }
          auto env = std::make_unique<DyldEnvironment>(*cmd);
          env->value(*value);
          load_command = std::move(env);
          break;
        }


      // ================
      // Encryption Info
      // ================
      case LoadCommand::TYPE::ENCRYPTION_INFO:
      case LoadCommand::TYPE::ENCRYPTION_INFO_64:
        {
          /*
           * DO NOT FORGET TO UPDATE EncryptionInfo::classof
           */
          LIEF_DEBUG("[+] Parsing {}", to_string(cmd_type));
          const auto cmd = stream_->peek<details::encryption_info_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse encryption_info_command");
            break;
          }
          load_command = std::make_unique<EncryptionInfo>(*cmd);
          break;
        }

      // ==============
      // File Set Entry
      // ==============
      case LoadCommand::TYPE::FILESET_ENTRY:
        {
          /*
           * DO NOT FORGET TO UPDATE FilesetCommand::classof
           */
          LIEF_DEBUG("[+] Parsing {}", to_string(cmd_type));

          const auto cmd = stream_->peek<details::fileset_entry_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse fileset_entry_command");
            break;
          }
          load_command = std::make_unique<FilesetCommand>(*cmd);
          const uint32_t entry_offset = cmd->entry_id;
          auto entry_name = stream_->peek_string_at(loadcommands_offset + entry_offset);

          if (!entry_name) {
            LIEF_ERR("Can't read fileset_entry_command.entry_id");
            break;
          }

          auto* fset = load_command->as<FilesetCommand>();
          fset->name(*entry_name);

          LIEF_DEBUG("Parsing fileset '{}' @ {:x} (size: {:x})",
                     fset->name(), cmd->fileoff, cmd->cmdsize);
          auto res_type = stream_->peek<uint32_t>(cmd->fileoff);
          if (!res_type) {
            LIEF_ERR("Can't access fileset_entry_command.fileoff");
            break;
          }
          auto type = static_cast<MACHO_TYPES>(*res_type);

          // Fat binary
          if (type == MACHO_TYPES::MAGIC_FAT || type == MACHO_TYPES::CIGAM_FAT) {
            LIEF_ERR("Mach-O is corrupted with a FAT Mach-O inside a fileset ?");
            break;
          }


          /* TODO(romain): This part needs to be refactored
           * we should not have to make this kind construction and move
           * with the BinaryParser constructor
           */
          const size_t current_pos = stream_->pos();
          if (!visited_.insert(cmd->fileoff).second) {
            break;
          }

          stream_->setpos(cmd->fileoff);
          BinaryParser bp;
          bp.binary_  = std::unique_ptr<Binary>(new Binary{});
          bp.stream_  = std::move(stream_);
          bp.config_  = config_;
          bp.visited_ = visited_;

          if (!bp.init_and_parse()) {
            LIEF_WARN("Parsing the Binary fileset raised error.");
          }

          stream_ = std::move(bp.stream_);
          stream_->setpos(current_pos);
          visited_ = std::move(bp.visited_);

          if (bp.binary_ != nullptr) {
            std::unique_ptr<Binary> filset_bin = std::move(bp.binary_);
            filset_bin->fileset_name_ = *entry_name;
            binary_->filesets_.push_back(std::move(filset_bin));
          }
          break;
        }
      case LoadCommand::TYPE::DYLD_CHAINED_FIXUPS:
        {
          const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse linkedit_data_command for LC_DYLD_CHAINED_FIXUPS");
            break;
          }

          LIEF_DEBUG("[*] dataoff:  0x{:x}", cmd->dataoff);
          LIEF_DEBUG("[*] datasize: 0x{:x}", cmd->datasize);

          load_command = std::make_unique<DyldChainedFixups>(*cmd);
          auto* chained = load_command->as<DyldChainedFixups>();
          SegmentCommand* lnk = config_.from_dyld_shared_cache ?
                                     binary_->get_segment("__LINKEDIT") :
                                     binary_->segment_from_offset(chained->data_offset());

          if (lnk == nullptr) {
            LIEF_WARN("Can't find the segment associated with "
                      "the LC_DYLD_CHAINED_FIXUPS payload (offset: 0x{:x})", chained->data_offset());
            break;
          }
          LIEF_DEBUG("LC_DYLD_CHAINED_FIXUPS payload in '{}'", lnk->name());
          span<uint8_t> content = lnk->writable_content();

          if (static_cast<int32_t>(chained->data_size()) < 0 ||
              static_cast<int32_t>(chained->data_offset()) < 0) {
            LIEF_WARN("LC_DYLD_CHAINED_FIXUPS payload is corrupted");
            break;
          }

          if ((chained->data_offset() + chained->data_size()) > (lnk->file_offset() + content.size())) {
            LIEF_WARN("LC_DYLD_CHAINED_FIXUPS payload does not fit in the '{}' segments",
                      lnk->name());
            LIEF_DEBUG("{}.file_size: 0x{:x}", lnk->name(), lnk->file_size());
            break;
          }
          const uint64_t rel_offset = chained->data_offset() - lnk->file_offset();
          chained->content_ = content.subspan(rel_offset, chained->data_size());
          break;
        }

      case LoadCommand::TYPE::DYLD_EXPORTS_TRIE:
        {
          if (const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset)) {
            LIEF_DEBUG("[*] dataoff:  0x{:x}", cmd->dataoff);
            LIEF_DEBUG("[*] datasize: 0x{:x}", cmd->datasize);

            load_command = std::make_unique<DyldExportsTrie>(*cmd);
          } else {
            LIEF_ERR("Can't parse linkedit_data_command for LC_DYLD_EXPORTS_TRIE");
          }
          break;
        }

      case LoadCommand::TYPE::TWOLEVEL_HINTS:
        {
          if (const auto cmd = stream_->peek<details::twolevel_hints_command>(loadcommands_offset)) {
            load_command = std::make_unique<TwoLevelHints>(*cmd);
            auto* two = load_command->as<TwoLevelHints>();
            {
              ScopedStream scoped(*stream_, cmd->offset);
              two->hints_.reserve(std::min<size_t>(0x1000, cmd->nhints));
              for (size_t i = 0; i < cmd->nhints; ++i) {
                if (auto res = stream_->read<details::twolevel_hint>()) {
                  uint32_t raw = 0;
                  memcpy(&raw, &*res, sizeof(raw));
                  two->hints_.push_back(raw);
                } else {
                  LIEF_WARN("Can't read LC_TWOLEVEL_HINTS.hints[{}]", i);
                  break;
                }
              }
            }
          } else {
            LIEF_ERR("Can't parse twolevel_hints_command for LC_TWOLEVEL_HINTS");
          }
          break;
        }


      case LoadCommand::TYPE::LINKER_OPTIMIZATION_HINT:
        {
          if (const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset)) {
            LIEF_DEBUG("  [*] dataoff:  0x{:x}", cmd->dataoff);
            LIEF_DEBUG("  [*] datasize: 0x{:x}", cmd->datasize);

            load_command = std::make_unique<LinkerOptHint>(*cmd);
          } else {
            LIEF_ERR("Can't parse linkedit_data_command for LC_LINKER_OPTIMIZATION_HINT");
          }
          break;
        }

      case LoadCommand::TYPE::NOTE:
        {
          if (const auto cmd = stream_->peek<details::note_command>(loadcommands_offset)) {
            load_command = std::make_unique<NoteCommand>(*cmd);
          } else {
            LIEF_ERR("Can't parse note_command for LC_NOTE");
          }
          break;
        }

      case LoadCommand::TYPE::FUNCTION_VARIANTS:
        {
          /*
           * DO NOT FORGET TO UPDATE FunctionVariants::classof
           */
          LIEF_DEBUG("[+] Parsing LC_FUNCTION_VARIANTS");
          const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse linkedit_data_command for LC_FUNCTION_VARIANTS");
            break;
          }
          load_command = std::make_unique<FunctionVariants>(*cmd);
          break;
        }

      case LoadCommand::TYPE::FUNCTION_VARIANT_FIXUPS:
        {
          /*
           * DO NOT FORGET TO UPDATE FunctionVariantFixups::classof
           */
          LIEF_DEBUG("[+] Parsing LC_FUNCTION_VARIANT_FIXUPS");
          const auto cmd = stream_->peek<details::linkedit_data_command>(loadcommands_offset);
          if (!cmd) {
            LIEF_ERR("Can't parse linkedit_data_command for LC_FUNCTION_VARIANT_FIXUPS");
            break;
          }
          load_command = std::make_unique<FunctionVariantFixups>(*cmd);
          break;
        }

      default:
        {
          if (not_parsed.insert(cmd_type).second) {
            LIEF_WARN("Command '{}' ({}) not parsed (size=0x{:04x})!",
                      to_string(cmd_type), static_cast<uint64_t>(cmd_type),
                      command->cmdsize);
          }
          load_command = std::make_unique<UnknownCommand>(*command);
        }
    }

    if (load_command != nullptr) {
      if (!stream_->peek_data(load_command->original_data_, loadcommands_offset, command->cmdsize)) {
        LIEF_ERR("Can't read the raw data of the load command");
        load_command->size_ = 0;
      }
      load_command->command_offset(loadcommands_offset);
      binary_->commands_.push_back(std::move(load_command));
    }
    loadcommands_offset += command->cmdsize;
  }
  binary_->available_command_space_ = low_fileoff - loadcommands_offset;
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::parse_relocations(Section& section) {
  if (section.numberof_relocations() == 0) {
    LIEF_DEBUG("No relocations in {}", section.name());
    return ok();
  }

  LIEF_DEBUG("Parse '{}' relocations (#{:d})", section.name(), section.numberof_relocations());

  uint64_t current_reloc_offset = section.relocation_offset();
  size_t numberof_relocations = section.numberof_relocations();
  if (section.numberof_relocations() > BinaryParser::MAX_RELOCATIONS) {
    numberof_relocations = BinaryParser::MAX_RELOCATIONS;
    LIEF_WARN("Huge number of relocations (#{:d}). Only the first #{:d} will be parsed",
              section.numberof_relocations(), numberof_relocations);

  }
  if (current_reloc_offset + numberof_relocations * 2 * sizeof(uint32_t) > stream_->size()) {
    LIEF_WARN("Relocations corrupted");
    return make_error_code(lief_errors::corrupted);
  }

  for (size_t i = 0; i < numberof_relocations; ++i) {
    std::unique_ptr<RelocationObject> reloc;
    int32_t address = 0;
    if (auto res = stream_->peek<int32_t>(current_reloc_offset)) {
      address = *res;
    } else {
      LIEF_INFO("Can't read relocation address for #{}@0x{:x}", i, address);
      break;
    }
    bool is_scattered = static_cast<bool>(address & Relocation::R_SCATTERED);

    if (is_scattered) {
      if (auto res = stream_->peek<details::scattered_relocation_info>(current_reloc_offset)) {
        reloc = std::make_unique<RelocationObject>(*res);
        reloc->section_ = &section;
      } else {
        LIEF_INFO("Can't read scattered_relocation_info for #{}@0x{:x}", i, current_reloc_offset);
        break;
      }
    } else {
      details::relocation_info reloc_info;
      if (auto res = stream_->peek<details::relocation_info>(current_reloc_offset)) {
        reloc_info = *res;
        reloc = std::make_unique<RelocationObject>(*res);
        reloc->section_ = &section;
      } else {
        LIEF_INFO("Can't read relocation_info for #{}@0x{:x}", i, current_reloc_offset);
        break;
      }

      const auto symbols = binary_->symbols();
      if (reloc_info.r_extern == 1 && reloc_info.r_symbolnum != Relocation::R_ABS) {
        if (reloc_info.r_symbolnum < symbols.size()) {
          Symbol& symbol = symbols[reloc_info.r_symbolnum];
          reloc->symbol_ = &symbol;

          LIEF_DEBUG("Symbol: {}", symbol.name());
        } else {
          LIEF_WARN("Relocation #{:d} of {} symbol index is out-of-bound", i, section.name());
        }
      }
      const auto sections = binary_->sections();
      if (reloc_info.r_extern == 0) {
        const uint32_t sec_num = reloc_info.r_symbolnum;
        if (sec_num == Relocation::R_ABS) {
          // TODO(romain): Find a sample that triggers this branch ..
          const auto it_sym = memoized_symbols_by_address_.find(reloc_info.r_address);
          if (it_sym != std::end(memoized_symbols_by_address_)) {
            reloc->symbol_ = it_sym->second;
          } else {
            LIEF_WARN("Can't find memoized symbol for the address: 0x{:x}", reloc_info.r_address);
          }
        }
        else if (sec_num < sections.size()) {
          Section& relsec = sections[reloc_info.r_symbolnum];
          reloc->section_ = &relsec;
          LIEF_DEBUG("Section: {}", relsec.name());
        } else {
          /*
           * According to ld64-609/src/ld/parsers/macho_relocatable_file.cpp,
           * r_symbolnum can be an index that out-bounds the section tables.
           *
           * if ( reloc->r_extern() ) {
           *   [...]
           * }
           * else {
           *   parser.findTargetFromAddressAndSectionNum(contentValue, reloc->r_symbolnum(), target);
           * }
           * findTargetFromAddressAndSectionNum can fail *silently* so no need to warn the user about that
           */
          LIEF_INFO("Relocation #{:d} of {} seems corrupted: "
                    "r_symbolnum is {} sections.size(): {}",
                    i, section.name(), reloc_info.r_symbolnum, sections.size());
        }
      }
    }

    if (reloc) {
      if (!reloc->has_section()) {
        reloc->section_ = &section;
      }
      reloc->architecture_ = binary_->header().cpu_type();
      section.relocations_.push_back(std::move(reloc));
    }

    current_reloc_offset += 2 * sizeof(uint32_t);
  }
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::parse_dyldinfo_rebases() {
  LIEF_DEBUG("[+] LC_DYLD_INFO.rebases");
  using pint_t = typename MACHO_T::uint;

  DyldInfo* dyldinfo = binary_->dyld_info();
  if (dyldinfo == nullptr) {
    LIEF_ERR("Missing DyldInfo in the main binary");
    return make_error_code(lief_errors::not_found);
  }

  uint32_t offset = std::get<0>(dyldinfo->rebase());
  uint32_t size   = std::get<1>(dyldinfo->rebase());

  if (offset == 0 || size == 0) {
    return ok();
  }

  if (static_cast<int32_t>(offset) < 0 ||
      static_cast<int32_t>(size)   < 0) {
    LIEF_WARN("LC_DYLD_INFO.rebases payload is corrupted");
    return make_error_code(lief_errors::read_out_of_bound);
  }

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(offset);

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the rebase opcodes");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();
  const uint64_t rel_offset = offset - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + size) > content.size()) {
    LIEF_ERR("Rebase opcodes are out of bounds of the segment {}", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  dyldinfo->rebase_opcodes_ = content.subspan(rel_offset, size);

  uint64_t end_offset = offset + size;

  bool     done = false;
  uint8_t  type = 0;
  uint32_t segment_index = 0;
  uint64_t segment_offset = 0;
  uint32_t count = 0;
  uint32_t skip = 0;

  Binary::it_segments segments = binary_->segments();
  const SegmentCommand* current_segment = nullptr;

  stream_->setpos(offset);

  while (!done && stream_->pos() < end_offset) {
    auto val = stream_->read<uint8_t>();
    if (!val) {
      break;
    }
    uint8_t imm    = *val & DyldInfo::IMMEDIATE_MASK;
    uint8_t opcode = *val & DyldInfo::OPCODE_MASK;

    switch(DyldInfo::REBASE_OPCODES(opcode)) {
      case DyldInfo::REBASE_OPCODES::DONE:
        {
          done = true;
          break;
        }

      case DyldInfo::REBASE_OPCODES::SET_TYPE_IMM:
        {
          type = imm;
          break;
        }

      case DyldInfo::REBASE_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          auto seg_offset = stream_->read_uleb128();
          if (!seg_offset) {
            LIEF_ERR("REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB: Can't read uleb128 offset");
            break;
          }
          segment_index   = imm;
          segment_offset  = *seg_offset;

          if (segment_index < segments.size()) {
            current_segment = &segments[segment_index];
          } else {
            LIEF_ERR("REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB: Bad index");
            done = true;
          }

          break;
        }

      case DyldInfo::REBASE_OPCODES::ADD_ADDR_ULEB:
        {
          auto seg_offset = stream_->read_uleb128();
          if (!seg_offset) {
            LIEF_ERR("REBASE_OPCODE_ADD_ADDR_ULEB: Can't read uleb128 offset");
            break;
          }
          segment_offset += *seg_offset;
          if (current_segment == nullptr) {
            LIEF_WARN("REBASE_OPCODE_ADD_ADDR_ULEB: the current segment is null");
          }
          else if (segment_offset > current_segment->file_size()) {
            LIEF_WARN("REBASE_OPCODE_ADD_ADDR_ULEB: Bad offset (0x{:x} > 0x{:x})",
                      segment_offset, current_segment->file_size());
          }
          break;
        }

      case DyldInfo::REBASE_OPCODES::ADD_ADDR_IMM_SCALED:
        {
          segment_offset += (imm * sizeof(pint_t));
          if (current_segment == nullptr) {
            LIEF_WARN("REBASE_OPCODE_ADD_ADDR_IMM_SCALED: the current segment is null");
          }
          else if (segment_offset > current_segment->file_size()) {
            LIEF_WARN("REBASE_OPCODE_ADD_ADDR_IMM_SCALED: Bad offset (0x{:x} > 0x{:x})",
                      segment_offset, current_segment->file_size());
          }
          break;
        }

      case DyldInfo::REBASE_OPCODES::DO_REBASE_IMM_TIMES:
        {
          for (size_t i = 0; i < imm; ++i) {
            do_rebase<MACHO_T>(type, segment_index, segment_offset, &segments);
            segment_offset += sizeof(pint_t);
            if (current_segment == nullptr) {
              LIEF_WARN("REBASE_OPCODE_DO_REBASE_IMM_TIMES: the current segment is null");
            }
            else if (segment_offset > current_segment->file_size()) {
              LIEF_WARN("REBASE_OPCODE_DO_REBASE_IMM_TIMES: Bad offset (0x{:x} > 0x{:x})",
                        segment_offset, current_segment->file_size());
            }
          }
          break;
        }
      case DyldInfo::REBASE_OPCODES::DO_REBASE_ULEB_TIMES:
        {
          auto uleb128_val = stream_->read_uleb128();
          if (!uleb128_val) {
            LIEF_ERR("REBASE_OPCODE_DO_REBASE_ULEB_TIMES: Can't read uleb128 count");
            break;
          }
          count = *uleb128_val;
          for (size_t i = 0; i < count; ++i) {
            if (current_segment == nullptr) {
              LIEF_WARN("REBASE_OPCODE_DO_REBASE_ULEB_TIMES: the current segment is null");
            }
            else if (segment_offset > current_segment->file_size()) {
              LIEF_WARN("REBASE_OPCODE_DO_REBASE_ULEB_TIMES: Bad offset (0x{:x} > 0x{:x})",
                        segment_offset, current_segment->file_size());
            }

            do_rebase<MACHO_T>(type, segment_index, segment_offset, &segments);
            segment_offset += sizeof(pint_t);
          }
          break;
        }

      case DyldInfo::REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB:
        {
          if (current_segment == nullptr) {
            LIEF_WARN("REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB: the current segment is null");
          }
          else if (segment_offset > current_segment->file_size()) {
            LIEF_WARN("REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB: Bad offset (0x{:x} > 0x{:x})",
                      segment_offset, current_segment->file_size());
          }
          do_rebase<MACHO_T>(type, segment_index, segment_offset, &segments);
          auto uleb128_val = stream_->read_uleb128();
          if (!uleb128_val) {
            LIEF_ERR("REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB: Can't read uleb128 segment_offset");
            break;
          }
          segment_offset += *uleb128_val + sizeof(pint_t);

          break;
        }

      case DyldInfo::REBASE_OPCODES::DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
        {
          // Count
          auto uleb128_val = stream_->read_uleb128();
          if (!uleb128_val) {
            LIEF_ERR("Can't read REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB count");
            break;
          }
          count = *uleb128_val;

          uleb128_val = stream_->read_uleb128();
          if (!uleb128_val) {
            LIEF_ERR("Can't read REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB skip");
            break;
          }
          // Skip
          skip = *uleb128_val;


          for (size_t i = 0; i < count; ++i) {
            if (current_segment == nullptr) {
              LIEF_WARN("REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB: the current segment is null");
            }
            else if (segment_offset > current_segment->file_size()) {
              LIEF_WARN("REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB: Bad offset (0x{:x} > 0x{:x})",
                        segment_offset, current_segment->file_size());
            }
            do_rebase<MACHO_T>(type, segment_index, segment_offset, &segments);
            segment_offset += skip + sizeof(pint_t);
          }

          break;
        }

      default:
        {
          LIEF_ERR("Unsupported opcode: 0x{:x}", static_cast<uint32_t>(opcode));
          break;
        }
    }
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::parse_dyldinfo_binds() {
  LIEF_DEBUG("[+] LC_DYLD_INFO.bindings");

  parse_dyldinfo_generic_bind<MACHO_T>();
  parse_dyldinfo_weak_bind<MACHO_T>();
  parse_dyldinfo_lazy_bind<MACHO_T>();

  return ok();
}

// Generic bindings
// ================
template<class MACHO_T>
ok_error_t BinaryParser::parse_dyldinfo_generic_bind() {
  using pint_t = typename MACHO_T::uint;

  DyldInfo* dyldinfo = binary_->dyld_info();
  if (dyldinfo == nullptr) {
    LIEF_ERR("Missing DyldInfo in the main binary");
    return make_error_code(lief_errors::not_found);
  }

  uint32_t offset = std::get<0>(dyldinfo->bind());
  uint32_t size   = std::get<1>(dyldinfo->bind());

  if (offset == 0 || size == 0) {
    return ok();
  }

  if (static_cast<int32_t>(offset) < 0 ||
      static_cast<int32_t>(size)   < 0) {
    LIEF_WARN("LC_DYLD_INFO.binding payload is corrupted");
    return make_error_code(lief_errors::read_out_of_bound);
  }

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(offset);

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the regular bind opcodes");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();
  const uint64_t rel_offset = offset - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + size) > content.size()) {
    LIEF_ERR("Regular bind opcodes are out of bounds of the segment {}", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  dyldinfo->bind_opcodes_ = content.subspan(rel_offset, size);

  uint64_t end_offset = offset + size;

  uint8_t     type = 0;
  uint8_t     segment_idx = 0;
  uint64_t    segment_offset = 0;
  std::string symbol_name;
  int         library_ordinal = 0;

  int64_t     addend = 0;
  uint32_t    count = 0;
  uint32_t    skip = 0;

  bool        is_weak_import = false;
  bool        done = false;

  size_t ordinal_table_size     = 0;
  bool use_threaded_rebase_bind = false;
  uint8_t symbol_flags          = 0;
  uint64_t start_offset         = 0;
  std::vector<ThreadedBindData> ordinal_table;

  Binary::it_segments segments = binary_->segments();
  stream_->setpos(offset);
  while (!done && stream_->pos() < end_offset) {
    auto val = stream_->read<uint8_t>();
    if (!val) {
      break;
    }
    uint8_t imm = *val & DyldInfo::IMMEDIATE_MASK;
    auto opcode = DyldInfo::BIND_OPCODES(*val & DyldInfo::OPCODE_MASK);

    switch (opcode) {
      case DyldInfo::BIND_OPCODES::DONE:
        {
          done = true;
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_DYLIB_ORDINAL_IMM:
        {
          library_ordinal = imm;
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB uleb128 ordinal");
            break;
          }
          library_ordinal = *val;

          break;
        }

      case DyldInfo::BIND_OPCODES::SET_DYLIB_SPECIAL_IMM:
        {
          // the special ordinals are negative numbers
          if (imm == 0) {
            library_ordinal = 0;
          } else {
            int8_t sign_extended = DyldInfo::OPCODE_MASK | imm;
            library_ordinal = sign_extended;
          }
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM:
        {
          auto str = stream_->read_string();
          if (!str) {
            LIEF_ERR("Can't read symbol name");
            break;
          }
          symbol_name = std::move(*str);
          symbol_flags = imm;

          if ((imm & uint8_t(DyldInfo::BIND_SYMBOL_FLAGS::WEAK_IMPORT)) != 0) {
            is_weak_import = true;
          } else {
            is_weak_import = false;
          }
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_TYPE_IMM:
        {
          type = imm;
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_ADDEND_SLEB:
        {
          auto val = stream_->read_sleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_ADDEND_SLEB uleb128 addend");
            break;
          }
          addend = *val;
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB uleb128 segment offset");
            break;
          }
          segment_idx    = imm;
          segment_offset = *val;
          break;
        }

      case DyldInfo::BIND_OPCODES::ADD_ADDR_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_ADD_ADDR_ULEB uleb128 segment offset");
            break;
          }
          segment_offset += *val;
          break;
        }

      case DyldInfo::BIND_OPCODES::DO_BIND:
        {
          if (!use_threaded_rebase_bind) {
            do_bind<MACHO_T>(
                DyldBindingInfo::CLASS::STANDARD,
                type,
                segment_idx,
                segment_offset,
                symbol_name,
                library_ordinal,
                addend,
                is_weak_import,
                false,
                &segments, start_offset);
            start_offset = stream_->pos() - offset + 1;
            segment_offset += sizeof(pint_t);
          } else {
            ordinal_table.push_back(ThreadedBindData{symbol_name, addend, library_ordinal, symbol_flags, type});
          }
          break;
        }

      case DyldInfo::BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB:
        {
          do_bind<MACHO_T>(
              DyldBindingInfo::CLASS::STANDARD,
              type,
              segment_idx,
              segment_offset,
              symbol_name,
              library_ordinal,
              addend,
              is_weak_import,
              false,
              &segments, start_offset);
          start_offset = stream_->pos() - offset + 1;

          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB uleb128 segment offset");
            break;
          }

          segment_offset += *val + sizeof(pint_t);
          break;
        }

      case DyldInfo::BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED:
        {
          do_bind<MACHO_T>(
              DyldBindingInfo::CLASS::STANDARD,
              type,
              segment_idx,
              segment_offset,
              symbol_name,
              library_ordinal,
              addend,
              is_weak_import,
              false,
              &segments, start_offset);
          start_offset = stream_->pos() - offset + 1;
          segment_offset += imm * sizeof(pint_t) + sizeof(pint_t);
          break;
        }

      case DyldInfo::BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB uleb128 count");
            break;
          }
          count = *val;

          val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB uleb128 skip");
            break;
          }
          skip = *val;

          for (size_t i = 0; i < count; ++i) {
            do_bind<MACHO_T>(
                DyldBindingInfo::CLASS::STANDARD,
                type,
                segment_idx,
                segment_offset,
                symbol_name,
                library_ordinal,
                addend,
                is_weak_import,
                false,
                &segments, start_offset);
            start_offset = stream_->pos() - offset + 1;
            segment_offset += skip + sizeof(pint_t);
          }
          break;
        }
      case DyldInfo::BIND_OPCODES::THREADED:
        {
          const auto subopcode = DyldInfo::BIND_SUBOPCODE_THREADED(imm);
          switch (subopcode) {
            case DyldInfo::BIND_SUBOPCODE_THREADED::APPLY:
              {
                uint64_t delta = 0;
                if (segment_idx >= segments.size()) {
                  LIEF_ERR("Wrong index ({:d})", segment_idx);
                  return make_error_code(lief_errors::corrupted);
                }
                const SegmentCommand& current_segment = segments[segment_idx];
                do {
                  const uint64_t address = current_segment.virtual_address() + segment_offset;
                  span<const uint8_t> content = current_segment.content();
                  if (segment_offset >= content.size() || segment_offset + sizeof(uint64_t) >= content.size()) {
                    LIEF_WARN("Bad segment offset (0x{:x})", segment_offset);
                    delta = 0; // exit from de do ... while
                    break;
                  }
                  auto value = *reinterpret_cast<const uint64_t*>(content.data() + segment_offset);
                  bool is_rebase = (value & (static_cast<uint64_t>(1) << 62)) == 0;

                  if (is_rebase) {
                    //LIEF_WARN("do rebase for addr: 0x{:x} vs 0x{:x}", address, current_segment)
                    do_rebase<MACHO_T>(static_cast<uint8_t>(DyldInfo::REBASE_TYPE::POINTER),
                                       segment_idx, segment_offset, &segments);
                  } else {
                    uint16_t ordinal = value & 0xFFFF;
                    if (ordinal >= ordinal_table_size || ordinal >= ordinal_table.size()) {
                      LIEF_WARN("bind ordinal ({:d}) is out of range (max={:d}) for disk pointer 0x{:04x} in "
                                "segment '{}' (segment offset: 0x{:04x})", ordinal, ordinal_table_size, value,
                                current_segment.name(), segment_offset);
                      break;
                    }
                    if (address < current_segment.virtual_address() ||
                        address >= (current_segment.virtual_address() + current_segment.virtual_size())) {
                      LIEF_WARN("Bad binding address");
                      break;
                    }
                    const ThreadedBindData& th_bind_data = ordinal_table[ordinal];
                    do_bind<MACHO_T>(
                        DyldBindingInfo::CLASS::THREADED,
                        th_bind_data.type,
                        segment_idx,
                        segment_offset,
                        th_bind_data.symbol_name,
                        th_bind_data.library_ordinal,
                        th_bind_data.addend,
                        th_bind_data.symbol_flags & uint8_t(DyldInfo::BIND_SYMBOL_FLAGS::WEAK_IMPORT),
                        false,
                        &segments, start_offset);
                        start_offset = stream_->pos() - offset + 1;
                  }
                  // The delta is bits [51..61]
                  // And bit 62 is to tell us if we are a rebase (0) or bind (1)
                  value &= ~(1ull << 62);
                  delta = (value & 0x3FF8000000000000) >> 51;
                  segment_offset += delta * sizeof(pint_t);
                } while (delta != 0);
                break;
              }
            case DyldInfo::BIND_SUBOPCODE_THREADED::SET_BIND_ORDINAL_TABLE_SIZE_ULEB:
              {
                // Maxium number of elements according to dyld's MachOAnalyzer.cpp
                static constexpr size_t MAX_COUNT = 65535;
                auto val = stream_->read_uleb128();
                if (!val) {
                  LIEF_ERR("Can't read BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB count");
                  break;
                }
                count = *val;
                if (count > MAX_COUNT) {
                  LIEF_ERR("BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB"
                           "count is too large ({})", count);
                  break;
                }
                ordinal_table_size = count + 1; // the +1 comes from: 'ld64 wrote the wrong value here and we need to offset by 1 for now.'
                use_threaded_rebase_bind = true;
                ordinal_table.reserve(ordinal_table_size);
                break;
              }
          }
          break;
        }
      default:
        {
          LIEF_ERR("Unsupported opcode: 0x{:x}", static_cast<uint32_t>(opcode));
          break;
        }
      }
  }
  dyldinfo->binding_encoding_version_ = use_threaded_rebase_bind ?
                                        DyldInfo::BINDING_ENCODING_VERSION::V2 :
                                        DyldInfo::BINDING_ENCODING_VERSION::V1;
  return ok();
}

// Weak binding
// ============
template<class MACHO_T>
ok_error_t BinaryParser::parse_dyldinfo_weak_bind() {
  using pint_t = typename MACHO_T::uint;

  DyldInfo* dyldinfo = binary_->dyld_info();
  if (dyldinfo == nullptr) {
    LIEF_ERR("Missing DyldInfo in the main binary");
    return make_error_code(lief_errors::not_found);
  }

  uint32_t offset = std::get<0>(dyldinfo->weak_bind());
  uint32_t size   = std::get<1>(dyldinfo->weak_bind());

  if (offset == 0 || size == 0) {
    return ok();
  }

  if (static_cast<int32_t>(offset) < 0 ||
      static_cast<int32_t>(size)   < 0) {
    LIEF_WARN("LC_DYLD_INFO.weak_bind payload is corrupted");
    return make_error_code(lief_errors::read_out_of_bound);
  }

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(offset);

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the weak bind opcodes");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();
  const uint64_t rel_offset = offset - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + size) > content.size()) {
    LIEF_ERR("Weak bind opcodes are out of bounds of the segment {}", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  dyldinfo->weak_bind_opcodes_ = content.subspan(rel_offset, size);

  uint64_t end_offset = offset + size;

  uint8_t     type = 0;
  uint8_t     segment_idx = 0;
  uint64_t    segment_offset = 0;
  std::string symbol_name;

  int64_t     addend = 0;
  uint32_t    count = 0;
  uint32_t    skip = 0;

  bool        is_weak_import = true;
  bool        is_non_weak_definition = false;
  bool        done = false;
  uint64_t    start_offset    = 0;

  Binary::it_segments segments = binary_->segments();

  stream_->setpos(offset);

  while (!done && stream_->pos() < end_offset) {
    auto val = stream_->read<uint8_t>();
    if (!val) {
      break;
    }
    uint8_t imm = *val & DyldInfo::IMMEDIATE_MASK;
    auto opcode = DyldInfo::BIND_OPCODES(*val & DyldInfo::OPCODE_MASK);

    switch (opcode) {
      case DyldInfo::BIND_OPCODES::DONE:
        {
          done = true;
          break;
        }


      case DyldInfo::BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM:
        {
          auto str = stream_->read_string();
          if (!str) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM symbol name");
            break;
          }
          symbol_name = std::move(*str);

          if ((imm & uint8_t(DyldInfo::BIND_SYMBOL_FLAGS::NON_WEAK_DEFINITION)) != 0) {
            is_non_weak_definition = true;
          } else {
            is_non_weak_definition = false;
          }
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_TYPE_IMM:
        {
          type = imm;
          break;
        }


      case DyldInfo::BIND_OPCODES::SET_ADDEND_SLEB:
        {
          auto val = stream_->read_sleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_ADDEND_SLEB sleb128 addend");
            break;
          }
          addend = *val;
          break;
        }


      case DyldInfo::BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB uleb128 segment offset");
            break;
          }
          segment_idx    = imm;
          segment_offset = *val;

          break;
        }


      case DyldInfo::BIND_OPCODES::ADD_ADDR_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_ADD_ADDR_ULEB uleb128 segment offset");
            break;
          }
          segment_offset += *val;
          break;
        }


      case DyldInfo::BIND_OPCODES::DO_BIND:
        {
          do_bind<MACHO_T>(
              DyldBindingInfo::CLASS::WEAK,
              type,
              segment_idx,
              segment_offset,
              symbol_name,
              0,
              addend,
              is_weak_import,
              is_non_weak_definition,
              &segments, start_offset);
          start_offset = stream_->pos() - offset + 1;
          segment_offset += sizeof(pint_t);
          break;
        }


      case DyldInfo::BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB:
        {
          do_bind<MACHO_T>(
              DyldBindingInfo::CLASS::WEAK,
              type,
              segment_idx,
              segment_offset,
              symbol_name,
              0,
              addend,
              is_weak_import,
              is_non_weak_definition,
              &segments, start_offset);
          start_offset = stream_->pos() - offset + 1;

          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB uleb128 segment offset");
            break;
          }
          segment_offset += *val + sizeof(pint_t);
          break;
        }


      case DyldInfo::BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED:
        {
          do_bind<MACHO_T>(
              DyldBindingInfo::CLASS::WEAK,
              type,
              segment_idx,
              segment_offset,
              symbol_name,
              0,
              addend,
              is_weak_import,
              is_non_weak_definition,
              &segments, start_offset);
          start_offset = stream_->pos() - offset + 1;
          segment_offset += imm * sizeof(pint_t) + sizeof(pint_t);
          break;
        }


      case DyldInfo::BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB uleb128 count");
            break;
          }
          // Count
          count = *val;

          // Skip
          val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB uleb128 skip");
            break;
          }
          skip = *val;

          for (size_t i = 0; i < count; ++i) {
            do_bind<MACHO_T>(
                DyldBindingInfo::CLASS::WEAK,
                type,
                segment_idx,
                segment_offset,
                symbol_name,
                0,
                addend,
                is_weak_import,
                is_non_weak_definition,
                &segments, start_offset);
            start_offset = stream_->pos() - offset + 1;
            segment_offset += skip + sizeof(pint_t);
          }
          break;
        }



      default:
        {
          LIEF_ERR("Unsupported opcode: 0x{:x}", static_cast<uint32_t>(opcode));
          break;
        }
      }
  }
  return ok();
}

// Lazy binding
// ============
template<class MACHO_T>
ok_error_t BinaryParser::parse_dyldinfo_lazy_bind() {
  using pint_t = typename MACHO_T::uint;

  DyldInfo* dyldinfo = binary_->dyld_info();
  if (dyldinfo == nullptr) {
    LIEF_ERR("Missing DyldInfo in the main binary");
    return make_error_code(lief_errors::not_found);
  }

  uint32_t offset = std::get<0>(dyldinfo->lazy_bind());
  uint32_t size   = std::get<1>(dyldinfo->lazy_bind());

  if (offset == 0 || size == 0) {
    return ok();
  }

  if (static_cast<int32_t>(offset) < 0 ||
      static_cast<int32_t>(size)   < 0) {
    LIEF_WARN("LC_DYLD_INFO.lazy payload is corrupted");
    return make_error_code(lief_errors::read_out_of_bound);
  }

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(offset);

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the lazy bind opcodes");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();
  const uint64_t rel_offset = offset - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + size) > content.size()) {
    LIEF_ERR("Lazy bind opcodes are out of bounds of the segment {}", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  dyldinfo->lazy_bind_opcodes_ = content.subspan(rel_offset, size);

  uint64_t end_offset = offset + size;

  //uint32_t    lazy_offset     = 0;
  std::string symbol_name;
  uint8_t     segment_idx     = 0;
  uint64_t    segment_offset  = 0;
  int32_t     library_ordinal = 0;
  int64_t     addend          = 0;
  bool        is_weak_import  = false;
  uint64_t    start_offset    = 0;

  Binary::it_segments segments = binary_->segments();
  stream_->setpos(offset);
  while (stream_->pos() < end_offset) {
    auto val = stream_->read<uint8_t>();
    if (!val) {
      break;
    }
    uint8_t imm = *val & DyldInfo::IMMEDIATE_MASK;
    auto opcode = DyldInfo::BIND_OPCODES(*val & DyldInfo::OPCODE_MASK);

    switch (opcode) {
      case DyldInfo::BIND_OPCODES::DONE:
        {
          //lazy_offset = current_offset - offset;
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_DYLIB_ORDINAL_IMM:
        {
          library_ordinal = imm;
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB uleb128 library ordinal");
            break;
          }

          library_ordinal = *val;
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_DYLIB_SPECIAL_IMM:
        {
          // the special ordinals are negative numbers
          if (imm == 0) {
            library_ordinal = 0;
          } else {
            int8_t sign_extended = DyldInfo::OPCODE_MASK | imm;
            library_ordinal = sign_extended;
          }
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM:
        {
          auto str = stream_->read_string();
          if (!str) {
            LIEF_ERR("Can't read symbol name");
            break;
          }
          symbol_name = std::move(*str);

          if ((imm & uint8_t(DyldInfo::BIND_SYMBOL_FLAGS::WEAK_IMPORT)) != 0) {
            is_weak_import = true;
          } else {
            is_weak_import = false;
          }
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_ADDEND_SLEB:
        {
          auto val = stream_->read_sleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_ADDEND_SLEB sleb128 addend");
            break;
          }
          addend = *val;
          break;
        }

      case DyldInfo::BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          auto val = stream_->read_uleb128();
          if (!val) {
            LIEF_ERR("Can't read BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB uleb128 segment offset");
            break;
          }
          segment_idx    = imm;
          segment_offset = *val;

          break;
        }

      case DyldInfo::BIND_OPCODES::DO_BIND:
        {
          do_bind<MACHO_T>(
              DyldBindingInfo::CLASS::LAZY,
              static_cast<uint8_t>(DyldBindingInfo::TYPE::POINTER),
              segment_idx,
              segment_offset,
              symbol_name,
              library_ordinal,
              addend,
              is_weak_import,
              false,
              &segments, start_offset);
          start_offset = stream_->pos() - offset + 1;
          segment_offset += sizeof(pint_t);
          break;
        }

      default:
        {
          LIEF_ERR("Unsupported opcode: 0x{:x}", static_cast<uint32_t>(opcode));
          break;
        }
      }
  }
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::do_bind(DyldBindingInfo::CLASS cls,
                                 uint8_t type,
                                 uint8_t segment_idx,
                                 uint64_t segment_offset,
                                 const std::string& symbol_name,
                                 int32_t ord,
                                 int64_t addend,
                                 bool is_weak,
                                 bool is_non_weak_definition,
                                 it_opaque_segments segments_ptr,
                                 uint64_t offset)
{
  auto& segments = *static_cast<Binary::it_segments*>(segments_ptr);
  if (segment_idx >= segments.size()) {
    LIEF_ERR("Wrong index: {:d}", segment_idx);
    return make_error_code(lief_errors::corrupted);
  }
  SegmentCommand& segment = segments[segment_idx];
  // Address to bind
  uint64_t address = segment.virtual_address() + segment_offset;

  if (address > (segment.virtual_address() + segment.virtual_size())) {
    LIEF_ERR("Bad address: 0x{:x}", address);
    return make_error_code(lief_errors::corrupted);
  }


  // Create a BindingInfo object
  auto binding_info = std::make_unique<DyldBindingInfo>(
      cls, DyldBindingInfo::TYPE(type), address, addend, ord, is_weak,
      is_non_weak_definition, offset);

  binding_info->segment_ = &segment;


  if (0 < ord && static_cast<size_t>(ord) <= binding_libs_.size()) {
    binding_info->library_ = binding_libs_[ord - 1];
  }

  Symbol* symbol = nullptr;
  auto search = memoized_symbols_.find(symbol_name);
  if (search != memoized_symbols_.end()) {
    symbol = search->second;
  } else {
    symbol = binary_->get_symbol(symbol_name);
  }
  if (symbol != nullptr) {
    binding_info->symbol_ = symbol;
    symbol->binding_info_ = binding_info.get();
  } else {
    LIEF_INFO("New symbol discovered: {}", symbol_name);
    auto symbol = std::make_unique<Symbol>();
    symbol->origin_            = Symbol::ORIGIN::DYLD_BIND;
    symbol->type_              = 0;
    symbol->numberof_sections_ = 0;
    symbol->description_       = 0;
    symbol->name(symbol_name);

    binding_info->symbol_ = symbol.get();
    symbol->binding_info_ = binding_info.get();
    binary_->symbols_.push_back(std::move(symbol));
  }

  DyldInfo* dyld_info = binary_->dyld_info();
  if (dyld_info == nullptr) {
    LIEF_ERR("Missing DyldInfo in the main binary");
    return make_error_code(lief_errors::not_found);
  }
  dyld_info->binding_info_.push_back(std::move(binding_info));
  LIEF_DEBUG("{} {} - {}", to_string(cls), segment.name(), symbol_name);
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::do_rebase(uint8_t type, uint8_t segment_idx, uint64_t segment_offset,
                                   const it_opaque_segments segments_ptr) {

  const auto& segments = *static_cast<const Binary::it_segments*>(segments_ptr);

  using pint_t = typename MACHO_T::uint;

  if (segment_idx >= segments.size()) {
    LIEF_ERR("Wrong index ({:d})", segment_idx);
    return make_error_code(lief_errors::corrupted);
  }

  SegmentCommand& segment = segments[segment_idx];
  uint64_t address = segment.virtual_address() + segment_offset;

  if (address > (segment.virtual_address() + segment.virtual_size())) {
    LIEF_ERR("Bad rebase address: 0x{:x}", address);
    return make_error_code(lief_errors::corrupted);
  }

  auto reloc = std::make_unique<RelocationDyld>(address, type);

  // result.second is true if the insertion succeed
  reloc->architecture_ = binary_->header().cpu_type();

  // Tie section and segment
  reloc->segment_ = &segment;
  Section* section = binary_->section_from_virtual_address(address);
  if (section == nullptr) {
    LIEF_ERR("Can't find the section associated with the virtual address 0x{:x}", address);
    return make_error_code(lief_errors::not_found);
  }
  reloc->section_ = section;

  // Tie symbol
  const auto it_symbol = memoized_symbols_by_address_.find(address);
  if (it_symbol != memoized_symbols_by_address_.end()) {
    reloc->symbol_ = it_symbol->second;
  }

  switch (DyldInfo::REBASE_TYPE(type)) {
    case DyldInfo::REBASE_TYPE::POINTER:
      {
        reloc->size_ = sizeof(pint_t) * BYTE_BITS;
        break;
      }


    case DyldInfo::REBASE_TYPE::TEXT_ABSOLUTE32:
    case DyldInfo::REBASE_TYPE::TEXT_PCREL32:
      {
        reloc->size_ = sizeof(uint32_t) * BYTE_BITS;
        break;
      }

    case DyldInfo::REBASE_TYPE::THREADED:
      {
        reloc->size_ = sizeof(pint_t) * BYTE_BITS;
        break;
      }

    default:
      {
        LIEF_ERR("Unsuported relocation type: 0x{:x}", type);
      }
  }
  // Check if a relocation already exists:
  if (dyld_reloc_addrs_.insert(address).second) {
    segment.relocations_.push_back(std::move(reloc));
  } else {
    LIEF_DEBUG("[!] Duplicated symbol address in the dyld rebase: 0x{:x}", address);
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::parse_chained_payload(SpanStream& stream) {
  details::dyld_chained_fixups_header header;

  if (auto res = stream.peek<details::dyld_chained_fixups_header>()) {
    header = *res;
  } else {
    LIEF_WARN("Can't read dyld_chained_fixups_header: {}",
              to_string(get_error(res)));
    return make_error_code(lief_errors::read_error);
  }

  LIEF_DEBUG("fixups_version = {}", header.fixups_version);
  LIEF_DEBUG("starts_offset  = {}", header.starts_offset);
  LIEF_DEBUG("imports_offset = {}", header.imports_offset);
  LIEF_DEBUG("symbols_offset = {}", header.symbols_offset);
  LIEF_DEBUG("imports_count  = {}", header.imports_count);
  LIEF_DEBUG("imports_format = {} ({})", header.imports_format,
                                         to_string(static_cast<DYLD_CHAINED_FORMAT>(header.imports_format)));
  LIEF_DEBUG("symbols_format = {}", header.symbols_format);
  chained_fixups_->update_with(header);

  auto res_symbols_pools = stream.slice(header.symbols_offset);
  if (!res_symbols_pools) {
    LIEF_WARN("Can't access the symbols pools (dyld_chained_fixups_header.symbols_offset)");
    return make_error_code(lief_errors::read_error);
  }

  SpanStream symbols_pool = std::move(*res_symbols_pools);
  symbols_pool.set_endian_swap(stream_->should_swap());
  if (!parse_chained_import<MACHO_T>(header, stream, symbols_pool)) {
    LIEF_WARN("Error while parsing the chained imports");
    return make_error_code(lief_errors::parsing_error);
  }
  if (!parse_chained_fixup<MACHO_T>(header, stream)) {
    LIEF_WARN("Error while parsing the chained fixup");
    return make_error_code(lief_errors::parsing_error);
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::parse_chained_import(const details::dyld_chained_fixups_header& header,
                                              SpanStream& stream, SpanStream& symbol_pool) {

  // According to validChainedFixupsInfo for the cases of
  // DYLD_CHAINED_PTR_64 / DYLD_CHAINED_PTR_64_OFFSET / DYLD_CHAINED_PTR_ARM64E_USERLAND24
  static constexpr uint32_t MAX_BIND_ORDINAL = 0xFFFFFF;


  // Sanity checks according to dyld-852.2 / MachOAnalyzer.cpp:forEachChainedFixupTarget
  if (header.imports_offset > stream.size() || header.symbols_offset > stream.size()) {
    LIEF_WARN("Malformed LC_DYLD_CHAINED_FIXUPS: "
              "dyld_chained_fixups_header.{{imports_offset, symbols_offset}} are out of ranges");
    return make_error_code(lief_errors::parsing_error);
  }

  if (header.imports_count >= MAX_BIND_ORDINAL) {
    LIEF_WARN("dyld_chained_fixups_header.imports_count is too large: {}. It should at most {}",
              header.imports_count, MAX_BIND_ORDINAL);
    return make_error_code(lief_errors::parsing_error);
  }

  const auto fmt = static_cast<DYLD_CHAINED_FORMAT>(header.imports_format);
  switch (fmt) {
    case DYLD_CHAINED_FORMAT::IMPORT:
      {
        stream.setpos(header.imports_offset);
        for (size_t i = 0; i < header.imports_count; ++i) {
          details::dyld_chained_import import;
          std::string symbol_name;
          if (auto res = stream.read<details::dyld_chained_import>()) {
            import = *res;
          } else {
            LIEF_WARN("Can't read dyld_chained_import #{}: {}", i,
                      to_string(get_error(res)));
            break;
          }
          LIEF_DEBUG("dyld chained import[{}]", i);
          if (auto res = symbol_pool.peek_string_at(import.name_offset)) {
            symbol_name = std::move(*res);
          } else {
            LIEF_WARN("Can't read dyld_chained_import.name #{}: {}", i,
                      to_string(get_error(res)));
            break;
          }
          int32_t lib_ordinal = 0;
          uint8_t lib_val = import.lib_ordinal;
          if (lib_val > 0xF0) {
            lib_ordinal = static_cast<int8_t>(lib_val);
          } else {
            lib_ordinal = lib_val;
          }
          do_fixup<MACHO_T>(fmt, lib_ordinal, symbol_name, 0, import.weak_import);
        }
        break;
      }
    case DYLD_CHAINED_FORMAT::IMPORT_ADDEND:
      {
        stream.setpos(header.imports_offset);
        for (size_t i = 0; i < header.imports_count; ++i) {
          details::dyld_chained_import_addend import;
          std::string symbol_name;
          if (auto res = stream.read<details::dyld_chained_import_addend>()) {
            import = *res;
          } else {
            LIEF_WARN("Can't read dyld_chained_import_addend #{}: {}", i,
                      to_string(get_error(res)));
            break;
          }
          if (auto res = symbol_pool.peek_string_at(import.name_offset)) {
            symbol_name = std::move(*res);
          } else {
            LIEF_WARN("Can't read dyld_chained_import_addend.name #{}: {}", i,
                      to_string(get_error(res)));
            break;
          }
          int32_t lib_ordinal = 0;
          uint8_t lib_val = import.lib_ordinal;
          if (lib_val > 0xF0) {
            lib_ordinal = static_cast<int8_t>(lib_val);
          } else {
            lib_ordinal = lib_val;
          }
          do_fixup<MACHO_T>(fmt, lib_ordinal, symbol_name, import.addend, import.weak_import);
        }
        break;
      }
    case DYLD_CHAINED_FORMAT::IMPORT_ADDEND64:
      {
        stream.setpos(header.imports_offset);
        for (size_t i = 0; i < header.imports_count; ++i) {
          details::dyld_chained_import_addend64 import;
          std::string symbol_name;
          if (auto res = stream.read<details::dyld_chained_import_addend64>()) {
            import = *res;
          } else {
            LIEF_WARN("Can't read dyld_chained_import_addend64 #{}: {}", i,
                      to_string(get_error(res)));
            break;
          }
          if (auto res = symbol_pool.peek_string_at(import.name_offset)) {
            symbol_name = std::move(*res);
          } else {
            LIEF_WARN("Can't read dyld_chained_import_addend64.name #{}: {}", i,
                      to_string(get_error(res)));
            break;
          }
          int32_t lib_ordinal = 0;
          uint16_t lib_val = import.lib_ordinal;
          if (lib_val > 0xFFF0) {
            lib_ordinal = static_cast<int16_t>(lib_val);
          } else {
            lib_ordinal = lib_val;
          }
          do_fixup<MACHO_T>(fmt, lib_ordinal, symbol_name, import.addend, import.weak_import);
        }
        break;
      }
    default:
      {
        LIEF_WARN("Dyld Chained Fixups: {} is an unknown format", header.imports_format);
      }
  }
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::parse_chained_fixup(const details::dyld_chained_fixups_header& header,
                                             SpanStream& stream)
{
  details::dyld_chained_starts_in_image starts;
  stream.setpos(header.starts_offset);
  if (auto res = stream.read<details::dyld_chained_starts_in_image>()) {
    starts = *res;
  } else {
    LIEF_WARN("Can't read dyld_chained_starts_in_image: {}",
              to_string(get_error(res)));
    return make_error_code(res.error());
  }

  LIEF_DEBUG("chained starts in image");
  LIEF_DEBUG("  seg_count = {}", starts.seg_count);

  uint32_t nb_segments = starts.seg_count;
  if (nb_segments > binary_->segments_.size()) {
    LIEF_WARN("Chained fixup: dyld_chained_starts_in_image.seg_count ({}) "
              "exceeds the number of segments ({})", starts.seg_count, binary_->segments_.size());
    nb_segments = binary_->segments_.size();
  }

  for (uint32_t seg_idx = 0; seg_idx < nb_segments; ++seg_idx) {
    uint32_t seg_info_offset = 0;
    if (auto res = stream.read<uint32_t>()) {
      seg_info_offset = *res;
    } else {
      LIEF_WARN("Can't read dyld_chained_starts_in_image.seg_info_offset[#{}]: {}",
                seg_idx, to_string(get_error(res)));
      break;
    }

    LIEF_DEBUG("    0x{:06x} seg_offset[{}] = {} ({})",
               stream.pos() - sizeof(uint32_t),
               seg_idx, seg_info_offset, binary_->segments_[seg_idx]->name());
    if (seg_info_offset == 0) {
      struct DyldChainedFixups::chained_starts_in_segment info(0, {}, *binary_->segments_[seg_idx]);
      chained_fixups_->chained_starts_in_segment_.push_back(std::move(info));
      continue;
    }
    LIEF_DEBUG("    #{} processing dyld_chained_starts_in_segment", seg_idx);
    const uint64_t offset = header.starts_offset + seg_info_offset;
    if (!parse_fixup_seg<MACHO_T>(stream, seg_info_offset, offset, seg_idx)) {
      LIEF_WARN("Error while parsing fixup in segment: {}", binary_->segments_[seg_idx]->name());
    }
  }

  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::parse_fixup_seg(SpanStream& stream, uint32_t seg_info_offset,
                                         uint64_t offset, uint32_t seg_idx)
{
  static constexpr const char DPREFIX[] = "    ";
  static constexpr auto DYLD_CHAINED_PTR_START_NONE  = 0xFFFF;
  static constexpr auto DYLD_CHAINED_PTR_START_MULTI = 0x8000;
  static constexpr auto DYLD_CHAINED_PTR_START_LAST  = 0x8000;

  const uint64_t imagebase = binary_->imagebase();

  details::dyld_chained_starts_in_segment seg_info;
  if (auto res = stream.peek<decltype(seg_info)>(offset)) {
    seg_info = *res;
  } else {
    LIEF_WARN("Can't read dyld_chained_starts_in_segment for #{}: {}", seg_idx,
              to_string(get_error(res)));
    return make_error_code(res.error());
  }
  auto res_seg_stream = stream.slice(offset);
  if (!res_seg_stream) {
    LIEF_ERR("Can't slice dyld_chained_starts_in_segment #{}: {}",
              seg_idx, to_string(get_error(res_seg_stream)));
    return make_error_code(res_seg_stream.error());
  }
  SpanStream seg_stream = std::move(*res_seg_stream);
  seg_stream.set_endian_swap(stream_->should_swap());
  seg_stream.read<details::dyld_chained_starts_in_segment>();

  LIEF_DEBUG("{}size              = {}",      DPREFIX, seg_info.size);
  LIEF_DEBUG("{}page_size         = 0x{:x}",  DPREFIX, seg_info.page_size);
  LIEF_DEBUG("{}pointer_format    = {} ({})", DPREFIX, seg_info.pointer_format, to_string(static_cast<DYLD_CHAINED_PTR_FORMAT>(seg_info.pointer_format)));
  LIEF_DEBUG("{}segment_offset    = 0x{:x}",  DPREFIX, seg_info.segment_offset);
  LIEF_DEBUG("{}max_valid_pointer = {}",      DPREFIX, seg_info.max_valid_pointer);
  LIEF_DEBUG("{}page_count        = {}",      DPREFIX, seg_info.page_count);

  SegmentCommand* segment = binary_->segments_[seg_idx];

  struct DyldChainedFixups::chained_starts_in_segment info(seg_info_offset, seg_info, *segment);
  info.page_start.reserve(10);
  const uint64_t page_start_off = seg_stream.pos();
  for (uint32_t page_idx = 0; page_idx < seg_info.page_count; ++page_idx) {
    uint16_t offset_in_page = 0;
    if (auto res = seg_stream.read<decltype(offset_in_page)>()) {
      offset_in_page = *res;
    } else {
      LIEF_WARN("Can't read dyld_chained_starts_in_segment.page_start[{}]", page_idx);
      break;
    }
    info.page_start.push_back(offset_in_page);

    LIEF_DEBUG("{}    page_start[{}]: {}", DPREFIX, page_idx, offset_in_page);

    if (offset_in_page == DYLD_CHAINED_PTR_START_NONE) {
      continue;
    }

    if ((offset_in_page & DYLD_CHAINED_PTR_START_MULTI) > 0) {
      uint32_t overflow_index = offset_in_page & ~DYLD_CHAINED_PTR_START_MULTI;
      bool chain_end = false;
      while (!chain_end) {
        uint16_t overflow_val = 0;
        const uint64_t off = page_start_off + overflow_index * sizeof(uint16_t); // &segInfo->page_start[overflowIndex]
        if (auto res = seg_stream.peek<decltype(overflow_val)>(off)) {
          overflow_val = *res;
        } else {
          LIEF_WARN("Can't read page_start[overflow_index: {}]", overflow_index);
          break;
        }
        chain_end      = overflow_val & DYLD_CHAINED_PTR_START_LAST;
        offset_in_page = overflow_val & ~DYLD_CHAINED_PTR_START_LAST;
        uint64_t page_content_start = seg_info.segment_offset + (page_idx * seg_info.page_size);
        uint64_t chain_address = imagebase + page_content_start + offset_in_page;
        uint64_t chain_offset = (chain_address - segment->virtual_address()) + segment->file_offset();
        auto is_ok = walk_chain<MACHO_T>(*segment, chain_address, chain_offset, seg_info);
        if (!is_ok) {
          LIEF_WARN("Error while walking through the chained fixup of the segment '{}'", segment->name());
        }
        ++overflow_index;
      }

    } else {
      uint64_t page_content_start = seg_info.segment_offset + (page_idx * seg_info.page_size);
      uint64_t chain_address = imagebase + page_content_start + offset_in_page;
      uint64_t chain_offset = (chain_address - segment->virtual_address()) + segment->file_offset();
      auto is_ok = walk_chain<MACHO_T>(*segment, chain_address, chain_offset, seg_info);
      if (!is_ok) {
        LIEF_WARN("Error while walking through the chained fixup of the segment '{}'", segment->name());
      }
    }
  }

  chained_fixups_->chained_starts_in_segment_.push_back(std::move(info));
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::do_fixup(DYLD_CHAINED_FORMAT fmt, int32_t ord, const std::string& symbol_name,
                                  int64_t addend, bool is_weak)
{
  auto binding_info = std::make_unique<ChainedBindingInfoList>(fmt, is_weak);
  binding_info->addend_ = addend;
  binding_info->library_ordinal_ = ord;
  if (0 < ord && static_cast<size_t>(ord) <= binding_libs_.size()) {
    binding_info->library_ = binding_libs_[ord - 1];
    LIEF_DEBUG("  lib_ordinal: {} ({})", ord, binding_libs_[ord - 1]->name());
  } else {
    LIEF_DEBUG("  lib_ordinal: {}", ord);
  }

  LIEF_DEBUG("  weak_import: {}", is_weak);
  LIEF_DEBUG("  name:        {}", symbol_name);

  auto search = memoized_symbols_.find(symbol_name);

  Symbol* symbol = nullptr;
  if (search != std::end(memoized_symbols_)) {
    symbol = search->second;
  } else {
    symbol = binary_->get_symbol(symbol_name);
  }
  if (symbol != nullptr) {
    binding_info->symbol_ = symbol;
    symbol->binding_info_ = binding_info.get();
    if (symbol->library_ordinal() == ord) {
      symbol->library_ = binding_info->library_;
    }
  } else {
    LIEF_INFO("New symbol discovered: {}", symbol_name);
    auto symbol = std::make_unique<Symbol>();
    symbol->type_              = 0;
    symbol->numberof_sections_ = 0;
    symbol->description_       = 0;
    symbol->library_           = binding_info->library_;
    symbol->name(symbol_name);

    binding_info->symbol_ = symbol.get();
    symbol->binding_info_ = binding_info.get();
    binary_->symbols_.push_back(std::move(symbol));
  }
  chained_fixups_->internal_bindings_.push_back(std::move(binding_info));
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::walk_chain(SegmentCommand& segment,
                                    uint64_t chain_address, uint64_t chain_offset,
                                    const details::dyld_chained_starts_in_segment& seg_info)
{
  bool stop      = false;
  bool chain_end = false;
  while (!stop && !chain_end) {
    if (!process_fixup<MACHO_T>(segment, chain_address, chain_offset, seg_info)) {
      LIEF_WARN("Error while processing the chain at offset: 0x{:x}", chain_offset);
      return make_error_code(lief_errors::parsing_error);
    }
    if (auto res = next_chain<MACHO_T>(/* in,out */chain_address, chain_offset, seg_info)) {
      chain_offset = *res;
    } else {
      LIEF_WARN("Error while computing the next chain for the offset: 0x{:x}", chain_offset);
      return make_error_code(lief_errors::parsing_error);
    }

    if (chain_offset == 0) {
      chain_end = true;
    }
  }
  return ok();
}


template<class MACHO_T>
result<uint64_t> BinaryParser::next_chain(uint64_t& chain_address, uint64_t chain_offset,
                                          const details::dyld_chained_starts_in_segment& seg_info)
{
  const auto ptr_fmt = static_cast<DYLD_CHAINED_PTR_FORMAT>(seg_info.pointer_format);
  static constexpr uint64_t CHAIN_END = 0;
  const uintptr_t stride = ChainedPointerAnalysis::stride(ptr_fmt);

  switch (ptr_fmt) {
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_KERNEL:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_FIRMWARE:
      {
        /* offset point to a dyld_chained_ptr_arm64e_* structure */
        details::dyld_chained_ptr_arm64e chain;
        if (auto res = stream_->peek<decltype(chain)>(chain_offset)) {
          chain = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }

        if (chain.rebase.next == 0) {
          return CHAIN_END;
        }
        const uint32_t delta = chain.rebase.next * stride;
        chain_address += delta;
        return chain_offset + delta;
      }

    case DYLD_CHAINED_PTR_FORMAT::PTR_64:
    case DYLD_CHAINED_PTR_FORMAT::PTR_64_OFFSET:
      {
        details::dyld_chained_ptr_generic64 chain;
        if (auto res = stream_->peek<decltype(chain)>(chain_offset)) {
          chain = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }

        if (chain.rebase.next == 0) {
          return CHAIN_END;
        }

        const uint32_t delta = chain.rebase.next * stride;
        chain_address += delta;
        return chain_offset + delta;
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_32:
      {
        details::dyld_chained_ptr_generic32 chain;
        if (auto res = stream_->peek<decltype(chain)>(chain_offset)) {
          chain = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }

        if (chain.rebase.next == 0) {
          return CHAIN_END;
        }

        const uint32_t delta = chain.rebase.next * stride;
        chain_offset += delta;
        chain_address += delta;

        if (auto res = stream_->peek<decltype(chain)>(chain_offset)) {
          chain = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return CHAIN_END;
        }

        while (chain.rebase.bind == 0 && chain.rebase.target > seg_info.max_valid_pointer) {
          const uint32_t delta = chain.rebase.next * stride;
          chain_offset += delta;
          chain_address += delta;
          if (auto res = stream_->peek<decltype(chain)>(chain_offset)) {
            chain = *res;
          } else {
            LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
            return CHAIN_END;
          }
        }
        return chain_offset;
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_64_KERNEL_CACHE:
    case DYLD_CHAINED_PTR_FORMAT::PTR_X86_64_KERNEL_CACHE:
      {
        details::dyld_chained_ptr_kernel64 chain;

        if (auto res = stream_->peek<decltype(chain)>(chain_offset)) {
          chain = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }

        if (chain.next == 0) {
          return CHAIN_END;
        }

        int32_t delta = chain.next * stride - chain_offset;
        chain_address += delta;
        return chain.next * stride;
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_32_FIRMWARE:
      {
        details::dyld_chained_ptr_firm32 chain;

        if (auto res = stream_->peek<decltype(chain)>(chain_offset)) {
          chain = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }

        if (chain.next == 0) {
          return CHAIN_END;
        }
        int32_t delta = chain.next * stride - chain_offset;
        chain_address += delta;
        return chain.next * stride;
      }

    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SEGMENTED:
      {
        details::dyld_chained_ptr_arm64e_segmented_rebase chain;

        if (auto res = stream_->peek<decltype(chain)>(chain_offset)) {
          chain = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }

        if (chain.next == 0) {
          return CHAIN_END;
        }

        int32_t delta = chain.next * stride - chain_offset;
        chain_address += delta;
        return chain.next * stride;
      }

    default:
      {
        LIEF_ERR("Unknown pointer format: 0x{:04x}", seg_info.pointer_format);
        return make_error_code(lief_errors::not_supported);
      }
  }
  return make_error_code(lief_errors::not_supported);
}


template<class MACHO_T>
ok_error_t BinaryParser::process_fixup(SegmentCommand& segment,
                                       uint64_t chain_address, uint64_t chain_offset,
                                       const details::dyld_chained_starts_in_segment& seg_info)
{
  const auto ptr_fmt = static_cast<DYLD_CHAINED_PTR_FORMAT>(seg_info.pointer_format);
  //LIEF_DEBUG("0x{:04x}: {}", chain_offset, to_string(ptr_fmt));
  switch (ptr_fmt) {
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_KERNEL:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND:
    case DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24:
      {
        /* offset point to a dyld_chained_ptr_arm64e_* structure */
        details::dyld_chained_ptr_arm64e fixup;
        if (auto res = stream_->peek<decltype(fixup)>(chain_offset)) {
          fixup = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }

        auto is_ok = do_chained_fixup(segment, chain_address, chain_offset, seg_info, fixup);
        if (!is_ok) {
          LIEF_WARN("Can't process the fixup {} - 0x{:x}", segment.name(), chain_offset);
          return make_error_code(is_ok.error());
        }
        return ok();
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_64:
    case DYLD_CHAINED_PTR_FORMAT::PTR_64_OFFSET:
      {
        details::dyld_chained_ptr_generic64 fixup;
        if (auto res = stream_->peek<decltype(fixup)>(chain_offset)) {
          fixup = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }
        auto is_ok = do_chained_fixup(segment, chain_address, chain_offset, seg_info, fixup);
        if (!is_ok) {
          LIEF_WARN("Can't process the fixup {} - 0x{:x}", segment.name(), chain_offset);
          return make_error_code(is_ok.error());
        }
        return ok();
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_32:
      {
        details::dyld_chained_ptr_generic32 fixup;
        if (auto res = stream_->peek<decltype(fixup)>(chain_offset)) {
          fixup = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }
        auto is_ok = do_chained_fixup(segment, chain_address, chain_offset, seg_info, fixup);
        if (!is_ok) {
          LIEF_WARN("Can't process the fixup {} - 0x{:x}", segment.name(), chain_offset);
          return make_error_code(is_ok.error());
        }
        return ok();
      }
    case DYLD_CHAINED_PTR_FORMAT::PTR_64_KERNEL_CACHE:
    case DYLD_CHAINED_PTR_FORMAT::PTR_X86_64_KERNEL_CACHE:
    case DYLD_CHAINED_PTR_FORMAT::PTR_32_FIRMWARE:
      {
        LIEF_INFO("DYLD_CHAINED_PTR_FORMAT: {} is not implemented. Please consider opening an issue with "
                  "the attached binary", to_string(ptr_fmt));
        return make_error_code(lief_errors::not_implemented);
      }
    case LIEF::MachO::DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SEGMENTED:
      {
        details::dyld_chained_ptr_arm64e_segmented fixup;
        if (auto res = stream_->peek<decltype(fixup)>(chain_offset)) {
          fixup = *res;
        } else {
          LIEF_ERR("Can't read the dyld chain at 0x{:x}", chain_offset);
          return make_error_code(res.error());
        }

        auto is_ok = do_chained_fixup(segment, chain_address, chain_offset, seg_info, fixup);
        if (!is_ok) {
          LIEF_WARN("Can't process the fixup {} - 0x{:x}", segment.name(), chain_offset);
          return make_error_code(is_ok.error());
        }
        return ok();
      }

    default:
      {
        LIEF_ERR("Unknown pointer format: 0x{:04x}", seg_info.pointer_format);
        return make_error_code(lief_errors::not_supported);
      }
  }
  return ok();
}


/* ARM64E Fixup
 * =====================================
 */
ok_error_t BinaryParser::do_chained_fixup(SegmentCommand& segment,
                                          uint64_t chain_address, uint32_t chain_offset,
                                          const details::dyld_chained_starts_in_segment& seg_info,
                                          const details::dyld_chained_ptr_arm64e& fixup)
{
  static constexpr const char DPREFIX[] = "          ";
  const auto ptr_fmt = static_cast<DYLD_CHAINED_PTR_FORMAT>(seg_info.pointer_format);
  const uint64_t imagebase = binary_->imagebase();

  const uint64_t address = chain_address;
  if (fixup.auth_rebase.auth) {
    // ---------- auth && bind ----------
    if (fixup.auth_bind.bind) {
      uint32_t bind_ordinal = ptr_fmt == DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24 ?
                              fixup.auth_bind24.ordinal :
                              fixup.auth_bind.ordinal;
      if (bind_ordinal >= chained_fixups_->internal_bindings_.size()) {
        LIEF_WARN("Out of range bind ordinal {} (max {})",
                  bind_ordinal, chained_fixups_->internal_bindings_.size());
        return make_error_code(lief_errors::read_error);
      }
      std::unique_ptr<ChainedBindingInfoList>& local_binding = chained_fixups_->internal_bindings_[bind_ordinal];
      local_binding->segment_    = &segment;
      local_binding->ptr_format_ = ptr_fmt;
      local_binding->set(fixup.auth_bind);

      chained_fixups_->all_bindings_.push_back(std::make_unique<ChainedBindingInfo>(*local_binding));
      auto& binding_extra_info = chained_fixups_->all_bindings_.back();
      copy_from(*binding_extra_info, *local_binding);

      binding_extra_info->offset_ = chain_offset;
      binding_extra_info->address_ = chain_address;
      local_binding->elements_.push_back(binding_extra_info.get());

      if (Symbol* sym = binding_extra_info->symbol()) {
        LIEF_DEBUG("{}[  BIND] {}@0x{:x}: {} / sign ext: {:x}",
                   DPREFIX, segment.name(), address, sym->name(), fixup.sign_extended_addend());
        return ok();
      }
      LIEF_DEBUG("{}[  BIND] {}@0x{:x}: <missing symbol> / sign ext: {:x}",
                 DPREFIX, segment.name(), address, fixup.sign_extended_addend());
      LIEF_ERR("Missing symbol for binding at ordinal {}", bind_ordinal);
      return make_error_code(lief_errors::not_found);
    }

    // ---------- auth && !bind ----------
    const uint64_t target = imagebase + fixup.auth_rebase.target;

    auto reloc = std::make_unique<RelocationFixup>(ptr_fmt, imagebase);
    reloc->set(fixup.auth_rebase);
    reloc->address_      = chain_address;
    reloc->architecture_ = binary_->header().cpu_type();
    reloc->segment_      = &segment;
    reloc->size_         = ChainedPointerAnalysis::stride(ptr_fmt) * BYTE_BITS;
    reloc->offset_       = chain_offset;

    if (Section* section = binary_->section_from_virtual_address(address)) {
      reloc->section_ = section;
    } else {
      LIEF_ERR("Can't find the section associated with the virtual address 0x{:x}", address);
    }

    const auto it_symbol = memoized_symbols_by_address_.find(address);
    if (it_symbol != memoized_symbols_by_address_.end()) {
      reloc->symbol_ = it_symbol->second;
    }

    LIEF_DEBUG("{}[REBASE] {}@0x{:x}: 0x{:x}",
               DPREFIX, segment.name(), address, target);

    segment.relocations_.push_back(std::move(reloc));

    return ok();
  }

  // ---------- !auth && bind ----------
  if (fixup.auth_bind.bind) {
      uint32_t bind_ordinal = ptr_fmt == DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24 ?
                              fixup.bind24.ordinal :
                              fixup.bind.ordinal;

      if (bind_ordinal >= chained_fixups_->internal_bindings_.size()) {
        LIEF_WARN("Out of range bind ordinal {} (max {})",
                  bind_ordinal, chained_fixups_->internal_bindings_.size());
        return make_error_code(lief_errors::read_error);
      }

      std::unique_ptr<ChainedBindingInfoList>& local_binding = chained_fixups_->internal_bindings_[bind_ordinal];
      local_binding->segment_    = &segment;
      local_binding->ptr_format_ = ptr_fmt;
      ptr_fmt == DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_USERLAND24 ?
                 local_binding->set(fixup.bind24) : local_binding->set(fixup.bind);

      chained_fixups_->all_bindings_.push_back(std::make_unique<ChainedBindingInfo>(*local_binding));
      auto& binding_extra_info = chained_fixups_->all_bindings_.back();
      copy_from(*binding_extra_info, *local_binding);

      binding_extra_info->offset_ = chain_offset;
      binding_extra_info->address_ = chain_address;
      local_binding->elements_.push_back(binding_extra_info.get());

      if (Symbol* sym = binding_extra_info->symbol()) {
        LIEF_DEBUG("{}[  BIND] {}@0x{:x}: {} / sign ext: {:x}",
                   DPREFIX, segment.name(), address, sym->name(), fixup.sign_extended_addend());
        return ok();
      }

      LIEF_DEBUG("{}[  BIND] {}@0x{:x}: <missing symbol> / sign ext: {:x}",
                 DPREFIX, segment.name(), address, fixup.sign_extended_addend());
      LIEF_ERR("Missing symbol for binding at ordinal {}", bind_ordinal);
      return make_error_code(lief_errors::not_found);
  }

  // ---------- !auth && !bind ----------
  // See comment for: dyld_chained_ptr_generic64
  const uint64_t target = ptr_fmt == DYLD_CHAINED_PTR_FORMAT::PTR_64 ?
                          fixup.unpack_target() :
                          fixup.unpack_target() + imagebase;

  auto reloc = std::make_unique<RelocationFixup>(ptr_fmt, imagebase);
  reloc->set(fixup.rebase);
  reloc->address_      = chain_address;
  reloc->architecture_ = binary_->header().cpu_type();
  reloc->segment_      = &segment;
  reloc->size_         = ChainedPointerAnalysis::stride(ptr_fmt) * BYTE_BITS;
  reloc->offset_       = chain_offset;

  if (Section* section = binary_->section_from_virtual_address(address)) {
    reloc->section_ = section;
  } else {
    LIEF_ERR("Can't find the section associated with the virtual address 0x{:x}", address);
  }

  const auto it_symbol = memoized_symbols_by_address_.find(address);
  if (it_symbol != memoized_symbols_by_address_.end()) {
    reloc->symbol_ = it_symbol->second;
  }

  LIEF_DEBUG("{}[REBASE] {}@0x{:x}: 0x{:x}",
             DPREFIX, segment.name(), address, target);

  segment.relocations_.push_back(std::move(reloc));
  return ok();
}

/* Generic64 Fixup
 * =====================================
 */
ok_error_t BinaryParser::do_chained_fixup(SegmentCommand& segment,
                                          uint64_t chain_address, uint32_t chain_offset,
                                          const details::dyld_chained_starts_in_segment& seg_info,
                                          const details::dyld_chained_ptr_generic64& fixup)
{
  static constexpr const char DPREFIX[] = "          ";
  const auto ptr_fmt = static_cast<DYLD_CHAINED_PTR_FORMAT>(seg_info.pointer_format);
  const uint64_t imagebase = binary_->imagebase();

  const uint64_t address = imagebase + chain_offset;
  if (fixup.bind.bind > 0) {
    const uint64_t ordinal = fixup.bind.ordinal;
    if (ordinal >= chained_fixups_->internal_bindings_.size()) {
      LIEF_WARN("Out of range bind ordinal {} (max {})",
                ordinal, chained_fixups_->internal_bindings_.size());
      return make_error_code(lief_errors::read_error);
    }

    std::unique_ptr<ChainedBindingInfoList>& local_binding = chained_fixups_->internal_bindings_[ordinal];
    local_binding->segment_    = &segment;
    local_binding->ptr_format_ = ptr_fmt;
    local_binding->set(fixup.bind);

    chained_fixups_->all_bindings_.push_back(std::make_unique<ChainedBindingInfo>(*local_binding));
    auto& binding_extra_info = chained_fixups_->all_bindings_.back();
    copy_from(*binding_extra_info, *local_binding);

    binding_extra_info->offset_ = chain_offset;
    binding_extra_info->address_ = chain_address;
    local_binding->elements_.push_back(binding_extra_info.get());

    if (Symbol* sym = binding_extra_info->symbol()) {
      LIEF_DEBUG("{}[  BIND] {}@0x{:x}: {} / sign ext: {:x}",
                 DPREFIX, segment.name(), address, sym->name(), fixup.sign_extended_addend());
      return ok();
    }
    LIEF_DEBUG("{}[  BIND] {}@0x{:x}: <missing symbol> / sign ext: {:x}",
               DPREFIX, segment.name(), address, fixup.sign_extended_addend());
    LIEF_ERR("Missing symbol for binding at ordinal {}", ordinal);
    return make_error_code(lief_errors::not_found);
  }


  // The fixup is a rebase.

  /* In the dyld source code (MachOLoaded.cpp) there is
   * a distinction between with DYLD_CHAINED_PTR_64:
   *
   *  // plain rebase (old format target is vmaddr, new format target is offset)
   *  if ( segInfo->pointer_format == DYLD_CHAINED_PTR_64 )
   *      newValue = (void*)(fixupLoc->generic64.unpackedTarget()+slide);
   *  else
   *      newValue = (void*)((uintptr_t)this + fixupLoc->generic64.unpackedTarget());
   *
   * Not sure if it really matters in our case
   */
  const uint64_t target = ptr_fmt == DYLD_CHAINED_PTR_FORMAT::PTR_64 ?
                          fixup.unpack_target() :
                          fixup.unpack_target() + imagebase;
  auto reloc = std::make_unique<RelocationFixup>(ptr_fmt, imagebase);
  reloc->set(fixup.rebase);
  reloc->address_      = chain_address;
  reloc->architecture_ = binary_->header().cpu_type();
  reloc->segment_      = &segment;
  reloc->size_         = ChainedPointerAnalysis::stride(ptr_fmt) * BYTE_BITS;
  reloc->offset_       = chain_offset;

  if (Section* section = binary_->section_from_virtual_address(address)) {
    reloc->section_ = section;
  } else {
    LIEF_ERR("Can't find the section associated with the virtual address 0x{:x}", address);
  }

  const auto it_symbol = memoized_symbols_by_address_.find(address);
  if (it_symbol != memoized_symbols_by_address_.end()) {
    reloc->symbol_ = it_symbol->second;
  }

  LIEF_DEBUG("{}[REBASE] {}@0x{:x}: 0x{:x}",
             DPREFIX, segment.name(), address, target);

  segment.relocations_.push_back(std::move(reloc));
  return ok();
}


ok_error_t BinaryParser::do_chained_fixup(SegmentCommand& segment,
                                          uint64_t chain_address, uint32_t chain_offset,
                                          const details::dyld_chained_starts_in_segment& seg_info,
                                          const details::dyld_chained_ptr_generic32& fixup)
{
  static constexpr const char DPREFIX[] = "          ";
  const auto ptr_fmt = static_cast<DYLD_CHAINED_PTR_FORMAT>(seg_info.pointer_format);
  const uint64_t imagebase = binary_->imagebase();

  const uint64_t address = imagebase + chain_offset;
  if (fixup.bind.bind > 0) {
    const uint64_t ordinal = fixup.bind.ordinal;

    if (ordinal >= chained_fixups_->internal_bindings_.size()) {
      LIEF_WARN("Out of range bind ordinal {} (max {})",
                ordinal, chained_fixups_->internal_bindings_.size());
      return make_error_code(lief_errors::read_error);
    }

    std::unique_ptr<ChainedBindingInfoList>& local_binding = chained_fixups_->internal_bindings_[ordinal];
    local_binding->segment_    = &segment;
    local_binding->ptr_format_ = ptr_fmt;
    local_binding->set(fixup.bind);

    chained_fixups_->all_bindings_.push_back(std::make_unique<ChainedBindingInfo>(*local_binding));
    auto& binding_extra_info = chained_fixups_->all_bindings_.back();
    copy_from(*binding_extra_info, *local_binding);

    binding_extra_info->offset_ = chain_offset;
    binding_extra_info->address_ = chain_address;
    local_binding->elements_.push_back(binding_extra_info.get());

    if (Symbol* sym = binding_extra_info->symbol()) {
      LIEF_DEBUG("{}[  BIND] {}@0x{:x}: {} / sign ext: {:x}",
                 DPREFIX, segment.name(), address, sym->name(), fixup.bind.addend);
      return ok();
    }
    LIEF_DEBUG("{}[  BIND] {}@0x{:x}: <missing symbol> / sign ext: {:x}",
               DPREFIX, segment.name(), address, fixup.bind.addend);
    LIEF_ERR("Missing symbol for binding at ordinal {}", ordinal);
    return make_error_code(lief_errors::not_found);
  }
  // Rebase
  std::unique_ptr<RelocationFixup> reloc;
  if (fixup.rebase.target > seg_info.max_valid_pointer) {
    const uint32_t bias = (0x04000000 + seg_info.max_valid_pointer) / 2;
    const uint64_t target = fixup.rebase.target - bias;

    /* This is used to avoid storing bias information */
    const uint64_t fake_bias   = target - fixup.rebase.target;
    const uint64_t fake_target = target - fake_bias;
    details::dyld_chained_ptr_32_rebase fake_fixup = fixup.rebase;
    fake_fixup.target = fake_target;
    reloc = std::make_unique<RelocationFixup>(ptr_fmt, fake_bias);
    reloc->set(fake_fixup);
  } else {
    reloc = std::make_unique<RelocationFixup>(ptr_fmt, imagebase);
    reloc->set(fixup.rebase);
  }
  reloc->address_      = chain_address;
  reloc->architecture_ = binary_->header().cpu_type();
  reloc->segment_      = &segment;
  reloc->size_         = ChainedPointerAnalysis::stride(ptr_fmt) * BYTE_BITS;
  reloc->offset_       = chain_offset;

  if (Section* section = binary_->section_from_virtual_address(address)) {
    reloc->section_ = section;
  } else {
    LIEF_ERR("Can't find the section associated with the virtual address 0x{:x}", address);
  }

  const auto it_symbol = memoized_symbols_by_address_.find(address);
  if (it_symbol != memoized_symbols_by_address_.end()) {
    reloc->symbol_ = it_symbol->second;
  }

  LIEF_DEBUG("{}[REBASE] {}@0x{:x}: 0x{:x}",
             DPREFIX, segment.name(), address, reloc->target());

  segment.relocations_.push_back(std::move(reloc));
  return ok();
}

ok_error_t BinaryParser::do_chained_fixup(
    SegmentCommand& /*segment*/, uint64_t /*chain_address*/, uint32_t /*chain_offset*/,
    const details::dyld_chained_starts_in_segment& /*seg_info*/,
    const details::dyld_chained_ptr_arm64e_segmented& /*fixup*/)
{
  LIEF_ERR("Segmented chained rebase is not supported");
  return make_error_code(lief_errors::not_supported);
}


template<class MACHO_T>
ok_error_t BinaryParser::post_process(SymbolCommand& cmd) {
  LIEF_DEBUG("[^] Post processing LC_SYMTAB");
  using nlist_t = typename MACHO_T::nlist;

  cmd.original_nb_symbols_ = cmd.numberof_symbols();
  cmd.original_str_size_   = cmd.strings_size();

  SegmentCommand* nlist_linkedit = config_.from_dyld_shared_cache ?
    binary_->get_segment("__LINKEDIT") :
    binary_->segment_from_offset(cmd.symbol_offset());

  SegmentCommand* strings_linkedit = config_.from_dyld_shared_cache ?
    binary_->get_segment("__LINKEDIT") :
    binary_->segment_from_offset(cmd.strings_offset());


  if (nlist_linkedit == nullptr || strings_linkedit == nullptr) {
    std::vector<uint8_t> nlist_buffer;
    std::vector<uint8_t> strings_buffer;

    const uint64_t nlist_size = sizeof(nlist_t) * cmd.numberof_symbols();

    nlist_buffer.resize(nlist_size);
    strings_buffer.resize(cmd.strings_size());

    if (!stream_->peek_data(nlist_buffer, cmd.symbol_offset(), nlist_size)) {
      LIEF_ERR("Can't read nlist buffer at: 0x{:010x}", cmd.symbol_offset());
      return make_error_code(lief_errors::read_error);
    }

    if (!stream_->peek_data(strings_buffer, cmd.strings_offset(), cmd.strings_size())) {
      return make_error_code(lief_errors::read_error);
    }

    SpanStream nlist_s(nlist_buffer);
    SpanStream strings_s(strings_buffer);

    nlist_s.set_endian_swap(stream_->should_swap());
    strings_s.set_endian_swap(stream_->should_swap());
    return parse_symtab<MACHO_T>(cmd, nlist_s, strings_s);
  }

  /* n_list table */ {
    span<uint8_t> content = nlist_linkedit->writable_content();

    const uint64_t rel_offset = cmd.symbol_offset() - nlist_linkedit->file_offset();
    const size_t symtab_size = cmd.numberof_symbols() * sizeof(nlist_t);
    if (rel_offset > content.size() || (rel_offset + symtab_size) > content.size()) {
      LIEF_ERR("The LC_SYMTAB.n_list is out of bounds of the segment '{}'", nlist_linkedit->name());
      return make_error_code(lief_errors::read_out_of_bound);
    }

    cmd.symbol_table_ = content.subspan(rel_offset, symtab_size);

    if (LinkEdit::segmentof(*nlist_linkedit)) {
      static_cast<LinkEdit*>(nlist_linkedit)->symtab_ = &cmd;
    } else {
      LIEF_WARN("Weird: LC_SYMTAB.n_list is not in the __LINKEDIT segment");
    }
  }

  /* strtable */ {
    span<uint8_t> content = strings_linkedit->writable_content();

    const uint64_t rel_offset = cmd.strings_offset() - strings_linkedit->file_offset();
    const size_t strtab_size = cmd.strings_size();
    if (rel_offset > content.size() || (rel_offset + strtab_size) > content.size()) {
      LIEF_ERR("The LC_SYMTAB.strtab is out of bounds of the segment {}", strings_linkedit->name());
      return make_error_code(lief_errors::read_out_of_bound);
    }

    cmd.string_table_ = content.subspan(rel_offset, strtab_size);

    if (LinkEdit::segmentof(*strings_linkedit)) {
      static_cast<LinkEdit*>(strings_linkedit)->symtab_ = &cmd;
    } else {
      LIEF_WARN("Weird: LC_SYMTAB.strtab is not in the __LINKEDIT segment");
    }
  }

  SpanStream nlist_stream(cmd.symbol_table_);
  SpanStream string_stream(cmd.string_table_);

  nlist_stream.set_endian_swap(stream_->should_swap());
  string_stream.set_endian_swap(stream_->should_swap());

  return parse_symtab<MACHO_T>(cmd, nlist_stream, string_stream);
}


template<class MACHO_T>
ok_error_t BinaryParser::post_process(FunctionStarts& cmd) {
  LIEF_DEBUG("[^] Post processing LC_FUNCTION_STARTS");
  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_FUNCTION_STARTS (offset=0x{:016x})", cmd.data_offset());
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_FUNCTION_STARTS is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->fstarts_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_FUNCTION_STARTS is not in the __LINKEDIT segment ({})", linkedit->name());
  }

  SpanStream stream(cmd.content_);
  stream.set_endian_swap(stream_->should_swap());

  uint64_t value = 0;
  do {
    auto val = stream.read_uleb128();
    if (!val) {
      LIEF_WARN("Can't read value at offset: 0x{:010x} (#{} read)",
                stream.pos(), cmd.functions_.size()
      );
      return make_error_code(lief_errors::read_error);
    }

    if (*val == 0) {
      break;
    }

    value += *val;
    cmd.add_function(value);
  } while(stream);

  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::post_process(DataInCode& cmd) {
  LIEF_DEBUG("[^] Post processing LC_DATA_IN_CODE");
  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());
  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_DATA_IN_CODE");
    ScopedStream scoped(*stream_, cmd.data_offset());
    return parse_data_in_code<MACHO_T>(cmd, *scoped);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_DATA_IN_CODE is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->data_code_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_DATA_IN_CODE is not in the __LINKEDIT segment");
  }

  SpanStream stream(cmd.content_);
  return parse_data_in_code<MACHO_T>(cmd, stream);
}

template<class MACHO_T>
ok_error_t BinaryParser::post_process(SegmentSplitInfo& cmd) {
  LIEF_DEBUG("[^] Post processing LC_SEGMENT_SPLIT_INFO");
  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());
  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_SEGMENT_SPLIT_INFO");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_SEGMENT_SPLIT_INFO is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->seg_split_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_SEGMENT_SPLIT_INFO is not in the __LINKEDIT segment");
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::post_process(DynamicSymbolCommand& cmd) {
  LIEF_DEBUG("[^] Post processing LC_DYSYMTAB");
  std::vector<Symbol*> symtab;
  symtab.reserve(binary_->symbols_.size());
  size_t isym = 0;
  for (const std::unique_ptr<Symbol>& sym : binary_->symbols_) {
    if (sym->origin() != Symbol::ORIGIN::SYMTAB) {
      continue;
    }

    if (cmd.nb_local_symbols() > 0 &&
        cmd.idx_local_symbol() <= isym  && isym < (cmd.idx_local_symbol() + cmd.nb_local_symbols()))
    {
      sym->category_ = Symbol::CATEGORY::LOCAL;
    }

    if (cmd.nb_external_define_symbols() > 0 &&
        cmd.idx_external_define_symbol() <= isym  && isym < (cmd.idx_external_define_symbol() + cmd.nb_external_define_symbols()))
    {
      sym->category_ = Symbol::CATEGORY::EXTERNAL;
    }

    if (cmd.nb_undefined_symbols() > 0 &&
        cmd.idx_undefined_symbol() <= isym  && isym < (cmd.idx_undefined_symbol() + cmd.nb_undefined_symbols()))
    {
      sym->category_ = Symbol::CATEGORY::UNDEFINED;
    }
    symtab.push_back(sym.get());
    ++isym;
  }

  if (cmd.indirect_symbol_offset() == 0 || cmd.nb_indirect_symbols() == 0) {
    return ok();
  }

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.indirect_symbol_offset());

  if (linkedit == nullptr) {
    ScopedStream scoped(*stream_, cmd.indirect_symbol_offset());
    return parse_indirect_symbols(cmd, symtab, *scoped);
  }

  span<uint8_t> content = linkedit->writable_content();

  const size_t size = cmd.nb_indirect_symbols() * sizeof(uint32_t);

  const uint64_t rel_offset = cmd.indirect_symbol_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + size) > content.size()) {
    LIEF_ERR("LC_DYSYMTAB.indirect_symbols is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }
  span<uint8_t> indirect_sym_buffer = content.subspan(rel_offset, size);
  SpanStream indirect_sym_s(indirect_sym_buffer);
  indirect_sym_s.set_endian_swap(stream_->should_swap());
  return parse_indirect_symbols(cmd, symtab, indirect_sym_s);
}

template<class MACHO_T>
ok_error_t BinaryParser::post_process(LinkerOptHint& cmd) {
  LIEF_DEBUG("[^] Post processing LC_LINKER_OPTIMIZATION_HINT");
  if (binary_->header().file_type() == Header::FILE_TYPE::OBJECT) {
    return ok();
  }

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_LINKER_OPTIMIZATION_HINT");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_LINKER_OPTIMIZATION_HINT is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->linker_opt_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_LINKER_OPTIMIZATION_HINT is not in the __LINKEDIT segment");
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::post_process(AtomInfo& cmd) {
  LIEF_DEBUG("[^] Post processing LC_ATOM_INFO");

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_ATOM_INFO");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_ATOM_INFO is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->atom_info_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_ATOM_INFO is not in the __LINKEDIT segment");
  }
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::post_process(CodeSignature& cmd) {
  LIEF_DEBUG("[^] Post processing LC_CODE_SIGNATURE");
  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_CODE_SIGNATURE");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_CODE_SIGNATURE is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->code_sig_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_CODE_SIGNATURE is not in the __LINKEDIT segment");
  }
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::post_process(CodeSignatureDir& cmd) {
  LIEF_DEBUG("[^] Post processing LC_DYLIB_CODE_SIGN_DRS");
  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_DYLIB_CODE_SIGN_DRS");
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_DYLIB_CODE_SIGN_DRS is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->code_sig_dir_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_DYLIB_CODE_SIGN_DRS is not in the __LINKEDIT segment");
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::post_process(TwoLevelHints& cmd) {
  LIEF_DEBUG("[^] Post processing TWOLEVEL_HINTS");
  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.offset());

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_TWOLEVEL_HINTS");
    return make_error_code(lief_errors::not_found);
  }

  const size_t raw_size = cmd.original_nb_hints() * sizeof(uint32_t);
  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + raw_size) > content.size()) {
    LIEF_ERR("The LC_TWOLEVEL_HINTS is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, raw_size);

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->two_lvl_hint_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_TWOLEVEL_HINTS is not in the __LINKEDIT segment");
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::infer_indirect_bindings() {
  const DynamicSymbolCommand* dynsym = binary_->dynamic_symbol_command();
  if (dynsym == nullptr) {
    return ok();
  }

  for (SegmentCommand& segment : binary_->segments()) {
    for (Section& section : segment.sections()) {
      const Section::TYPE type = section.type();
      const bool might_have_indirect =
        type == Section::TYPE::NON_LAZY_SYMBOL_POINTERS ||
        type == Section::TYPE::LAZY_SYMBOL_POINTERS ||
        type == Section::TYPE::LAZY_DYLIB_SYMBOL_POINTERS ||
        type == Section::TYPE::THREAD_LOCAL_VARIABLE_POINTERS ||
        type == Section::TYPE::SYMBOL_STUBS;
      if (!might_have_indirect) {
        continue;
      }

      uint32_t stride = type == Section::TYPE::SYMBOL_STUBS ?
                        section.reserved2() :
                        sizeof(typename MACHO_T::uint);
      uint32_t count = section.size() / stride;
      uint32_t n = section.reserved1();
      auto indirect_syms = dynsym->indirect_symbols();

      for (size_t i = 0; i < count; ++i) {
        uint64_t addr = section.virtual_address() + i * stride;
        if (n + i >= indirect_syms.size()) {
          return make_error_code(lief_errors::corrupted);
        }
        Symbol& sym = indirect_syms[n + i];
        const int lib_ordinal = sym.library_ordinal();
        DylibCommand* dylib = nullptr;

        if (Symbol::is_valid_index_ordinal(lib_ordinal)) {
          const size_t idx = lib_ordinal - 1;
          if (idx < binding_libs_.size()) {
            dylib = binding_libs_[idx];
          }
        }
        auto binding = std::make_unique<IndirectBindingInfo>(
          segment, sym, lib_ordinal, dylib, addr
        );
        binary_->indirect_bindings_.push_back(std::move(binding));
      }
    }
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::parse_symtab(SymbolCommand&/*cmd*/,
                                      SpanStream& nlist_s, SpanStream& string_s)
{
  using nlist_t = typename MACHO_T::nlist;

  size_t idx = 0;
  while (nlist_s) {
    auto nlist = nlist_s.read<nlist_t>();
    if (!nlist) {
      LIEF_ERR("Can't read nlist #{}", idx);
      return make_error_code(lief_errors::read_error);
    }

    auto symbol = std::make_unique<Symbol>(*nlist);
    const uint32_t str_idx = nlist->n_strx;
    if (str_idx > 0) {
      auto name = string_s.peek_string_at(str_idx);
      if (name) {
        symbol->name(*name);
        memoized_symbols_[*name] = symbol.get();
      } else {
        LIEF_WARN("Can't read symbol's name for nlist #{}", idx);
      }
    }
    memoized_symbols_by_address_[symbol->value()] = symbol.get();
    binary_->symbols_.push_back(std::move(symbol));
    ++idx;
  }
  return ok();
}


template<class MACHO_T>
ok_error_t BinaryParser::parse_data_in_code(DataInCode& cmd, BinaryStream& stream) {
  const size_t nb_entries = cmd.data_size() / sizeof(details::data_in_code_entry);

  for (size_t i = 0; i < nb_entries; ++i) {
    auto entry = stream.read<details::data_in_code_entry>();
    if (!entry) {
      LIEF_ERR("Can't read data in code entry #{}", i);
      return make_error_code(lief_errors::read_error);
    }
    cmd.add(*entry);
  }
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::post_process(FunctionVariants& cmd) {
  LIEF_DEBUG("[^] Post processing LC_FUNCTION_VARIANTS");

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_FUNCTION_VARIANTS (offset=0x{:016x})", cmd.data_offset());
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_FUNCTION_VARIANTS is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->func_variants_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_FUNCTION_VARIANTS is not in the __LINKEDIT segment ({})", linkedit->name());
  }

  SpanStream stream(cmd.content_);
  stream.set_endian_swap(stream_->should_swap());
  cmd.runtime_table_ = FunctionVariants::parse_payload(stream);
  return ok();
}

template<class MACHO_T>
ok_error_t BinaryParser::post_process(FunctionVariantFixups& cmd) {
  LIEF_DEBUG("[^] Post processing LC_FUNCTION_VARIANT_FIXUPS");

  SegmentCommand* linkedit = config_.from_dyld_shared_cache ?
                             binary_->get_segment("__LINKEDIT") :
                             binary_->segment_from_offset(cmd.data_offset());

  if (linkedit == nullptr) {
    LIEF_WARN("Can't find the segment that contains the LC_FUNCTION_VARIANT_FIXUPS (offset=0x{:016x})", cmd.data_offset());
    return make_error_code(lief_errors::not_found);
  }

  span<uint8_t> content = linkedit->writable_content();

  const uint64_t rel_offset = cmd.data_offset() - linkedit->file_offset();
  if (rel_offset > content.size() || (rel_offset + cmd.data_size()) > content.size()) {
    LIEF_ERR("The LC_FUNCTION_VARIANT_FIXUPS is out of bounds of the segment '{}'", linkedit->name());
    return make_error_code(lief_errors::read_out_of_bound);
  }

  cmd.content_ = content.subspan(rel_offset, cmd.data_size());

  if (LinkEdit::segmentof(*linkedit)) {
    static_cast<LinkEdit*>(linkedit)->func_variant_fixups_ = &cmd;
  } else {
    LIEF_WARN("Weird: LC_FUNCTION_VARIANT_FIXUPS is not in the __LINKEDIT segment ({})", linkedit->name());
  }

  SpanStream stream(cmd.content_);
  stream.set_endian_swap(stream_->should_swap());
  // TODO

  return ok();
}


/* This method is needed since the C++ copy constructor of ChainedBindingInfo
 * does not (on purpose) copy the pointers associated with the object.
 * Thus we need this helper to maker sure that in the context on the parser,
 * the pointers are correctly copied.
 */
void BinaryParser::copy_from(ChainedBindingInfo& to, ChainedBindingInfo& from) {
  to.segment_ = from.segment_;
  to.symbol_  = from.symbol_;
  to.library_ = from.library_;
}

}
}
