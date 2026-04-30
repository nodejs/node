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
#ifndef LIEF_MACHO_HASH_H
#define LIEF_MACHO_HASH_H

#include "LIEF/visibility.h"
#include "LIEF/hash.hpp"

namespace LIEF {
namespace MachO {

class Binary;
class BindingInfo;
class BuildToolVersion;
class BuildVersion;
class ChainedBindingInfo;
class CodeSignature;
class CodeSignatureDir;
class DataCodeEntry;
class DataInCode;
class DyldBindingInfo;
class DyldEnvironment;
class DyldExportsTrie;
class DylibCommand;
class DylinkerCommand;
class DynamicSymbolCommand;
class EncryptionInfo;
class ExportInfo;
class FilesetCommand;
class FunctionStarts;
class Header;
class LinkerOptHint;
class LoadCommand;
class MainCommand;
class NoteCommand;
class RPathCommand;
class Relocation;
class RelocationDyld;
class RelocationFixup;
class RelocationObject;
class Section;
class SegmentCommand;
class SegmentSplitInfo;
class SourceVersion;
class Routine;
class SubFramework;
class Symbol;
class SymbolCommand;
class ThreadCommand;
class TwoLevelHints;
class UUIDCommand;
class VersionMin;
class UnknownCommand;

/// Class which implements a visitor to compute
/// a **deterministic** hash for LIEF MachO objects
class LIEF_API Hash : public LIEF::Hash {
  public:
  static LIEF::Hash::value_type hash(const Object& obj);

  public:
  using LIEF::Hash::Hash;
  using LIEF::Hash::visit;

  public:
  void visit(const Binary& binary)                        override;
  void visit(const BindingInfo& binding)                  override;
  void visit(const BuildToolVersion& e)                   override;
  void visit(const BuildVersion& e)                       override;
  void visit(const ChainedBindingInfo& binding)           override;
  void visit(const CodeSignature& cs)                     override;
  void visit(const CodeSignatureDir& e)                   override;
  void visit(const DataCodeEntry& dce)                    override;
  void visit(const DataInCode& dic)                       override;
  void visit(const DyldBindingInfo& binding)              override;
  void visit(const DyldEnvironment& sf)                   override;
  void visit(const DyldExportsTrie& trie)                 override;
  void visit(const DylibCommand& dylib)                   override;
  void visit(const DylinkerCommand& dylinker)             override;
  void visit(const DynamicSymbolCommand& dynamic_symbol)  override;
  void visit(const EncryptionInfo& e)                     override;
  void visit(const ExportInfo& einfo)                     override;
  void visit(const FilesetCommand& e)                     override;
  void visit(const FunctionStarts& fs)                    override;
  void visit(const Header& header)                        override;
  void visit(const LinkerOptHint& e)                      override;
  void visit(const LoadCommand& cmd)                      override;
  void visit(const MainCommand& maincmd)                  override;
  void visit(const NoteCommand& note)                     override;
  void visit(const Routine& rpath)                        override;
  void visit(const RPathCommand& rpath)                   override;
  void visit(const Relocation& relocation)                override;
  void visit(const RelocationDyld& rdyld)                 override;
  void visit(const RelocationFixup& fixup)                override;
  void visit(const RelocationObject& robject)             override;
  void visit(const Section& section)                      override;
  void visit(const SegmentCommand& segment)               override;
  void visit(const SegmentSplitInfo& ssi)                 override;
  void visit(const SourceVersion& sv)                     override;
  void visit(const SubFramework& sf)                      override;
  void visit(const Symbol& symbol)                        override;
  void visit(const SymbolCommand& symbol)                 override;
  void visit(const ThreadCommand& threadcmd)              override;
  void visit(const TwoLevelHints& e)                      override;
  void visit(const UUIDCommand& uuid)                     override;
  void visit(const VersionMin& vmin)                      override;
  void visit(const UnknownCommand& ukn)                   override;

  ~Hash() override;
};

}
}

#endif
