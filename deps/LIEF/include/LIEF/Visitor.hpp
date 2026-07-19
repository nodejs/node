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
#ifndef LIEF_VISITOR_H
#define LIEF_VISITOR_H
#include <set>
#include <utility>
#include <cstddef>

#include "LIEF/visibility.h"
#include "LIEF/visitor_macros.hpp"


namespace LIEF {
// Forward declarations
// ====================
class Object;
LIEF_ABSTRACT_FORWARD(Binary)
LIEF_ABSTRACT_FORWARD(Header)
LIEF_ABSTRACT_FORWARD(Section)
LIEF_ABSTRACT_FORWARD(Symbol)
LIEF_ABSTRACT_FORWARD(Relocation)
LIEF_ABSTRACT_FORWARD(Function)

// PE
// ===============================
LIEF_PE_FORWARD(Binary)
LIEF_PE_FORWARD(DelayImport)
LIEF_PE_FORWARD(DelayImportEntry)
LIEF_PE_FORWARD(DosHeader)
LIEF_PE_FORWARD(Header)
LIEF_PE_FORWARD(OptionalHeader)
LIEF_PE_FORWARD(RichHeader)
LIEF_PE_FORWARD(RichEntry)
LIEF_PE_FORWARD(DataDirectory)
LIEF_PE_FORWARD(Section)
LIEF_PE_FORWARD(Relocation)
LIEF_PE_FORWARD(RelocationEntry)
LIEF_PE_FORWARD(Export)
LIEF_PE_FORWARD(ExportEntry)
LIEF_PE_FORWARD(TLS)
LIEF_PE_FORWARD(Debug)
LIEF_PE_FORWARD(CodeView)
LIEF_PE_FORWARD(CodeViewPDB)
LIEF_PE_FORWARD(Import)
LIEF_PE_FORWARD(ImportEntry)
LIEF_PE_FORWARD(ResourceNode)
LIEF_PE_FORWARD(ResourceData)
LIEF_PE_FORWARD(ResourceDirectory)
LIEF_PE_FORWARD(ResourcesManager)
LIEF_PE_FORWARD(ResourceVersion)
LIEF_PE_FORWARD(ResourceStringFileInfo)
LIEF_PE_FORWARD(ResourceVarFileInfo)
LIEF_PE_FORWARD(ResourceIcon)
LIEF_PE_FORWARD(ResourceDialogExtended)
LIEF_PE_FORWARD(ResourceDialogRegular)
LIEF_PE_FORWARD(ResourceStringTable)
LIEF_PE_FORWARD(ResourceAccelerator)

LIEF_PE_FORWARD(Signature)
LIEF_PE_FORWARD(x509)
LIEF_PE_FORWARD(SignerInfo)
LIEF_PE_FORWARD(ContentInfo)
LIEF_PE_FORWARD(GenericContent)
LIEF_PE_FORWARD(SpcIndirectData)
LIEF_PE_FORWARD(Attribute)
LIEF_PE_FORWARD(ContentType)
LIEF_PE_FORWARD(GenericType)
LIEF_PE_FORWARD(MsCounterSign)
LIEF_PE_FORWARD(MsSpcNestedSignature)
LIEF_PE_FORWARD(MsSpcStatementType)
LIEF_PE_FORWARD(MsManifestBinaryID)
LIEF_PE_FORWARD(PKCS9AtSequenceNumber)
LIEF_PE_FORWARD(PKCS9CounterSignature)
LIEF_PE_FORWARD(PKCS9MessageDigest)
LIEF_PE_FORWARD(PKCS9SigningTime)
LIEF_PE_FORWARD(SpcSpOpusInfo)
LIEF_PE_FORWARD(SigningCertificateV2)
LIEF_PE_FORWARD(SpcRelaxedPeMarkerCheck)

LIEF_PE_FORWARD(CodeIntegrity)
LIEF_PE_FORWARD(LoadConfiguration)
LIEF_PE_FORWARD(Pogo)
LIEF_PE_FORWARD(PogoEntry)
LIEF_PE_FORWARD(Repro)

// ELF
// ==================================
LIEF_ELF_FORWARD(Binary)
LIEF_ELF_FORWARD(Header)
LIEF_ELF_FORWARD(Section)
LIEF_ELF_FORWARD(Segment)
LIEF_ELF_FORWARD(DynamicEntry)
LIEF_ELF_FORWARD(DynamicEntryArray)
LIEF_ELF_FORWARD(DynamicEntryLibrary)
LIEF_ELF_FORWARD(DynamicSharedObject)
LIEF_ELF_FORWARD(DynamicEntryRunPath)
LIEF_ELF_FORWARD(DynamicEntryRpath)
LIEF_ELF_FORWARD(DynamicEntryFlags)
LIEF_ELF_FORWARD(Symbol)
LIEF_ELF_FORWARD(Relocation)
LIEF_ELF_FORWARD(SymbolVersion)
LIEF_ELF_FORWARD(SymbolVersionRequirement)
LIEF_ELF_FORWARD(SymbolVersionDefinition)
LIEF_ELF_FORWARD(SymbolVersionAux)
LIEF_ELF_FORWARD(SymbolVersionAuxRequirement)
LIEF_ELF_FORWARD(Note)
LIEF_ELF_FORWARD(AndroidIdent)
LIEF_ELF_FORWARD(QNXStack)
LIEF_ELF_FORWARD(NoteAbi)
LIEF_ELF_FORWARD(NoteGnuProperty)
LIEF_ELF_FORWARD(CorePrPsInfo)
LIEF_ELF_FORWARD(CorePrStatus)
LIEF_ELF_FORWARD(CoreAuxv)
LIEF_ELF_FORWARD(CoreSigInfo)
LIEF_ELF_FORWARD(CoreFile)
LIEF_ELF_FORWARD(GnuHash)
LIEF_ELF_FORWARD(SysvHash)


// MACHO
// ===============================
LIEF_MACHO_FORWARD(Binary)
LIEF_MACHO_FORWARD(Header)
LIEF_MACHO_FORWARD(LoadCommand)
LIEF_MACHO_FORWARD(UUIDCommand)
LIEF_MACHO_FORWARD(SymbolCommand)
LIEF_MACHO_FORWARD(SegmentCommand)
LIEF_MACHO_FORWARD(Section)
LIEF_MACHO_FORWARD(MainCommand)
LIEF_MACHO_FORWARD(NoteCommand)
LIEF_MACHO_FORWARD(DynamicSymbolCommand)
LIEF_MACHO_FORWARD(DylinkerCommand)
LIEF_MACHO_FORWARD(DylibCommand)
LIEF_MACHO_FORWARD(ThreadCommand)
LIEF_MACHO_FORWARD(RPathCommand)
LIEF_MACHO_FORWARD(Symbol)
LIEF_MACHO_FORWARD(Relocation)
LIEF_MACHO_FORWARD(RelocationObject)
LIEF_MACHO_FORWARD(RelocationDyld)
LIEF_MACHO_FORWARD(RelocationFixup)
LIEF_MACHO_FORWARD(BindingInfo)
LIEF_MACHO_FORWARD(DyldBindingInfo)
LIEF_MACHO_FORWARD(DyldExportsTrie)
LIEF_MACHO_FORWARD(ChainedBindingInfo)
LIEF_MACHO_FORWARD(ExportInfo)
LIEF_MACHO_FORWARD(FunctionStarts)
LIEF_MACHO_FORWARD(CodeSignature)
LIEF_MACHO_FORWARD(DataInCode)
LIEF_MACHO_FORWARD(DataCodeEntry)
LIEF_MACHO_FORWARD(SourceVersion)
LIEF_MACHO_FORWARD(VersionMin)
LIEF_MACHO_FORWARD(SegmentSplitInfo)
LIEF_MACHO_FORWARD(SubFramework)
LIEF_MACHO_FORWARD(Routine)
LIEF_MACHO_FORWARD(DyldEnvironment)
LIEF_MACHO_FORWARD(EncryptionInfo)
LIEF_MACHO_FORWARD(BuildVersion)
LIEF_MACHO_FORWARD(BuildToolVersion)
LIEF_MACHO_FORWARD(FilesetCommand)
LIEF_MACHO_FORWARD(TwoLevelHints)
LIEF_MACHO_FORWARD(CodeSignatureDir)
LIEF_MACHO_FORWARD(LinkerOptHint)
LIEF_MACHO_FORWARD(UnknownCommand)

// OAT
// ===============================
LIEF_OAT_FORWARD(Binary)
LIEF_OAT_FORWARD(Header)
LIEF_OAT_FORWARD(DexFile)
LIEF_OAT_FORWARD(Method)
LIEF_OAT_FORWARD(Class)

// DEX
// ===============================
LIEF_DEX_FORWARD(File)
LIEF_DEX_FORWARD(Field)
LIEF_DEX_FORWARD(Method)
LIEF_DEX_FORWARD(Header)
LIEF_DEX_FORWARD(Class)
LIEF_DEX_FORWARD(CodeInfo)
LIEF_DEX_FORWARD(Type)
LIEF_DEX_FORWARD(Prototype)
LIEF_DEX_FORWARD(MapItem)
LIEF_DEX_FORWARD(MapList)

// VDEX
// ===============================
LIEF_VDEX_FORWARD(File)
LIEF_VDEX_FORWARD(Header)

// ART
// ===============================
LIEF_ART_FORWARD(File)
LIEF_ART_FORWARD(Header)


class LIEF_API Visitor {
  public:
  Visitor();
  virtual ~Visitor();

