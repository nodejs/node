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
#ifndef LIEF_MACHO_BUIDLER_H
#define LIEF_MACHO_BUIDLER_H

#include <vector>

#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"

#include "LIEF/iostream.hpp"

namespace LIEF {
namespace MachO {

class AtomInfo;
class Binary;
class BuildVersion;
class CodeSignature;
class CodeSignatureDir;
class DataInCode;
class DyldChainedFixups;
class DyldEnvironment;
class DyldExportsTrie;
class DyldInfo;
class DylibCommand;
class DylinkerCommand;
class DynamicSymbolCommand;
class EncryptionInfo;
class FatBinary;
class FunctionStarts;
class FunctionVariants;
class FunctionVariantFixups;
class LinkerOptHint;
class LoadCommand;
class MainCommand;
class NoteCommand;
class RPathCommand;
class Routine;
class SegmentSplitInfo;
class SourceVersion;
class SubClient;
class SubFramework;
class SymbolCommand;
class ThreadCommand;
class TwoLevelHints;
class VersionMin;

/// Class used to rebuild a Mach-O file
class LIEF_API Builder {
  public:
  /// Options to tweak the building process
  struct config_t {
    bool linkedit = true;
  };

  Builder() = delete;

  static ok_error_t write(Binary& binary, const std::string& filename);
  static ok_error_t write(Binary& binary, const std::string& filename, config_t config);

  static ok_error_t write(Binary& binary, std::vector<uint8_t>& out);
  static ok_error_t write(Binary& binary, std::vector<uint8_t>& out, config_t config);

  static ok_error_t write(Binary& binary, std::ostream& out);
  static ok_error_t write(Binary& binary, std::ostream& out, config_t config);

  static ok_error_t write(FatBinary& fat, const std::string& filename);
  static ok_error_t write(FatBinary& fat, const std::string& filename, config_t config);

  static ok_error_t write(FatBinary& fat, std::vector<uint8_t>& out);
  static ok_error_t write(FatBinary& fat, std::vector<uint8_t>& out, config_t config);

  static ok_error_t write(FatBinary& fat, std::ostream& out);
  static ok_error_t write(FatBinary& fat, std::ostream& out, config_t config);

  ~Builder();
  private:
  LIEF_LOCAL ok_error_t build();

  LIEF_LOCAL const std::vector<uint8_t>& get_build();
  LIEF_LOCAL ok_error_t write(const std::string& filename) const;
  LIEF_LOCAL ok_error_t write(std::ostream& os) const;

  LIEF_LOCAL Builder(Binary& binary, config_t config);
  LIEF_LOCAL Builder(std::vector<Binary*> binaries, config_t config);

  LIEF_LOCAL static std::vector<uint8_t> build_raw(Binary& binary, config_t config);
  LIEF_LOCAL static std::vector<uint8_t> build_raw(FatBinary& binary, config_t config);

  template<class T>
  LIEF_LOCAL static size_t get_cmd_size(const LoadCommand& cmd);

  template<typename T>
  LIEF_LOCAL ok_error_t build();

  LIEF_LOCAL ok_error_t build_fat();
  LIEF_LOCAL ok_error_t build_fat_header();
  LIEF_LOCAL ok_error_t build_load_commands();

  template<typename T>
  LIEF_LOCAL ok_error_t build_header();

  template<typename T>
  LIEF_LOCAL ok_error_t build_linkedit();

  template<typename T>
  LIEF_LOCAL ok_error_t build(DylibCommand& library);

  template<typename T>
  LIEF_LOCAL ok_error_t build(DylinkerCommand& linker);

  template<class T>
  LIEF_LOCAL ok_error_t build(VersionMin& version_min);

  template<class T>
  LIEF_LOCAL ok_error_t build(SourceVersion& source_version);

  template<class T>
  LIEF_LOCAL ok_error_t build(FunctionStarts& function_starts);

  template<class T>
  LIEF_LOCAL ok_error_t build(MainCommand& main_cmd);

  template<class T>
  LIEF_LOCAL ok_error_t build(NoteCommand& main_cmd);

  template<class T>
  LIEF_LOCAL ok_error_t build(Routine& routine);

  template<class T>
  LIEF_LOCAL ok_error_t build(RPathCommand& rpath_cmd);

  template<class T>
  LIEF_LOCAL ok_error_t build(DyldInfo& dyld_info);

  template<class T>
  LIEF_LOCAL ok_error_t build(SymbolCommand& symbol_command);

  template<class T>
  LIEF_LOCAL ok_error_t build(DynamicSymbolCommand& symbol_command);

  template<class T>
  LIEF_LOCAL ok_error_t build(DataInCode& datacode);

  template<class T>
  LIEF_LOCAL ok_error_t build(CodeSignature& code_signature);

  template<class T>
  LIEF_LOCAL ok_error_t build(SegmentSplitInfo& ssi);

  template<class T>
  LIEF_LOCAL ok_error_t build(SubFramework& sf);

  template<class T>
  LIEF_LOCAL ok_error_t build(SubClient& sf);

  template<class T>
  LIEF_LOCAL ok_error_t build(DyldEnvironment& de);

  template<class T>
  LIEF_LOCAL ok_error_t build(ThreadCommand& tc);

  template<class T>
  LIEF_LOCAL ok_error_t build(DyldChainedFixups& fixups);

  template<class T>
  LIEF_LOCAL ok_error_t build(DyldExportsTrie& exports);

  template<class T>
  LIEF_LOCAL ok_error_t build(TwoLevelHints& two);

  template<class T>
  LIEF_LOCAL ok_error_t build(LinkerOptHint& opt);

  template<class T>
  LIEF_LOCAL ok_error_t build(AtomInfo& opt);

  template<class T>
  LIEF_LOCAL ok_error_t build(CodeSignatureDir& sig);

  template<class T>
  LIEF_LOCAL ok_error_t build(EncryptionInfo& tc);

  template<class T>
  LIEF_LOCAL ok_error_t build(FunctionVariants& func);

  template<class T>
  LIEF_LOCAL ok_error_t build(FunctionVariantFixups& func);

  template <typename T>
  LIEF_LOCAL ok_error_t build_segments();

  template<class T>
  LIEF_LOCAL ok_error_t build(BuildVersion& bv);

  template <typename T>
  LIEF_LOCAL ok_error_t build_symbols();

  LIEF_LOCAL ok_error_t build_uuid();

  template <typename T>
  LIEF_LOCAL ok_error_t update_fixups(DyldChainedFixups& fixups);

  std::vector<Binary*> binaries_;
  Binary* binary_ = nullptr;
  mutable vector_iostream raw_;
  uint64_t linkedit_offset_ = 0;
  mutable vector_iostream linkedit_;
  config_t config_;
};

} // namespace MachO
} // namespace LIEF
#endif
