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
#ifndef LIEF_ELF_HASH_H
#define LIEF_ELF_HASH_H

#include "LIEF/visibility.h"
#include "LIEF/hash.hpp"

namespace LIEF {
namespace ELF {

class Binary;
class Header;
class Section;
class Segment;
class DynamicEntry;
class DynamicEntryArray;
class DynamicEntryLibrary;
class DynamicEntryRpath;
class DynamicEntryRunPath;
class DynamicSharedObject;
class DynamicEntryFlags;
class Symbol;
class Relocation;
class SymbolVersion;
class SymbolVersionAux;
class SymbolVersionAuxRequirement;
class SymbolVersionRequirement;
class SymbolVersionDefinition;
class Note;
class NoteDetails;
class AndroidNote;
class NoteAbi;
class NoteGnuProperty;
class CorePrPsInfo;
class CorePrStatus;
class CoreAuxv;
class CoreSigInfo;
class CoreFile;
class GnuHash;
class SysvHash;

/// Class which implements a visitor to compute
/// a **deterministic** hash for LIEF ELF objects
class LIEF_API Hash : public LIEF::Hash {
  public:
  static LIEF::Hash::value_type hash(const Object& obj);

  public:
  using LIEF::Hash::Hash;
  using LIEF::Hash::visit;

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
  void visit(const AndroidIdent& note)              override;
  void visit(const QNXStack& note)                  override;
  void visit(const NoteAbi& note)                   override;
  void visit(const NoteGnuProperty& note)           override;
  void visit(const CorePrPsInfo& pinfo)             override;
  void visit(const CorePrStatus& pstatus)           override;
  void visit(const CoreAuxv& auxv)                  override;
  void visit(const CoreSigInfo& siginfo)            override;
  void visit(const CoreFile& file)                  override;
  void visit(const GnuHash& gnuhash)                override;
  void visit(const SysvHash& sysvhash)              override;

  ~Hash() override;
};

}
}

#endif
