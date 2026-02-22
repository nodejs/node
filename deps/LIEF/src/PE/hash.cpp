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

#include "LIEF/PE/hash.hpp"
#include "LIEF/PE.hpp"
#include "Object.tcc"

namespace LIEF {
namespace PE {

Hash::~Hash() = default;

size_t Hash::hash(const Object& obj) {
  return LIEF::Hash::hash<LIEF::PE::Hash>(obj);
}

void Hash::visit(const Binary& binary) {
  process(binary.dos_header());
  process(binary.header());
  process(binary.optional_header());

  process(std::begin(binary.data_directories()), std::end(binary.data_directories()));
  process(std::begin(binary.sections()), std::end(binary.sections()));
  process(std::begin(binary.imports()), std::end(binary.imports()));
  process(std::begin(binary.delay_imports()), std::end(binary.delay_imports()));
  process(std::begin(binary.relocations()), std::end(binary.relocations()));

  if (binary.has_debug()) {
    process(std::begin(binary.debug()), std::end(binary.debug()));
  }

  if (const Export* exp = binary.get_export()) {
    process(*exp);
  }

  if (const TLS* tls_obj = binary.tls()) {
    process(*tls_obj);
  }

  if (const RichHeader* rheader = binary.rich_header()) {
    process(*rheader);
  }

}


void Hash::visit(const DosHeader& dos_header) {
  process(dos_header.magic());
  process(dos_header.used_bytes_in_last_page());
  process(dos_header.file_size_in_pages());
  process(dos_header.numberof_relocation());
  process(dos_header.header_size_in_paragraphs());
  process(dos_header.minimum_extra_paragraphs());
  process(dos_header.maximum_extra_paragraphs());
  process(dos_header.initial_relative_ss());
  process(dos_header.initial_sp());
  process(dos_header.checksum());
  process(dos_header.initial_ip());
  process(dos_header.initial_relative_cs());
  process(dos_header.addressof_relocation_table());
  process(dos_header.overlay_number());
  process(dos_header.reserved());
  process(dos_header.oem_id());
  process(dos_header.oem_info());
  process(dos_header.reserved2());
  process(dos_header.addressof_new_exeheader());
}

void Hash::visit(const RichHeader& rich_header) {
  RichHeader::it_const_entries entries = rich_header.entries();
  process(rich_header.key());
  process(entries.begin(), entries.end());
}

void Hash::visit(const RichEntry& rich_entry) {
  process(rich_entry.id());
  process(rich_entry.build_id());
  process(rich_entry.count());
}

void Hash::visit(const Header& header) {
  process(header.signature());
  process(header.machine());
  process(header.numberof_sections());
  process(header.time_date_stamp());
  process(header.pointerto_symbol_table());
  process(header.numberof_symbols());
  process(header.sizeof_optional_header());
  process(header.characteristics());
}

void Hash::visit(const OptionalHeader& optional_header) {
  process(static_cast<uint8_t>(optional_header.magic()));
  process(optional_header.major_linker_version());
  process(optional_header.minor_linker_version());
  process(optional_header.sizeof_code());
  process(optional_header.sizeof_initialized_data());
  process(optional_header.sizeof_uninitialized_data());
  process(optional_header.addressof_entrypoint());
  process(optional_header.baseof_code());
  if (optional_header.magic() == PE_TYPE::PE32) {
    process(optional_header.baseof_data());
  }
  process(optional_header.imagebase());
  process(optional_header.section_alignment());
  process(optional_header.file_alignment());
  process(optional_header.major_operating_system_version());
  process(optional_header.minor_operating_system_version());
  process(optional_header.major_image_version());
  process(optional_header.minor_image_version());
  process(optional_header.major_subsystem_version());
  process(optional_header.minor_subsystem_version());
  process(optional_header.win32_version_value());
  process(optional_header.sizeof_image());
  process(optional_header.sizeof_headers());
  process(optional_header.checksum());
  process(optional_header.subsystem());
  process(optional_header.dll_characteristics());
  process(optional_header.sizeof_stack_reserve());
  process(optional_header.sizeof_stack_commit());
  process(optional_header.sizeof_heap_reserve());
  process(optional_header.sizeof_heap_commit());
  process(optional_header.loader_flags());
  process(optional_header.numberof_rva_and_size());

}

void Hash::visit(const DataDirectory& data_directory) {
  process(data_directory.RVA());
  process(data_directory.size());
  process(data_directory.type());
}

void Hash::visit(const Section& section) {
  process(section.name());
  process(section.offset());
  process(section.size());

  process(section.virtual_size());
  process(section.virtual_address());
  process(section.pointerto_raw_data());
  process(section.pointerto_relocation());
  process(section.pointerto_line_numbers());
  process(section.numberof_relocations());
  process(section.numberof_line_numbers());
  process(section.characteristics());
  process(section.content());

}

void Hash::visit(const Relocation& relocation) {
  process(relocation.virtual_address());
  process(std::begin(relocation.entries()), std::end(relocation.entries()));
}

void Hash::visit(const RelocationEntry& relocation_entry) {
  process(relocation_entry.data());
  process(relocation_entry.position());
  process(relocation_entry.type());

}

void Hash::visit(const Export& export_) {
  process(export_.export_flags());
  process(export_.timestamp());
  process(export_.major_version());
  process(export_.minor_version());
  process(export_.ordinal_base());
  process(export_.name());
  process(std::begin(export_.entries()), std::end(export_.entries()));
}

void Hash::visit(const ExportEntry& export_entry) {
  process(export_entry.name());
  process(export_entry.ordinal());
  process(export_entry.address());
  process(static_cast<size_t>(export_entry.is_extern()));
}

void Hash::visit(const TLS& tls) {
  process(tls.addressof_raw_data().first);
  process(tls.addressof_raw_data().second);
  process(tls.addressof_index());
  process(tls.addressof_callbacks());
  process(tls.sizeof_zero_fill());
  process(tls.characteristics());
  process(tls.data_template());
  process(tls.callbacks());
}

void Hash::visit(const Debug& debug) {
  process(debug.characteristics());
  process(debug.timestamp());
  process(debug.major_version());
  process(debug.minor_version());
  process(debug.type());
  process(debug.sizeof_data());
  process(debug.addressof_rawdata());
  process(debug.pointerto_rawdata());
}

void Hash::visit(const CodeView& cv) {
  visit(static_cast<const Debug&>(cv));
  process(cv.signature());
}

void Hash::visit(const CodeViewPDB& cvpdb) {
  visit(*cvpdb.as<CodeView>());
  process(cvpdb.signature());
  process(cvpdb.age());
  process(cvpdb.filename());
}

void Hash::visit(const Import& import) {
  process(import.forwarder_chain());
  process(import.timedatestamp());
  process(import.import_address_table_rva());
  process(import.import_lookup_table_rva());
  process(import.name());
  process(std::begin(import.entries()), std::end(import.entries()));
}

void Hash::visit(const ImportEntry& import_entry) {
  process(import_entry.hint_name_rva());
  process(import_entry.hint());
  process(import_entry.iat_value());
  process(import_entry.name());
  process(import_entry.data());
}


void Hash::visit(const DelayImport& import) {
  DelayImport::it_const_entries entries = import.entries();

  process(import.attribute());
  process(import.name());
  process(import.handle());
  process(import.names_table());
  process(import.iat());
  process(import.biat());
  process(import.uiat());
  process(import.timestamp());
  process(entries.begin(), entries.end());
}

void Hash::visit(const DelayImportEntry& import_entry) {
  process(import_entry.name());
  process(import_entry.data());
  process(import_entry.iat_value());
}

void Hash::visit(const ResourceNode& resource_node) {
  process(resource_node.id());
  if (resource_node.has_name()) {
    process(resource_node.name());
  }

  process(std::begin(resource_node.childs()), std::end(resource_node.childs()));
}

void Hash::visit(const ResourceData& resource_data) {
  visit(*resource_data.as<ResourceNode>());
  process(resource_data.code_page());
  process(resource_data.content());
}

void Hash::visit(const ResourceDirectory& resource_directory) {
  visit(*resource_directory.as<ResourceNode>());
  process(resource_directory.characteristics());
  process(resource_directory.time_date_stamp());
  process(resource_directory.major_version());
  process(resource_directory.minor_version());
  process(resource_directory.numberof_name_entries());
  process(resource_directory.numberof_id_entries());
}


void Hash::visit(const ResourcesManager& resources_manager) {
  if (resources_manager.has_manifest()) {
    process(resources_manager.manifest());
  }
  for (const ResourceVersion& version : resources_manager.version()) {
    process(version);
  }

  if (resources_manager.has_icons()) {
    process(std::begin(resources_manager.icons()), std::end(resources_manager.icons()));
  }

  if (resources_manager.has_dialogs()) {
    process(std::begin(resources_manager.dialogs()), std::end(resources_manager.dialogs()));
  }
}

void Hash::visit(const ResourceStringFileInfo& info) {
  process(info.type());
  process(info.key());
  for (const ResourceStringTable& table : info.children()) {
    for (const ResourceStringTable::entry_t& entry : table.entries()) {
      process(entry.key);
      process(entry.value);
    }
  }
}

void Hash::visit(const ResourceVarFileInfo& info) {
  process(info.type());
  process(info.key());
  for (const ResourceVar& var : info.vars()) {
    for (uint32_t value : var.values()) {
      process(value);
    }
  }
}

void Hash::visit(const ResourceVersion& version) {
  process(version.type());
  process(version.key());

  {
    const ResourceVersion::fixed_file_info_t& info = version.file_info();
    process(info.signature);
    process(info.struct_version);
    process(info.file_version_ms);
    process(info.file_version_ls);
    process(info.product_version_ms);
    process(info.product_version_ls);
    process(info.file_flags_mask);
    process(info.file_flags);
    process(info.file_os);
    process(info.file_type);
    process(info.file_subtype);
    process(info.file_date_ms);
    process(info.file_date_ls);
  }

  if (const ResourceStringFileInfo* info = version.string_file_info()) {
    process(*info);
  }

  if (const ResourceVarFileInfo* info = version.var_file_info()) {
    process(*info);
  }
}

void Hash::visit(const ResourceIcon& resource_icon) {
  if (resource_icon.id() != static_cast<uint32_t>(-1)) {
    process(resource_icon.id());
  }
  process(resource_icon.lang());
  process(resource_icon.sublang());
  process(resource_icon.width());
  process(resource_icon.height());
  process(resource_icon.color_count());
  process(resource_icon.reserved());
  process(resource_icon.planes());
  process(resource_icon.bit_count());
  process(resource_icon.pixels());

}

void Hash::visit(const ResourceStringTable& table) {
  for (const ResourceStringTable::entry_t& entry : table.entries()) {
    process(entry.key);
    process(entry.value);
  }
}

void Hash::visit(const ResourceAccelerator& accelerator) {
  process(accelerator.flags());
  process(accelerator.ansi());
  process(accelerator.id());
  process(accelerator.padding());
}

void Hash::visit(const Signature& signature) {
  process(signature.version());
  process(signature.digest_algorithm());
  process(signature.content_info());
  process(std::begin(signature.certificates()), std::end(signature.certificates()));
  process(std::begin(signature.signers()), std::end(signature.signers()));
}

void Hash::visit(const x509& x509) {
  process(x509.subject());
  process(x509.issuer());
  process(x509.valid_to());
  process(x509.valid_from());
  process(x509.signature_algorithm());
  process(x509.serial_number());
  process(x509.version());
}

void Hash::visit(const SignerInfo& signerinfo) {
  process(signerinfo.version());
  process(signerinfo.serial_number());
  process(signerinfo.issuer());
  process(signerinfo.encryption_algorithm());
  process(signerinfo.digest_algorithm());
  process(signerinfo.encrypted_digest());
  process(std::begin(signerinfo.authenticated_attributes()), std::end(signerinfo.authenticated_attributes()));
  process(std::begin(signerinfo.unauthenticated_attributes()), std::end(signerinfo.unauthenticated_attributes()));
}

void Hash::visit(const Attribute& attr) {
  process(attr.type());
}

void Hash::visit(const ContentInfo& info) {
  process(info.content_type());
  info.value().accept(*this);
}


void Hash::visit(const GenericContent& content) {
  process(content.raw());
  process(content.oid());
}

void Hash::visit(const SpcIndirectData& content) {
  process(content.file());
  process(content.digest());
  process(content.digest_algorithm());

}


void Hash::visit(const ContentType& attr) {
  visit(*attr.as<Attribute>());
  process(attr.oid());
}

void Hash::visit(const GenericType& attr) {
  visit(*attr.as<Attribute>());
  process(attr.raw_content());
  process(attr.oid());
}

void Hash::visit(const MsSpcNestedSignature& attr) {
  visit(*attr.as<Attribute>());
  process(attr.sig());
}

void Hash::visit(const MsSpcStatementType& attr) {
  visit(*attr.as<Attribute>());
  process(attr.oid());
}

void Hash::visit(const MsCounterSign& attr) {
  visit(*attr.as<Attribute>());
  // TODO
}

void Hash::visit(const MsManifestBinaryID& attr) {
  visit(*attr.as<Attribute>());
  process(attr.manifest_id());
}

void Hash::visit(const PKCS9AtSequenceNumber& attr) {
  visit(*attr.as<Attribute>());
  process(attr.number());
}

void Hash::visit(const PKCS9CounterSignature& attr) {
  visit(*attr.as<Attribute>());
  process(attr.signer());
}

void Hash::visit(const PKCS9MessageDigest& attr) {
  visit(*attr.as<Attribute>());
  process(attr.digest());
}

void Hash::visit(const PKCS9SigningTime& attr) {
  visit(*attr.as<Attribute>());
  process(attr.time());
}

void Hash::visit(const SpcSpOpusInfo& attr) {
  visit(*attr.as<Attribute>());
  process(attr.program_name());
  process(attr.more_info());
}

void Hash::visit(const SpcRelaxedPeMarkerCheck& attr) {
  visit(*attr.as<Attribute>());
  process(attr.value());
}

void Hash::visit(const SigningCertificateV2& attr) {
  visit(*attr.as<Attribute>());
  //TODO
}

void Hash::visit(const CodeIntegrity& code_integrity) {
  process(code_integrity.flags());
  process(code_integrity.catalog());
  process(code_integrity.catalog_offset());
  process(code_integrity.reserved());
}

void Hash::visit(const LoadConfiguration& config) {
  process(config.characteristics());
  process(config.timedatestamp());
  process(config.major_version());
  process(config.minor_version());
  process(config.global_flags_clear());
  process(config.global_flags_set());
  process(config.critical_section_default_timeout());
  process(config.decommit_free_block_threshold());
  process(config.decommit_total_free_threshold());
  process(config.lock_prefix_table());
  process(config.maximum_allocation_size());
  process(config.virtual_memory_threshold());
  process(config.process_affinity_mask());
  process(config.process_heap_flags());
  process(config.csd_version());
  process(config.reserved1());
  process(config.editlist());
  process(config.security_cookie());
  process(config.se_handler_table());
  process(config.se_handler_count());
  process(config.guard_cf_check_function_pointer());
  process(config.guard_cf_dispatch_function_pointer());
  process(config.guard_cf_function_table());
  process(config.guard_cf_function_count());
  process(config.guard_flags());
  if (const CodeIntegrity* CI = config.code_integrity()) {
    process(*CI);
  }
  process(config.guard_address_taken_iat_entry_table());
  process(config.guard_address_taken_iat_entry_count());
  process(config.guard_long_jump_target_table());
  process(config.guard_long_jump_target_count());
  process(config.dynamic_value_reloc_table());
  process(config.hybrid_metadata_pointer());
  process(config.guard_rf_failure_routine());
  process(config.guard_rf_failure_routine_function_pointer());
  process(config.dynamic_value_reloctable_offset());
  process(config.dynamic_value_reloctable_section());
  process(config.guard_rf_verify_stackpointer_function_pointer());
  process(config.hotpatch_table_offset());
  process(config.reserved3());
  process(config.enclave_configuration_ptr());
  process(config.volatile_metadata_pointer());
  process(config.guard_eh_continuation_table());
  process(config.guard_eh_continuation_count());
  process(config.guard_xfg_check_function_pointer());
  process(config.guard_xfg_dispatch_function_pointer());
  process(config.guard_xfg_table_dispatch_function_pointer());
  process(config.cast_guard_os_determined_failure_mode());
  process(config.guard_memcpy_function_pointer());
}

void Hash::visit(const Pogo& pogo) {
  Pogo::it_const_entries entries = pogo.entries();
  visit(static_cast<const Debug&>(pogo));
  process(pogo.signature());
  process(std::begin(entries), std::end(entries));
}


void Hash::visit(const PogoEntry& entry) {
  process(entry.name());
  process(entry.start_rva());
  process(entry.size());
}

void Hash::visit(const Repro& repro) {
  visit(static_cast<const Debug&>(repro));
  process(repro.hash());
}

void Hash::visit(const ResourceDialogRegular& dialog) {
  process(dialog.style());
  process(dialog.extended_style());
  process(dialog.x());
  process(dialog.y());
  process(dialog.cx());
  process(dialog.cy());
  process(dialog.font().point_size);
  process(dialog.font().name);
  process(dialog.menu().ordinal.value_or(0));
  process(dialog.menu().string);

  for (const ResourceDialogRegular::Item& item : dialog.items()) {
    process(item.id());
    process(item.style());
    process(item.extended_style());
    process(item.x());
    process(item.y());
    process(item.cx());
    process(item.cy());
    process(item.clazz().ordinal.value_or(0));
    process(item.clazz().string);
    process(item.title().ordinal.value_or(0));
    process(item.title().string);
    process(item.creation_data());
  }
}

void Hash::visit(const ResourceDialogExtended& dialog) {
  process(dialog.help_id());
  process(dialog.style());
  process(dialog.extended_style());
  process(dialog.x());
  process(dialog.y());
  process(dialog.cx());
  process(dialog.cy());
  process(dialog.font().point_size);
  process(dialog.font().typeface);
  process(dialog.font().italic);
  process(dialog.font().charset);
  process(dialog.font().weight);
  process(dialog.menu().ordinal.value_or(0));
  process(dialog.menu().string);


  for (const ResourceDialogExtended::Item& item : dialog.items()) {
    process(item.help_id());
    process(item.id());
    process(item.style());
    process(item.extended_style());
    process(item.x());
    process(item.y());
    process(item.cx());
    process(item.cy());
    process(item.clazz().ordinal.value_or(0));
    process(item.clazz().string);
    process(item.title().ordinal.value_or(0));
    process(item.title().string);
    process(item.creation_data());
  }

}

} // namespace PE
} // namespace LIEF

