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

#include "PE/json_internal.hpp"
#include "LIEF/PE.hpp"

#include "Object.tcc"

namespace LIEF {
namespace PE {

std::string to_hex(const char c) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << std::hex << (0xff & (unsigned int)c);
  return std::string("\\x") + ss.str();
}

std::string escape_non_ascii(const std::string& s) {
  std::string result;
  const auto len = s.size();
  for (auto i = 0u; i < len; i++) {
    const auto c = s[i];
    if (c < 32 || c >= 127) {
      result += to_hex(c);
    } else {
      result.push_back(c);
    }
  }
  return result;
}

void JsonVisitor::visit(const Binary& binary) {
  node_["entrypoint"]   = binary.entrypoint();
  node_["virtual_size"] = binary.virtual_size();

  // DOS Header
  JsonVisitor dos_header_visitor;
  dos_header_visitor(binary.dos_header());

  // Rich Header
  if (const RichHeader* rheader = binary.rich_header()) {
    JsonVisitor visitor;
    visitor(*rheader);
    node_["rich_header"] = visitor.get();
  }

  // PE header
  JsonVisitor header_visitor;
  header_visitor(binary.header());

  // PE Optional Header
  JsonVisitor optional_header_visitor;
  optional_header_visitor(binary.optional_header());

  node_["dos_header"]      = dos_header_visitor.get();
  node_["header"]          = header_visitor.get();
  node_["optional_header"] = optional_header_visitor.get();

  // Data directories
  std::vector<json> data_directories;
  for (const DataDirectory& data_directory : binary.data_directories()) {
    JsonVisitor visitor;
    visitor(data_directory);
    data_directories.emplace_back(visitor.get());
  }
  node_["data_directories"] = data_directories;


  // Section
  std::vector<json> sections;
  for (const Section& section : binary.sections()) {
    JsonVisitor visitor;
    visitor(section);
    sections.emplace_back(visitor.get());
  }
  node_["sections"] = sections;

  // Relocations
  if (binary.has_relocations()) {
    std::vector<json> relocations;
    for (const Relocation& relocation : binary.relocations()) {
      JsonVisitor visitor;
      visitor(relocation);
      relocations.emplace_back(visitor.get());
    }
    node_["relocations"] = relocations;
  }

  // TLS
  if (const TLS* tls_object = binary.tls()) {
    JsonVisitor visitor;
    visitor(*tls_object);
    node_["tls"] = visitor.get();
  }


  // Exports
  if (const Export* exp = binary.get_export()) {
    JsonVisitor visitor;
    visitor(*exp);
    node_["export"] = visitor.get();
  }


  // Debug
  if (binary.has_debug()) {
    std::vector<json> debug_entries;
    for (const Debug& debug : binary.debug()) {
      JsonVisitor visitor;
      visitor(debug);
      debug_entries.emplace_back(visitor.get());
    }
    node_["debug"] = debug_entries;
  }

  // Imports
  if (binary.has_imports()) {
    std::vector<json> imports;
    for (const Import& import : binary.imports()) {
      JsonVisitor visitor;
      visitor(import);
      imports.emplace_back(visitor.get());
    }
    node_["imports"] = imports;
  }

  // Delay Imports
  if (binary.has_delay_imports()) {
    std::vector<json> imports;
    for (const DelayImport& import : binary.delay_imports()) {
      JsonVisitor visitor;
      visitor(import);
      imports.emplace_back(visitor.get());
    }
    node_["delay_imports"] = imports;
  }

  // Resources
  if (binary.has_resources()) {
    JsonVisitor visitor;
    binary.resources()->accept(visitor);

    JsonVisitor manager_visitor;

    if (auto manager = binary.resources_manager()) {
      manager->accept(manager_visitor);
    }

    node_["resources_tree"]    = visitor.get();
    node_["resources_manager"] = manager_visitor.get();
  }


  // Signatures
  std::vector<json> sigs;
  if (binary.has_signatures()) {
    for (const Signature& sig : binary.signatures()) {
      JsonVisitor visitor;
      visitor(sig);
      sigs.push_back(visitor.get());
    }
    node_["signatures"] = sigs;
  }

  // Load Configuration
  if (binary.has_configuration()) {
    JsonVisitor visitor;
    const LoadConfiguration* config = binary.load_configuration();
    config->accept(visitor);
    node_["load_configuration"] = visitor.get();
  }

}


void JsonVisitor::visit(const DosHeader& dos_header) {
  node_["magic"]                       = dos_header.magic();
  node_["used_bytes_in_last_page"]     = dos_header.used_bytes_in_last_page();
  node_["file_size_in_pages"]          = dos_header.file_size_in_pages();
  node_["numberof_relocation"]         = dos_header.numberof_relocation();
  node_["header_size_in_paragraphs"]   = dos_header.header_size_in_paragraphs();
  node_["minimum_extra_paragraphs"]    = dos_header.minimum_extra_paragraphs();
  node_["maximum_extra_paragraphs"]    = dos_header.maximum_extra_paragraphs();
  node_["initial_relative_ss"]         = dos_header.initial_relative_ss();
  node_["initial_sp"]                  = dos_header.initial_sp();
  node_["checksum"]                    = dos_header.checksum();
  node_["initial_ip"]                  = dos_header.initial_ip();
  node_["initial_relative_cs"]         = dos_header.initial_relative_cs();
  node_["addressof_relocation_table"]  = dos_header.addressof_relocation_table();
  node_["overlay_number"]              = dos_header.overlay_number();
  node_["reserved"]                    = dos_header.reserved();
  node_["oem_id"]                      = dos_header.oem_id();
  node_["oem_info"]                    = dos_header.oem_info();
  node_["reserved2"]                   = dos_header.reserved2();
  node_["addressof_new_exeheader"]     = dos_header.addressof_new_exeheader();
}

void JsonVisitor::visit(const RichHeader& rich_header) {
  std::vector<json> entries;
  for (const RichEntry& entry : rich_header.entries()) {
    JsonVisitor visitor;
    visitor(entry);
    entries.emplace_back(visitor.get());
  }

  node_["key"]     = rich_header.key();
  node_["entries"] = entries;
}

void JsonVisitor::visit(const RichEntry& rich_entry) {

  node_["id"]       = rich_entry.id();
  node_["build_id"] = rich_entry.build_id();
  node_["count"]    = rich_entry.count();
}

void JsonVisitor::visit(const Header& header) {

  node_["signature"]              = header.signature();
  node_["machine"]                = to_string(header.machine());
  node_["numberof_sections"]      = header.numberof_sections();
  node_["time_date_stamp"]        = header.time_date_stamp();
  node_["pointerto_symbol_table"] = header.pointerto_symbol_table();
  node_["numberof_symbols"]       = header.numberof_symbols();
  node_["sizeof_optional_header"] = header.sizeof_optional_header();
  node_["characteristics"]        = static_cast<size_t>(header.characteristics());
}

void JsonVisitor::visit(const OptionalHeader& optional_header) {

  node_["magic"]                          = to_string(optional_header.magic());
  node_["major_linker_version"]           = optional_header.major_linker_version();
  node_["minor_linker_version"]           = optional_header.minor_linker_version();
  node_["sizeof_code"]                    = optional_header.sizeof_code();
  node_["sizeof_initialized_data"]        = optional_header.sizeof_initialized_data();
  node_["sizeof_uninitialized_data"]      = optional_header.sizeof_uninitialized_data();
  node_["addressof_entrypoint"]           = optional_header.addressof_entrypoint();
  node_["baseof_code"]                    = optional_header.baseof_code();
  if (optional_header.magic() == PE_TYPE::PE32) {
    node_["baseof_data"]     = optional_header.baseof_data();
  }
  node_["imagebase"]                      = optional_header.imagebase();
  node_["section_alignment"]              = optional_header.section_alignment();
  node_["file_alignment"]                 = optional_header.file_alignment();
  node_["major_operating_system_version"] = optional_header.major_operating_system_version();
  node_["minor_operating_system_version"] = optional_header.minor_operating_system_version();
  node_["major_image_version"]            = optional_header.major_image_version();
  node_["minor_image_version"]            = optional_header.minor_image_version();
  node_["major_subsystem_version"]        = optional_header.major_subsystem_version();
  node_["minor_subsystem_version"]        = optional_header.minor_subsystem_version();
  node_["win32_version_value"]            = optional_header.win32_version_value();
  node_["sizeof_image"]                   = optional_header.sizeof_image();
  node_["sizeof_headers"]                 = optional_header.sizeof_headers();
  node_["checksum"]                       = optional_header.checksum();
  node_["subsystem"]                      = to_string(optional_header.subsystem());
  node_["dll_characteristics"]            = optional_header.dll_characteristics();
  node_["sizeof_stack_reserve"]           = optional_header.sizeof_stack_reserve();
  node_["sizeof_stack_commit"]            = optional_header.sizeof_stack_commit();
  node_["sizeof_heap_reserve"]            = optional_header.sizeof_heap_reserve();
  node_["sizeof_heap_commit"]             = optional_header.sizeof_heap_commit();
  node_["loader_flags"]                   = optional_header.loader_flags();
  node_["numberof_rva_and_size"]          = optional_header.numberof_rva_and_size();
}

void JsonVisitor::visit(const DataDirectory& data_directory) {

  node_["RVA"]  = data_directory.RVA();
  node_["size"] = data_directory.size();
  node_["type"] = to_string(data_directory.type());
  if (data_directory.has_section()) {
    node_["section"] = escape_non_ascii(data_directory.section()->name());
  }
}

void JsonVisitor::visit(const Section& section) {
  std::vector<json> characteristics;
  for (Section::CHARACTERISTICS c : section.characteristics_list()) {
    characteristics.emplace_back(to_string(c));
  }

  node_["name"]                   = escape_non_ascii(section.name());
  node_["pointerto_relocation"]   = section.pointerto_relocation();
  node_["pointerto_line_numbers"] = section.pointerto_line_numbers();
  node_["numberof_relocations"]   = section.numberof_relocations();
  node_["numberof_line_numbers"]  = section.numberof_line_numbers();
  node_["entropy"]                = section.entropy();
  node_["characteristics"]        = characteristics;
}

void JsonVisitor::visit(const Relocation& relocation) {

  std::vector<json> entries;
  for (const RelocationEntry& entry : relocation.entries()) {
    JsonVisitor visitor;
    visitor(entry);
    entries.emplace_back(visitor.get());
  }

  node_["virtual_address"] = relocation.virtual_address();
  node_["block_size"]      = relocation.block_size();
  node_["entries"]         = entries;
}

void JsonVisitor::visit(const RelocationEntry& relocation_entry) {

  node_["data"]     = relocation_entry.data();
  node_["position"] = relocation_entry.position();
  node_["type"]     = to_string(relocation_entry.type());
}

void JsonVisitor::visit(const Export& export_) {

  std::vector<json> entries;
  for (const ExportEntry& entry : export_.entries()) {
    JsonVisitor visitor;
    visitor(entry);
    entries.emplace_back(visitor.get());
  }
  node_["export_flags"]  = export_.export_flags();
  node_["timestamp"]     = export_.timestamp();
  node_["major_version"] = export_.major_version();
  node_["minor_version"] = export_.minor_version();
  node_["ordinal_base"]  = export_.ordinal_base();
  node_["name"]          = escape_non_ascii(export_.name());
  node_["entries"]       = entries;
}

void JsonVisitor::visit(const ExportEntry& export_entry) {

  node_["name"]      = escape_non_ascii(export_entry.name());
  node_["ordinal"]   = export_entry.ordinal();
  node_["address"]   = export_entry.address();
  node_["is_extern"] = export_entry.is_extern();

  if (export_entry.is_forwarded()) {
    const ExportEntry::forward_information_t& fwd_info = export_entry.forward_information();
    node_["forward_information"] = {
      {"library",  fwd_info.library},
      {"function", fwd_info.function},
    };
  }
}

void JsonVisitor::visit(const TLS& tls) {

  node_["callbacks"]           = tls.callbacks();
  node_["addressof_raw_data"]  = std::vector<uint64_t>{tls.addressof_raw_data().first, tls.addressof_raw_data().second};
  node_["addressof_index"]     = tls.addressof_index();
  node_["addressof_callbacks"] = tls.addressof_callbacks();
  node_["sizeof_zero_fill"]    = tls.sizeof_zero_fill();
  node_["characteristics"]     = tls.characteristics();

  if (tls.has_data_directory()) {
    node_["data_directory"] = to_string(tls.directory()->type());
  }

  if (tls.has_section()) {
    node_["section"] = escape_non_ascii(tls.section()->name());
  }
}

void JsonVisitor::visit(const Debug& debug) {
  node_["characteristics"]   = debug.characteristics();
  node_["timestamp"]         = debug.timestamp();
  node_["major_version"]     = debug.major_version();
  node_["minor_version"]     = debug.minor_version();
  node_["type"]              = to_string(debug.type());
  node_["sizeof_data"]       = debug.sizeof_data();
  node_["addressof_rawdata"] = debug.addressof_rawdata();
  node_["pointerto_rawdata"] = debug.pointerto_rawdata();
}

void JsonVisitor::visit(const CodeView& cv) {
  visit(static_cast<const Debug&>(cv));
  node_["cv_signature"] = to_string(cv.signature());
}

void JsonVisitor::visit(const CodeViewPDB& cvpdb) {
  visit(static_cast<const CodeView&>(cvpdb));
  node_["signature"] = cvpdb.signature();
  node_["age"]       = cvpdb.age();
  node_["filename"]  = escape_non_ascii(cvpdb.filename());
}

void JsonVisitor::visit(const DelayImport& import) {

  std::vector<json> entries;
  for (const DelayImportEntry& entry : import.entries()) {
    JsonVisitor visitor;
    visitor(entry);
    entries.emplace_back(visitor.get());
  }
  node_["attribute"]   = import.attribute();
  node_["name"]        = import.name();
  node_["handle"]      = import.handle();
  node_["iat"]         = import.iat();
  node_["names_table"] = import.names_table();
  node_["biat"]        = import.biat();
  node_["uiat"]        = import.uiat();
  node_["timestamp"]   = import.timestamp();
  node_["entries"]     = entries;
}

void JsonVisitor::visit(const DelayImportEntry& import_entry) {
  if (import_entry.is_ordinal()) {
    node_["ordinal"] = import_entry.ordinal();
  } else {
    node_["name"] = import_entry.name();
  }

  node_["value"]       = import_entry.value();
  node_["iat_address"] = import_entry.iat_value();
  node_["data"]        = import_entry.data();
  node_["hint"]        = import_entry.hint();
}


void JsonVisitor::visit(const Import& import) {

  std::vector<json> entries;
  for (const ImportEntry& entry : import.entries()) {
    JsonVisitor visitor;
    visitor(entry);
    entries.emplace_back(visitor.get());
  }

  node_["forwarder_chain"]          = import.forwarder_chain();
  node_["timedatestamp"]            = import.timedatestamp();
  node_["import_address_table_rva"] = import.import_address_table_rva();
  node_["import_lookup_table_rva"]  = import.import_lookup_table_rva();
  node_["name"]                     = import.name();
  node_["entries"]                  = entries;

}

void JsonVisitor::visit(const ImportEntry& import_entry) {

  if (import_entry.is_ordinal()) {
    node_["ordinal"] = import_entry.ordinal();
  } else {
    node_["name"] = import_entry.name();
  }

  node_["iat_address"] = import_entry.iat_address();
  node_["data"]        = import_entry.data();
  node_["hint"]        = import_entry.hint();
}

void JsonVisitor::visit(const ResourceData& resource_data) {

  node_["code_page"] = resource_data.code_page();
  node_["reserved"]  = resource_data.reserved();
  node_["offset"]    = resource_data.offset();
  node_["hash"]      = Hash::hash(resource_data.content());

}

void JsonVisitor::visit(const ResourceNode& resource_node) {

  node_["id"] = resource_node.id();

  if (resource_node.has_name()) {
    node_["name"] = u16tou8(resource_node.name());
  }

  if (!resource_node.childs().empty()) {
    std::vector<json> childs;
    for (const ResourceNode& rsrc : resource_node.childs()) {
      JsonVisitor visitor;
      rsrc.accept(visitor);
      childs.emplace_back(visitor.get());
    }

    node_["childs"] = childs;
  }

}

void JsonVisitor::visit(const ResourceDirectory& resource_directory) {

  node_["id"] = resource_directory.id();

  if (resource_directory.has_name()) {
    node_["name"] = u16tou8(resource_directory.name());
  }

  node_["characteristics"]       = resource_directory.characteristics();
  node_["time_date_stamp"]       = resource_directory.time_date_stamp();
  node_["major_version"]         = resource_directory.major_version();
  node_["minor_version"]         = resource_directory.minor_version();
  node_["numberof_name_entries"] = resource_directory.numberof_name_entries();
  node_["numberof_id_entries"]   = resource_directory.numberof_id_entries();

  if (!resource_directory.childs().empty()) {
    std::vector<json> childs;
    for (const ResourceNode& rsrc : resource_directory.childs()) {
      JsonVisitor visitor;
      rsrc.accept(visitor);
      childs.emplace_back(visitor.get());
    }

    node_["childs"] = childs;
  }
}


void JsonVisitor::visit(const ResourcesManager& resources_manager) {

  if (resources_manager.has_manifest()) {
    node_["manifest"] = escape_non_ascii(resources_manager.manifest()) ;
  }

  if (resources_manager.has_html()) {
    std::vector<std::string> escaped_strs;
    for (const std::string& elem : resources_manager.html()) {
      escaped_strs.emplace_back(escape_non_ascii(elem));
    }
    node_["html"] = escaped_strs;
  }


  std::vector<ResourceVersion> versions = resources_manager.version();
  if (!versions.empty()) {
    std::vector<json> j_versions;
    j_versions.reserve(versions.size());

    for (const ResourceVersion& version : versions) {
      JsonVisitor version_visitor;
      version_visitor(version);
      j_versions.push_back(version_visitor.get());
    }
    node_["version"] = std::move(j_versions);
  }

  if (resources_manager.has_icons()) {
    std::vector<json> icons;
    for (const ResourceIcon& icon : resources_manager.icons()) {
      JsonVisitor icon_visitor;
      icon_visitor(icon);
      icons.emplace_back(icon_visitor.get());
    }
    node_["icons"] = icons;
  }

  if (resources_manager.has_dialogs()) {
    std::vector<json> dialogs;
    for (const ResourceDialog& dialog : resources_manager.dialogs()) {
      JsonVisitor dialog_visitor;
      dialog_visitor(dialog);
      dialogs.emplace_back(dialog_visitor.get());
    }
    node_["dialogs"] = dialogs;
  }

  if (resources_manager.has_string_table()) {
    std::vector<json> string_table_json;
    for (const ResourcesManager::string_entry_t& entry : resources_manager.string_table()) {
      string_table_json.push_back(std::make_pair(entry.string_u8(), entry.id));
    }
    node_["string_table"] = string_table_json;
  }

  if (resources_manager.has_accelerator()) {
    std::vector<json> accelerator_json;
    for (const ResourceAccelerator& acc : resources_manager.accelerator()) {
      JsonVisitor accelerator_visitor;
      accelerator_visitor(acc);
      accelerator_json.emplace_back(accelerator_visitor.get());
    }
    node_["accelerator"] = accelerator_json;
  }
}

void JsonVisitor::visit(const ResourceStringFileInfo& info) {
  std::vector<json> items;
  for (const ResourceStringTable& item : info.children()) {
    JsonVisitor V;
    V(item);
    items.push_back(V.get());
  }

  node_["type"] = info.type();
  node_["key"] = info.key_u8();
  node_["items"] = std::move(items);
}

void JsonVisitor::visit(const ResourceVarFileInfo& info) {
  node_["type"] = info.type();
  node_["key"]  = info.key_u8();
  std::vector<json> j_vars;
  for (const ResourceVar& var : info.vars()) {
    j_vars.push_back(std::unordered_map<std::string, json> {
      std::make_pair("key", json(var.key_u8())),
      std::make_pair("type", json(var.type())),
      std::make_pair("values", json(var.values())),
    });
  }
  node_["values"] = std::move(j_vars);
}

void JsonVisitor::visit(const ResourceVersion& version) {
  node_["type"] = version.type();
  node_["key"] = version.key_u8();
  {
    const ResourceVersion::fixed_file_info_t& info = version.file_info();
    node_["file_info"] = std::unordered_map<std::string, json> {
      std::make_pair("signature", info.signature),
      std::make_pair("struct_version", info.struct_version),
      std::make_pair("file_version_ms", info.file_version_ms),
      std::make_pair("file_version_ls", info.file_version_ls),
      std::make_pair("product_version_ms", info.product_version_ms),
      std::make_pair("product_version_ls", info.product_version_ls),
      std::make_pair("file_flags_mask", info.file_flags_mask),
      std::make_pair("file_flags", info.file_flags),
      std::make_pair("file_os", info.file_os),
      std::make_pair("file_type", info.file_type),
      std::make_pair("file_subtype", info.file_subtype),
      std::make_pair("file_date_ms", info.file_date_ms),
      std::make_pair("file_date_ls", info.file_date_ls),
    };
  }

  if (const ResourceVarFileInfo* info = version.var_file_info()) {
    JsonVisitor visitor;
    visitor(*info);
    node_["var_file_info"] = visitor.get();
  }

  if (const ResourceStringFileInfo* info = version.string_file_info()) {
    JsonVisitor visitor;
    visitor(*info);
    node_["string_file_info"] = visitor.get();
  }
}

void JsonVisitor::visit(const ResourceIcon& resource_icon) {
  node_["id"]          = resource_icon.id();
  node_["lang"]        = resource_icon.lang();
  node_["sublang"]     = resource_icon.sublang();
  node_["width"]       = resource_icon.width();
  node_["height"]      = resource_icon.height();
  node_["color_count"] = resource_icon.color_count();
  node_["reserved"]    = resource_icon.reserved();
  node_["planes"]      = resource_icon.planes();
  node_["bit_count"]   = resource_icon.bit_count();
}

void JsonVisitor::visit(const ResourceStringTable& table) {
  std::vector<json> values;
  node_["key"] = table.key_u8();
  node_["type"] = table.type();

  for (const ResourceStringTable::entry_t& entry : table.entries()) {
    values.push_back(std::make_pair(entry.key_u8(), entry.value_u8()));
  }

  node_["values"] = std::move(values);
}

void JsonVisitor::visit(const ResourceAccelerator& acc) {

  std::vector<json> flags;
  for (const ResourceAccelerator::FLAGS c : acc.flags_list()) {
    flags.emplace_back(to_string(c));
  }
  node_["flags"]   = flags;
  node_["ansi"]    = acc.ansi_str();
  node_["id"]      = acc.id();
  node_["padding"] = acc.padding();
}

void JsonVisitor::visit(const Signature& signature) {

  JsonVisitor content_info_visitor;
  content_info_visitor(signature.content_info());

  std::vector<json> jsigners;
  for (const SignerInfo& signer : signature.signers()) {
    JsonVisitor visitor;
    visitor(signer);
    jsigners.emplace_back(visitor.get());
  }

  std::vector<json> crts;
  for (const x509& crt : signature.certificates()) {
    JsonVisitor crt_visitor;
    crt_visitor(crt);
    crts.emplace_back(crt_visitor.get());
  }

  node_["digest_algorithm"] = to_string(signature.digest_algorithm());
  node_["version"]          = signature.version();
  node_["content_info"]     = content_info_visitor.get();
  node_["signer_info"]      = jsigners;
  node_["certificates"]     = crts;
}

void JsonVisitor::visit(const x509& x509) {

  node_["serial_number"]       = x509.serial_number();
  node_["version"]             = x509.version();
  node_["issuer"]              = x509.issuer();
  node_["subject"]             = x509.subject();
  node_["signature_algorithm"] = x509.signature_algorithm();
  node_["valid_from"]          = x509.valid_from();
  node_["valid_to"]            = x509.valid_to();
}

void JsonVisitor::visit(const SignerInfo& signerinfo) {

  std::vector<json> auth_attrs;
  for (const Attribute& attr : signerinfo.authenticated_attributes()) {
    JsonVisitor visitor;
    visitor(attr);
    auth_attrs.emplace_back(visitor.get());
  }

  std::vector<json> unauth_attrs;
  for (const Attribute& attr : signerinfo.unauthenticated_attributes()) {
    JsonVisitor visitor;
    visitor(attr);
    auth_attrs.emplace_back(visitor.get());
  }

  node_["version"]                    = signerinfo.version();
  node_["digest_algorithm"]           = to_string(signerinfo.digest_algorithm());
  node_["encryption_algorithm"]       = to_string(signerinfo.encryption_algorithm());
  node_["encrypted_digest"]           = signerinfo.encrypted_digest();
  node_["issuer"]                     = signerinfo.issuer();
  node_["serial_number"]              = signerinfo.serial_number();
  node_["authenticated_attributes"]   = auth_attrs;
  node_["unauthenticated_attributes"] = unauth_attrs;
}

void JsonVisitor::visit(const ContentInfo& contentinfo) {
  node_["content_type"] = contentinfo.content_type();
  contentinfo.value().accept(*this);
}

void JsonVisitor::visit(const GenericContent& content) {
  node_["oid"] = content.oid();
}

void JsonVisitor::visit(const SpcIndirectData& content) {
  node_["file"]             = content.file();
  node_["digest"]           = content.digest();
  node_["digest_algorithm"] = to_string(content.digest_algorithm());
  node_["content_type"]     = content.content_type();
}

void JsonVisitor::visit(const Attribute& auth) {

  node_["type"] = to_string(auth.type());
}

void JsonVisitor::visit(const ContentType& attr) {

  visit(*attr.as<Attribute>());
  node_["oid"] = attr.oid();
}

void JsonVisitor::visit(const GenericType& attr) {

  visit(*attr.as<Attribute>());
  node_["oid"] = attr.oid();
}

void JsonVisitor::visit(const MsSpcNestedSignature& attr) {

  visit(*attr.as<Attribute>());
  JsonVisitor visitor;
  visitor(attr.sig());
  node_["signature"] = visitor.get();
}

void JsonVisitor::visit(const MsSpcStatementType& attr) {

  visit(*attr.as<Attribute>());
  node_["oid"] = attr.oid();
}

void JsonVisitor::visit(const MsCounterSign& attr) {
  visit(*attr.as<Attribute>());
}

void JsonVisitor::visit(const MsManifestBinaryID& attr) {
  visit(*attr.as<Attribute>());
  node_["manifest_id"] = attr.manifest_id();
}

void JsonVisitor::visit(const PKCS9AtSequenceNumber& attr) {

  visit(*attr.as<Attribute>());
  node_["number"] = attr.number();
}
void JsonVisitor::visit(const PKCS9CounterSignature& attr) {

  visit(*attr.as<Attribute>());

  JsonVisitor visitor;
  visitor(attr.signer());
  node_["signer"] = visitor.get();
}

void JsonVisitor::visit(const PKCS9MessageDigest& attr) {

  visit(*attr.as<Attribute>());
  node_["digest"] = attr.digest();
}

void JsonVisitor::visit(const PKCS9SigningTime& attr) {

  visit(*attr.as<Attribute>());
  node_["time"] = attr.time();
}

void JsonVisitor::visit(const SpcSpOpusInfo& attr) {

  visit(*attr.as<Attribute>());
  node_["more_info"]    = attr.more_info();
  node_["program_name"] = attr.program_name();
}

void JsonVisitor::visit(const SpcRelaxedPeMarkerCheck& attr) {

  visit(*attr.as<Attribute>());
  node_["value"] = attr.value();
}

void JsonVisitor::visit(const SigningCertificateV2& attr) {

  visit(*attr.as<Attribute>());
  // TODO
}

void JsonVisitor::visit(const CodeIntegrity& code_integrity) {

  node_["flags"]          = code_integrity.flags();
  node_["catalog"]        = code_integrity.catalog();
  node_["catalog_offset"] = code_integrity.catalog_offset();
  node_["reserved"]       = code_integrity.reserved();
}

void JsonVisitor::visit(const LoadConfiguration& config) {
  node_["characteristics"]                  = config.characteristics();
  node_["timedatestamp"]                    = config.timedatestamp();
  node_["major_version"]                    = config.major_version();
  node_["minor_version"]                    = config.minor_version();
  node_["global_flags_clear"]               = config.global_flags_clear();
  node_["global_flags_set"]                 = config.global_flags_set();
  node_["critical_section_default_timeout"] = config.critical_section_default_timeout();
  node_["decommit_free_block_threshold"]    = config.decommit_free_block_threshold();
  node_["decommit_total_free_threshold"]    = config.decommit_total_free_threshold();
  node_["lock_prefix_table"]                = config.lock_prefix_table();
  node_["maximum_allocation_size"]          = config.maximum_allocation_size();
  node_["virtual_memory_threshold"]         = config.virtual_memory_threshold();
  node_["process_affinity_mask"]            = config.process_affinity_mask();
  node_["process_heap_flags"]               = config.process_heap_flags();
  node_["csd_version"]                      = config.csd_version();
  node_["reserved1"]                        = config.reserved1();
  node_["editlist"]                         = config.editlist();
  node_["security_cookie"]                  = config.security_cookie();
}

void JsonVisitor::visit(const Pogo& pogo) {
  visit(static_cast<const Debug&>(pogo));
  node_["signature"] = to_string(pogo.signature());

  std::vector<json> entries;
  for (const PogoEntry& entry : pogo.entries()) {
    JsonVisitor v;
    v(entry);
    entries.emplace_back(v.get());
  }
  node_["entries"] = entries;
}

void JsonVisitor::visit(const PogoEntry& entry) {
  node_["name"]      = entry.name();
  node_["start_rva"] = entry.start_rva();
  node_["size"]      = entry.size();
}

void JsonVisitor::visit(const Repro& repro) {
  visit(static_cast<const Debug&>(repro));
  node_["hash"] = repro.hash();
}

void JsonVisitor::visit(const ResourceDialogRegular& dialog) {
  node_["style"] = dialog.style();
  node_["extended_style"] = dialog.extended_style();
  node_["x"] = dialog.x();
  node_["y"] = dialog.y();
  node_["cx"] = dialog.cx();
  node_["cy"] = dialog.cy();

  if (const ResourceDialogRegular::font_t& font  = dialog.font()) {
    node_["font"]["point_size"] = font.point_size;
    node_["font"]["name"] = u16tou8(font.name);
  }

  if (const ResourceDialog::ordinal_or_str_t& menu  = dialog.menu()) {
    node_["menu"] = menu.to_string();
  }
  std::vector<json> items;
  items.reserve(dialog.nb_items());
  for (const ResourceDialogRegular::Item& item : dialog.items()) {
    json j_item;
    j_item["id"] = item.id();
    j_item["style"] = item.style();
    j_item["extended_style"] = item.extended_style();
    j_item["x"] = item.x();
    j_item["y"] = item.y();
    j_item["cx"] = item.cx();
    j_item["cy"] = item.cy();
  }
}

void JsonVisitor::visit(const ResourceDialogExtended& dialog) {
  node_["style"] = dialog.style();
  node_["extended_style"] = dialog.extended_style();
  node_["x"] = dialog.x();
  node_["y"] = dialog.y();
  node_["cx"] = dialog.cx();
  node_["cy"] = dialog.cy();

  if (const ResourceDialogExtended::font_t& font  = dialog.font()) {
    node_["font"]["point_size"] = font.point_size;
    node_["font"]["name"] = u16tou8(font.typeface);
  }

  if (const ResourceDialog::ordinal_or_str_t& menu  = dialog.menu()) {
    node_["menu"] = menu.to_string();
  }
  std::vector<json> items;
  items.reserve(dialog.items().size());
  for (const ResourceDialogExtended::Item& item : dialog.items()) {
    json j_item;
    j_item["id"] = item.id();
    j_item["style"] = item.style();
    j_item["extended_style"] = item.extended_style();
    j_item["x"] = item.x();
    j_item["y"] = item.y();
    j_item["cx"] = item.cx();
    j_item["cy"] = item.cy();
  }
}

// LIEF Abstract
void JsonVisitor::visit(const LIEF::Binary& binary) {
  visit(static_cast<const LIEF::PE::Binary&>(binary));
}

void JsonVisitor::visit(const LIEF::Section& section) {
  visit(static_cast<const LIEF::PE::Section&>(section));
}

} // namespace PE
} // namespace LIEF