  virtual void operator()();

  template<typename Arg1, typename... Args>
  void operator()(Arg1&& arg1, Args&&... args);

  virtual void visit(const Object&);

  // Abstract Part
  // =============

  /// Method to visit a LIEF::Binary
  LIEF_ABSTRACT_VISITABLE(Binary)

  /// Method to visit a LIEF::Header
  LIEF_ABSTRACT_VISITABLE(Header)

  /// Method to visit a LIEF::Section
  LIEF_ABSTRACT_VISITABLE(Section)

  /// Method to visit a LIEF::Symbol
  LIEF_ABSTRACT_VISITABLE(Symbol)

  /// Method to visit a LIEF::Relocation
  LIEF_ABSTRACT_VISITABLE(Relocation)

  /// Method to visit a LIEF::Function
  LIEF_ABSTRACT_VISITABLE(Function)

  LIEF_ELF_VISITABLE(Binary)
  LIEF_ELF_VISITABLE(Header)
  LIEF_ELF_VISITABLE(Section)
  LIEF_ELF_VISITABLE(Segment)
  LIEF_ELF_VISITABLE(DynamicEntry)
  LIEF_ELF_VISITABLE(DynamicEntryArray)
  LIEF_ELF_VISITABLE(DynamicEntryLibrary)
  LIEF_ELF_VISITABLE(DynamicSharedObject)
  LIEF_ELF_VISITABLE(DynamicEntryRunPath)
  LIEF_ELF_VISITABLE(DynamicEntryRpath)
  LIEF_ELF_VISITABLE(DynamicEntryFlags)
  LIEF_ELF_VISITABLE(Symbol)
  LIEF_ELF_VISITABLE(Relocation)
  LIEF_ELF_VISITABLE(SymbolVersion)
  LIEF_ELF_VISITABLE(SymbolVersionRequirement)
  LIEF_ELF_VISITABLE(SymbolVersionDefinition)
  LIEF_ELF_VISITABLE(SymbolVersionAux)
  LIEF_ELF_VISITABLE(SymbolVersionAuxRequirement)
  LIEF_ELF_VISITABLE(Note)
  LIEF_ELF_VISITABLE(AndroidIdent)
  LIEF_ELF_VISITABLE(QNXStack)
  LIEF_ELF_VISITABLE(NoteAbi)
  LIEF_ELF_VISITABLE(NoteGnuProperty)
  LIEF_ELF_VISITABLE(CorePrPsInfo)
  LIEF_ELF_VISITABLE(CorePrStatus)
  LIEF_ELF_VISITABLE(CoreAuxv)
  LIEF_ELF_VISITABLE(CoreSigInfo)
  LIEF_ELF_VISITABLE(CoreFile)
  LIEF_ELF_VISITABLE(GnuHash)
  LIEF_ELF_VISITABLE(SysvHash)

