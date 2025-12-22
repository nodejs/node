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
#ifndef LIEF_ELF_JSON_INTERNAL_H
#define LIEF_ELF_JSON_INTERNAL_H

#include "LIEF/visibility.h"
#include "visitors/json.hpp"
#include "LIEF/ELF.hpp"

namespace LIEF {
namespace ELF {

class AndroidIdent;
class Binary;
class CoreAuxv;
class CoreFile;
class CorePrPsInfo;
class CorePrStatus;
class CoreSigInfo;
class DynamicEntry;
class DynamicEntryArray;
class DynamicEntryFlags;
class DynamicEntryLibrary;
class DynamicEntryRpath;
class DynamicEntryRunPath;
class DynamicSharedObject;
class GnuHash;
class Header;
class Note;
class NoteAbi;
class NoteDetails;
class NoteGnuProperty;
class Relocation;
class Section;
class Segment;
class Symbol;
class SymbolVersion;
class SymbolVersionAux;
class SymbolVersionAuxRequirement;
class SymbolVersionDefinition;
class SymbolVersionRequirement;
class SysvHash;

/// Class that implements the Visitor pattern to output
/// a JSON representation of an ELF object
class JsonVisitor : public LIEF::JsonVisitor {
  public:
  using LIEF::JsonVisitor::JsonVisitor;

  public:
  void visit(const Binary& binary)                  override;
  void visit(const Header& header)                  override;
  void visit(const Section& section)                override;
  void visit(const Segment& segment)                override;
  void visit(const DynamicEntry& entry)             override;
  void visit(const DynamicEntryArray& entry)        override;
  void visit(const DynamicEntryLibrary& entry)      override;
  void visit(const DynamicEntryRpath& entry)        override;
  void visit(const DynamicEntryRunPath& entry)      override;
  void visit(const DynamicSharedObject& entry)      override;
  void visit(const DynamicEntryFlags& entry)        override;
  void visit(const Symbol& symbol)                  override;
  void visit(const Relocation& relocation)          override;
  void visit(const SymbolVersion& sv)               override;
  void visit(const SymbolVersionAux& sv)            override;
  void visit(const SymbolVersionAuxRequirement& sv) override;
  void visit(const SymbolVersionRequirement& svr)   override;
  void visit(const SymbolVersionDefinition& svd)    override;
  void visit(const Note& note)                      override;
  void visit(const NoteAbi& note)                   override;
  void visit(const NoteGnuProperty& note)           override;
  void visit(const QNXStack& note)                  override;
  void visit(const AndroidIdent& note)              override;
  void visit(const CorePrPsInfo& pinfo)             override;
  void visit(const CorePrStatus& pstatus)           override;
  void visit(const CoreAuxv& auxv)                  override;
  void visit(const CoreSigInfo& siginfo)            override;
  void visit(const CoreFile& file)                  override;
  void visit(const GnuHash& gnuhash)                override;
  void visit(const SysvHash& sysvhash)              override;
};

}
}
#endif
