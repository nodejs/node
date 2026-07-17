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
#ifndef LIEF_PE_JSON_INTERNAL_H
#define LIEF_PE_JSON_INTERNAL_H

#include "LIEF/visibility.h"
#include "visitors/json.hpp"

namespace LIEF {

class Binary;
class Section;

namespace PE {

class Binary;
class DosHeader;
class RichHeader;
class RichEntry;
class Header;
class OptionalHeader;
class DataDirectory;
class Section;
class Relocation;
class RelocationEntry;
class Export;
class ExportEntry;
class TLS;
class Debug;
class CodeView;
class CodeViewPDB;
class Import;
class ImportEntry;
class DelayImport;
class DelayImportEntry;
class ResourceNode;
class ResourceData;
class ResourceDirectory;
class ResourcesManager;
class ResourceVersion;
class ResourceStringFileInfo;
class ResourceVarFileInfo;
class ResourceStringTable;
class ResourceAccelerator;
class ResourceIcon;
class ResourceDialogExtended;
class ResourceDialogRegular;
class Signature;
class x509;
class SignerInfo;
class ContentInfo;
class Attribute;
class ContentType;
class GenericType;
class MsCounterSign;
class MsSpcNestedSignature;
class MsSpcStatementType;
class PKCS9AtSequenceNumber;
class PKCS9CounterSignature;
class PKCS9MessageDigest;
class PKCS9SigningTime;
class SpcSpOpusInfo;
class SpcRelaxedPeMarkerCheck;
class SigningCertificateV2;
class CodeIntegrity;
class LoadConfiguration;
class Pogo;
class PogoEntry;
class Repro;

/// Class that implements the Visitor pattern to output
/// a JSON representation of a PE object
class JsonVisitor : public LIEF::JsonVisitor {
  public:
  using LIEF::JsonVisitor::JsonVisitor;

  public:
  void visit(const Binary& Binary)                        override;
  void visit(const DosHeader& dos_header)                 override;
  void visit(const RichHeader& rich_header)               override;
  void visit(const RichEntry& rich_entry)                 override;
  void visit(const Header& header)                        override;
  void visit(const OptionalHeader& optional_header)       override;
  void visit(const DataDirectory& data_directory)         override;
  void visit(const Section& section)                      override;
  void visit(const Relocation& relocation)                override;
  void visit(const RelocationEntry& relocation_entry)     override;
  void visit(const Export& export_)                       override;
  void visit(const ExportEntry& export_entry)             override;
  void visit(const TLS& tls)                              override;
  void visit(const Debug& debug)                          override;
  void visit(const CodeView& dv)                          override;
  void visit(const CodeViewPDB& cvpdb)                    override;
  void visit(const Import& import)                        override;
  void visit(const ImportEntry& import_entry)             override;
  void visit(const DelayImport& import)                   override;
  void visit(const DelayImportEntry& import_entry)        override;
  void visit(const ResourceNode& resource_node)           override;
  void visit(const ResourceData& resource_data)           override;
  void visit(const ResourceDirectory& resource_directory) override;
  void visit(const ResourcesManager& resources_manager)   override;
  void visit(const ResourceVersion& resource_version)     override;
  void visit(const ResourceStringFileInfo& resource_sfi)  override;
  void visit(const ResourceVarFileInfo& resource_vfi)     override;
  void visit(const ResourceStringTable& resource_st)      override;
  void visit(const ResourceAccelerator& resource_acc)     override;
  void visit(const ResourceIcon& resource_icon)           override;
  void visit(const ResourceDialogExtended& dialog)        override;
  void visit(const ResourceDialogRegular& dialog)         override;
  void visit(const Signature& signature)                  override;
  void visit(const x509& x509)                            override;
  void visit(const SignerInfo& signerinfo)                override;
  void visit(const ContentInfo& contentinfo)              override;
  void visit(const GenericContent& content)               override;
  void visit(const SpcIndirectData& content)              override;
  void visit(const Attribute& attr)                       override;
  void visit(const ContentType& attr)                     override;
  void visit(const GenericType& attr)                     override;
  void visit(const MsCounterSign& attr)                   override;
  void visit(const MsSpcNestedSignature& attr)            override;
  void visit(const MsSpcStatementType& attr)              override;
  void visit(const MsManifestBinaryID& attr)              override;
  void visit(const PKCS9AtSequenceNumber& attr)           override;
  void visit(const PKCS9CounterSignature& attr)           override;
  void visit(const PKCS9MessageDigest& attr)              override;
  void visit(const PKCS9SigningTime& attr)                override;
  void visit(const SpcSpOpusInfo& attr)                   override;
  void visit(const SpcRelaxedPeMarkerCheck& attr)         override;
  void visit(const SigningCertificateV2& attr)            override;
  void visit(const CodeIntegrity& code_integrity)         override;
  void visit(const LoadConfiguration& config)             override;

  void visit(const Pogo& pogo)        override;
  void visit(const PogoEntry& entry)  override;
  void visit(const Repro& entry)      override;

  void visit(const LIEF::Binary& binary)   override;
  void visit(const LIEF::Section& section) override;
};

}
}

#endif