  // PE Part
  // =======
  /// Method to visit a LIEF::PE::Binary
  LIEF_PE_VISITABLE(Binary)

  /// Method to visit a LIEF::PE::DosHeader
  LIEF_PE_VISITABLE(DosHeader)

  /// Method to visit a LIEF::PE:RichHeader
  LIEF_PE_VISITABLE(RichHeader)

  /// Method to visit a LIEF::PE:RichEntry
  LIEF_PE_VISITABLE(RichEntry)

  /// Method to visit a LIEF::PE::Header
  LIEF_PE_VISITABLE(Header)

  /// Method to visit a LIEF::PE::OptionalHeader
  LIEF_PE_VISITABLE(OptionalHeader)

  /// Method to visit a LIEF::PE::DataDirectory
  LIEF_PE_VISITABLE(DataDirectory)

  /// Method to visit a LIEF::PE::TLS
  LIEF_PE_VISITABLE(TLS)

  /// Method to visit a LIEF::PE::Section
  LIEF_PE_VISITABLE(Section)

  /// Method to visit a LIEF::PE::Relocation
  LIEF_PE_VISITABLE(Relocation)

  /// Method to visit a LIEF::PE::RelocationEntry
  LIEF_PE_VISITABLE(RelocationEntry)

  /// Method to visit a LIEF::PE::Export
  LIEF_PE_VISITABLE(Export)

