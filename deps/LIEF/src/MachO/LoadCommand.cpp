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
#include <ostream>

#include "frozen.hpp"
#include <spdlog/fmt/fmt.h>
#include "LIEF/MachO/hash.hpp"

#include "LIEF/MachO/LoadCommand.hpp"

#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/DynamicSymbolCommand.hpp"
#include "LIEF/MachO/SegmentSplitInfo.hpp"
#include "LIEF/MachO/FunctionStarts.hpp"
#include "LIEF/MachO/DataInCode.hpp"
#include "LIEF/MachO/SymbolCommand.hpp"
#include "LIEF/MachO/CodeSignature.hpp"

#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

LoadCommand::LoadCommand(const details::load_command& command) :
  command_{static_cast<LoadCommand::TYPE>(command.cmd)},
  size_{command.cmdsize}
{}

void LoadCommand::swap(LoadCommand& other) noexcept {
  std::swap(original_data_,  other.original_data_);
  std::swap(command_,        other.command_);
  std::swap(size_,           other.size_);
  std::swap(command_offset_, other.command_offset_);
}

void LoadCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

bool LoadCommand::is_linkedit_data(const LoadCommand& cmd) {
  if (DyldInfo::classof(&cmd))             return true;
  if (DyldExportsTrie::classof(&cmd))      return true;
  if (DyldChainedFixups::classof(&cmd))    return true;
  if (DynamicSymbolCommand::classof(&cmd)) return true;
  if (SegmentSplitInfo::classof(&cmd))     return true;
  if (FunctionStarts::classof(&cmd))       return true;
  if (DataInCode::classof(&cmd))           return true;
  if (SymbolCommand::classof(&cmd))        return true;
  if (CodeSignature::classof(&cmd))        return true;
  return false;
}


std::ostream& LoadCommand::print(std::ostream& os) const {
  os << fmt::format("Command: {}", to_string(command())) << '\n'
     << fmt::format("Offset:  0x{:x}", command_offset()) << '\n'
     << fmt::format("Size:    0x{:x}", size());

  return os;
}

const char* to_string(LoadCommand::TYPE e) {
  #define ENTRY(X) std::pair(LoadCommand::TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(SEGMENT),
    ENTRY(SYMTAB),
    ENTRY(SYMSEG),
    ENTRY(THREAD),
    ENTRY(UNIXTHREAD),
    ENTRY(LOADFVMLIB),
    ENTRY(IDFVMLIB),
    ENTRY(IDENT),
    ENTRY(FVMFILE),
    ENTRY(PREPAGE),
    ENTRY(DYSYMTAB),
    ENTRY(LOAD_DYLIB),
    ENTRY(ID_DYLIB),
    ENTRY(LOAD_DYLINKER),
    ENTRY(ID_DYLINKER),
    ENTRY(PREBOUND_DYLIB),
    ENTRY(ROUTINES),
    ENTRY(SUB_FRAMEWORK),
    ENTRY(SUB_UMBRELLA),
    ENTRY(SUB_CLIENT),
    ENTRY(SUB_LIBRARY),
    ENTRY(TWOLEVEL_HINTS),
    ENTRY(PREBIND_CKSUM),
    ENTRY(LOAD_WEAK_DYLIB),
    ENTRY(SEGMENT_64),
    ENTRY(ROUTINES_64),
    ENTRY(UUID),
    ENTRY(RPATH),
    ENTRY(CODE_SIGNATURE),
    ENTRY(SEGMENT_SPLIT_INFO),
    ENTRY(REEXPORT_DYLIB),
    ENTRY(LAZY_LOAD_DYLIB),
    ENTRY(ENCRYPTION_INFO),
    ENTRY(DYLD_INFO),
    ENTRY(DYLD_INFO_ONLY),
    ENTRY(LOAD_UPWARD_DYLIB),
    ENTRY(VERSION_MIN_MACOSX),
    ENTRY(VERSION_MIN_IPHONEOS),
    ENTRY(FUNCTION_STARTS),
    ENTRY(DYLD_ENVIRONMENT),
    ENTRY(MAIN),
    ENTRY(DATA_IN_CODE),
    ENTRY(SOURCE_VERSION),
    ENTRY(DYLIB_CODE_SIGN_DRS),
    ENTRY(ENCRYPTION_INFO_64),
    ENTRY(LINKER_OPTION),
    ENTRY(LINKER_OPTIMIZATION_HINT),
    ENTRY(VERSION_MIN_TVOS),
    ENTRY(VERSION_MIN_WATCHOS),
    ENTRY(NOTE),
    ENTRY(BUILD_VERSION),
    ENTRY(DYLD_EXPORTS_TRIE),
    ENTRY(DYLD_CHAINED_FIXUPS),
    ENTRY(FILESET_ENTRY),
    ENTRY(ATOM_INFO),
    ENTRY(FUNCTION_VARIANTS),
    ENTRY(FUNCTION_VARIANT_FIXUPS),
    ENTRY(TARGET_TRIPLE),
    ENTRY(LIEF_UNKNOWN),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}