  /// Method to visit a LIEF::PE::ExportEntry
  LIEF_PE_VISITABLE(ExportEntry)

  /// Method to visit a LIEF::PE::Debug
  LIEF_PE_VISITABLE(Debug)

  /// Method to visit a LIEF::PE::CodeView
  LIEF_PE_VISITABLE(CodeView)

  /// Method to visit a LIEF::PE::CodeViewPDB
  LIEF_PE_VISITABLE(CodeViewPDB)

  /// Method to visit a LIEF::PE::Import
  LIEF_PE_VISITABLE(Import)

  /// Method to visit a LIEF::PE::ImportEntry
  LIEF_PE_VISITABLE(ImportEntry)

  /// Method to visit a LIEF::PE::DelayImport
  LIEF_PE_VISITABLE(DelayImport)

  /// Method to visit a LIEF::PE::DelayImportEntry
  LIEF_PE_VISITABLE(DelayImportEntry)

  /// Method to visit a LIEF::PE::ResourceNode
  LIEF_PE_VISITABLE(ResourceNode)

  /// Method to visit a LIEF::PE::ResourceData
  LIEF_PE_VISITABLE(ResourceData)

  /// Method to visit a LIEF::PE::ResourceDirectory
  LIEF_PE_VISITABLE(ResourceDirectory)

  /// Method to visit a LIEF::PE::ResourceVersion
  LIEF_PE_VISITABLE(ResourcesManager)

  /// Method to visit a LIEF::PE::ResourceVersion
  LIEF_PE_VISITABLE(ResourceVersion)

  /// Method to visit a LIEF::PE::ResourceStringFileInfo
  LIEF_PE_VISITABLE(ResourceStringFileInfo)

  /// Method to visit a LIEF::PE::ResourceVarFileInfo
  LIEF_PE_VISITABLE(ResourceVarFileInfo)

  /// Method to visit a LIEF::PE::ResourceStringTable
  LIEF_PE_VISITABLE(ResourceStringTable)

  /// Method to visit a LIEF::PE::ResourceAccelerator
  LIEF_PE_VISITABLE(ResourceAccelerator)

  /// Method to visit a LIEF::PE::ResourceIcon
  LIEF_PE_VISITABLE(ResourceIcon)

  /// Method to visit a LIEF::PE::ResourceDialogExtended
  LIEF_PE_VISITABLE(ResourceDialogExtended)

  /// Method to visit a LIEF::PE::ResourceDialogRegular
  LIEF_PE_VISITABLE(ResourceDialogRegular)

  /// Method to visit a LIEF::PE::Signature
  LIEF_PE_VISITABLE(Signature)

  /// Method to visit a LIEF::PE::x509
  LIEF_PE_VISITABLE(x509)

  /// Method to visit a LIEF::PE::SignerInfo
  LIEF_PE_VISITABLE(SignerInfo)

  /// Method to visit a LIEF::PE::ContentInfo
  LIEF_PE_VISITABLE(ContentInfo)

  /// Method to visit a LIEF::PE::Attribute
  LIEF_PE_VISITABLE(Attribute)

  /// Method to visit a LIEF::PE::ContentType
  LIEF_PE_VISITABLE(ContentType)

  /// Method to visit a LIEF::PE::GenericContent
  LIEF_PE_VISITABLE(GenericContent)

  /// Method to visit a LIEF::PE::SpcIndirectData
  LIEF_PE_VISITABLE(SpcIndirectData)

  /// Method to visit a LIEF::PE::GenericType
  LIEF_PE_VISITABLE(GenericType)

  /// Method to visit a LIEF::PE::MsCounterSign
  LIEF_PE_VISITABLE(MsCounterSign)

  /// Method to visit a LIEF::PE::MsManifestBinaryID
  LIEF_PE_VISITABLE(MsManifestBinaryID)

  /// Method to visit a LIEF::PE::MsSpcNestedSignature
  LIEF_PE_VISITABLE(MsSpcNestedSignature)

  /// Method to visit a LIEF::PE::MsSpcStatementType
  LIEF_PE_VISITABLE(MsSpcStatementType)

  /// Method to visit a LIEF::PE::PKCS9AtSequenceNumber
  LIEF_PE_VISITABLE(PKCS9AtSequenceNumber)

  /// Method to visit a LIEF::PE::PKCS9CounterSignature
  LIEF_PE_VISITABLE(PKCS9CounterSignature)

  /// Method to visit a LIEF::PE::PKCS9MessageDigest
  LIEF_PE_VISITABLE(PKCS9MessageDigest)

  /// Method to visit a LIEF::PE::PKCS9SigningTime
  LIEF_PE_VISITABLE(PKCS9SigningTime)

  /// Method to visit a LIEF::PE::SpcSpOpusInfo
  LIEF_PE_VISITABLE(SpcSpOpusInfo)

  /// Method to visit a LIEF::PE::SpcRelaxedPeMarkerCheck
  LIEF_PE_VISITABLE(SpcRelaxedPeMarkerCheck)

  /// Method to visit a LIEF::PE::SigningCertificateV2
  LIEF_PE_VISITABLE(SigningCertificateV2)

  /// Method to visit a LIEF::PE::LoadConfiguration
  LIEF_PE_VISITABLE(LoadConfiguration)

  /// Method to visit a LIEF::PE::CodeIntegrity
  LIEF_PE_VISITABLE(CodeIntegrity)

  /// Method to visit a LIEF::PE::Pogo
  LIEF_PE_VISITABLE(Pogo)

  /// Method to visit a LIEF::PE::PogoEntry
  LIEF_PE_VISITABLE(PogoEntry)

  /// Method to visit a LIEF::PE::Repro
  LIEF_PE_VISITABLE(Repro)

  // MachO part
  // ==========
  /// Method to visit a LIEF::MachO::Binary
  LIEF_MACHO_VISITABLE(Binary)

  /// Method to visit a LIEF::MachO::Header
  LIEF_MACHO_VISITABLE(Header)

  /// Method to visit a LIEF::MachO::LoadCommand
  LIEF_MACHO_VISITABLE(LoadCommand)

  /// Method to visit a LIEF::MachO::UUIDCommand
  LIEF_MACHO_VISITABLE(UUIDCommand)

  /// Method to visit a LIEF::MachO::SymbolCommand
  LIEF_MACHO_VISITABLE(SymbolCommand)

  /// Method to visit a LIEF::MachO::SegmentCommand
  LIEF_MACHO_VISITABLE(SegmentCommand)

  /// Method to visit a LIEF::MachO::Section
  LIEF_MACHO_VISITABLE(Section)

  /// Method to visit a LIEF::MachO::MainCommand
  LIEF_MACHO_VISITABLE(MainCommand)

  /// Method to visit a LIEF::MachO::NoteCommand
  LIEF_MACHO_VISITABLE(NoteCommand)

  /// Method to visit a LIEF::MachO::DynamicSymbolCommand
  LIEF_MACHO_VISITABLE(DynamicSymbolCommand)

  /// Method to visit a LIEF::MachO::DylinkerCommand
  LIEF_MACHO_VISITABLE(DylinkerCommand)

  /// Method to visit a LIEF::MachO::DylibCommand
  LIEF_MACHO_VISITABLE(DylibCommand)

  /// Method to visit a LIEF::MachO::ThreadCommand
  LIEF_MACHO_VISITABLE(ThreadCommand)

  /// Method to visit a LIEF::MachO::RPathCommand
  LIEF_MACHO_VISITABLE(RPathCommand)

  /// Method to visit a LIEF::MachO::Symbol
  LIEF_MACHO_VISITABLE(Symbol)

  /// Method to visit a LIEF::MachO::Relocation
  LIEF_MACHO_VISITABLE(Relocation)

  /// Method to visit a LIEF::MachO::RelocationObject
  LIEF_MACHO_VISITABLE(RelocationObject)

  /// Method to visit a LIEF::MachO::RelocationDyld
  LIEF_MACHO_VISITABLE(RelocationDyld)

  /// Method to visit a LIEF::MachO::RelocationFixup
  LIEF_MACHO_VISITABLE(RelocationFixup)

  /// Method to visit a LIEF::MachO::BindingInfo
  LIEF_MACHO_VISITABLE(BindingInfo)

  /// Method to visit a LIEF::MachO::DyldBindingInfo
  LIEF_MACHO_VISITABLE(DyldBindingInfo)

  /// Method to visit a LIEF::MachO::ChainedBindingInfo
  LIEF_MACHO_VISITABLE(ChainedBindingInfo)

  /// Method to visit a LIEF::MachO::DyldExportsTrie
  LIEF_MACHO_VISITABLE(DyldExportsTrie)

  /// Method to visit a LIEF::MachO::ExportInfo
  LIEF_MACHO_VISITABLE(ExportInfo)

  /// Method to visit a LIEF::MachO::FunctionStarts
  LIEF_MACHO_VISITABLE(FunctionStarts)

  /// Method to visit a LIEF::MachO::CodeSignature
  LIEF_MACHO_VISITABLE(CodeSignature)

  /// Method to visit a LIEF::MachO::DataInCode
  LIEF_MACHO_VISITABLE(DataInCode)

  /// Method to visit a LIEF::MachO::DataCodeEntry
  LIEF_MACHO_VISITABLE(DataCodeEntry)

  /// Method to visit a LIEF::MachO::SourceVersion
  LIEF_MACHO_VISITABLE(SourceVersion)

  /// Method to visit a LIEF::MachO::VersionMin
  LIEF_MACHO_VISITABLE(VersionMin)

  /// Method to visit a LIEF::MachO::SegmentSplitInfo
  LIEF_MACHO_VISITABLE(SegmentSplitInfo)

  /// Method to visit a LIEF::MachO::SubFramework
  LIEF_MACHO_VISITABLE(SubFramework)

  /// Method to visit a LIEF::MachO::Routine
  LIEF_MACHO_VISITABLE(Routine)

  /// Method to visit a LIEF::MachO::DyldEnvironment
  LIEF_MACHO_VISITABLE(DyldEnvironment)

  /// Method to visit a LIEF::MachO::DyldEnvironment
  LIEF_MACHO_VISITABLE(EncryptionInfo)

  /// Method to visit a LIEF::MachO:BuildVersion:
  LIEF_MACHO_VISITABLE(BuildVersion)

  /// Method to visit a LIEF::MachO:BuildToolVersion:
  LIEF_MACHO_VISITABLE(BuildToolVersion)

  /// Method to visit a LIEF::MachO:BuildToolVersion:
  LIEF_MACHO_VISITABLE(FilesetCommand)

  /// Method to visit a LIEF::MachO::CodeSignatureDir
  LIEF_MACHO_VISITABLE(CodeSignatureDir)

  /// Method to visit a LIEF::MachO::TwoLevelHints
  LIEF_MACHO_VISITABLE(TwoLevelHints)

  /// Method to visit a LIEF::MachO::LinkerOptHint
  LIEF_MACHO_VISITABLE(LinkerOptHint)

  /// Method to visit a LIEF::MachO::UnknownCommand
  LIEF_MACHO_VISITABLE(UnknownCommand)

  // OAT part
  // ========

  /// Method to visit a LIEF::OAT::Binary
  LIEF_OAT_VISITABLE(Binary)

  /// Method to visit a LIEF::OAT::Header
  LIEF_OAT_VISITABLE(Header)

  /// Method to visit a LIEF::OAT::DexFile
  LIEF_OAT_VISITABLE(DexFile)

  /// Method to visit a LIEF::OAT::Class
  LIEF_OAT_VISITABLE(Class)

  /// Method to visit a LIEF::OAT::Method
  LIEF_OAT_VISITABLE(Method)


  // DEX part
  // ========

  /// Method to visit a LIEF::DEX::File
  LIEF_DEX_VISITABLE(File)

/// Method to visit a LIEF::DEX::Field
  LIEF_DEX_VISITABLE(Field)

  /// Method to visit a LIEF::DEX::Method
  LIEF_DEX_VISITABLE(Method)

  /// Method to visit a LIEF::DEX::Header
  LIEF_DEX_VISITABLE(Header)

  /// Method to visit a LIEF::DEX::Class
  LIEF_DEX_VISITABLE(Class)

  /// Method to visit a LIEF::DEX::CodeInfo
  LIEF_DEX_VISITABLE(CodeInfo)

  /// Method to visit a LIEF::DEX::Type
  LIEF_DEX_VISITABLE(Type)

  /// Method to visit a LIEF::DEX:Prototype:
  LIEF_DEX_VISITABLE(Prototype)

  /// Method to visit a LIEF::DEX:MapList:
  LIEF_DEX_VISITABLE(MapList)

  /// Method to visit a LIEF::DEX:MapItem:
  LIEF_DEX_VISITABLE(MapItem)

  // VDEX part
  // =========

  /// Method to visit a LIEF::VDEX::File
  LIEF_VDEX_VISITABLE(File)

  /// Method to visit a LIEF::VDEX::Header
  LIEF_VDEX_VISITABLE(Header)

  // ART part
  // =========

  /// Method to visit a LIEF::ART::File
  LIEF_ART_VISITABLE(File)

  /// Method to visit a LIEF::ART::Header
  LIEF_ART_VISITABLE(Header)

  template<class T>
  void dispatch(const T& obj);


  private:
  std::set<size_t> visited_;
};



template<typename Arg1, typename... Args>
void Visitor::operator()(Arg1&& arg1, Args&&... args) {
  dispatch(std::forward<Arg1>(arg1));
  operator()(std::forward<Args>(args)... );
}

template<class T>
void Visitor::dispatch(const T& obj) {
  auto hash = reinterpret_cast<size_t>(&obj);
  if (visited_.find(hash) != std::end(visited_)) {
    // Already visited
    return;
  }

  visited_.insert(hash);
  visit(obj);
}

}
#endif
