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

#include "LIEF/MachO/hash.hpp"
#include "LIEF/MachO.hpp"
#include "Object.tcc"

namespace LIEF {
namespace MachO {

Hash::~Hash() = default;

size_t Hash::hash(const Object& obj) {
  return LIEF::Hash::hash<LIEF::MachO::Hash>(obj);
}

void Hash::visit(const Binary& binary) {
  process(binary.header());
  process(std::begin(binary.commands()), std::end(binary.commands()));
  process(std::begin(binary.symbols()), std::end(binary.symbols()));
}


void Hash::visit(const Header& header) {
  process(header.magic());
  process(header.cpu_type());
  process(header.cpu_subtype());
  process(header.file_type());
  process(header.nb_cmds());
  process(header.sizeof_cmds());
  process(header.flags());
  process(header.reserved());
}

void Hash::visit(const LoadCommand& cmd) {
  process(cmd.command());
  process(cmd.size());
  process(cmd.data());
  process(cmd.command_offset());
}

void Hash::visit(const UUIDCommand& uuid) {
  visit(*uuid.as<LoadCommand>());
  process(uuid.uuid());
}

void Hash::visit(const SymbolCommand& symbol) {

  visit(*symbol.as<LoadCommand>());
  process(symbol.symbol_offset());
  process(symbol.numberof_symbols());
  process(symbol.strings_offset());
  process(symbol.strings_size());
}

void Hash::visit(const SegmentCommand& segment) {

  visit(*segment.as<LoadCommand>());
  process(segment.name());
  process(segment.virtual_address());
  process(segment.virtual_size());
  process(segment.file_size());
  process(segment.file_offset());
  process(segment.max_protection());
  process(segment.init_protection());
  process(segment.numberof_sections());
  process(segment.flags());
  process(segment.content());
  process(std::begin(segment.sections()), std::end(segment.sections()));
}

void Hash::visit(const Section& section) {
  process(section.content());
  process(section.segment_name());
  process(section.address());
  process(section.alignment());
  process(section.relocation_offset());
  process(section.numberof_relocations());
  process(section.flags());
  process(section.type());
  process(section.reserved1());
  process(section.reserved2());
  process(section.reserved3());
  process(section.raw_flags());
  process(std::begin(section.relocations()), std::end(section.relocations()));
}

void Hash::visit(const MainCommand& maincmd) {

  visit(*maincmd.as<LoadCommand>());

  process(maincmd.entrypoint());
  process(maincmd.stack_size());
}

void Hash::visit(const NoteCommand& note) {
  visit(*note.as<LoadCommand>());
}

void Hash::visit(const DynamicSymbolCommand& dynamic_symbol) {
  visit(*dynamic_symbol.as<LoadCommand>());
  process(dynamic_symbol.idx_local_symbol());
  process(dynamic_symbol.nb_local_symbols());

  process(dynamic_symbol.idx_external_define_symbol());
  process(dynamic_symbol.nb_external_define_symbols());

  process(dynamic_symbol.idx_undefined_symbol());
  process(dynamic_symbol.nb_undefined_symbols());

  process(dynamic_symbol.toc_offset());
  process(dynamic_symbol.nb_toc());

  process(dynamic_symbol.module_table_offset());
  process(dynamic_symbol.nb_module_table());

  process(dynamic_symbol.external_reference_symbol_offset());
  process(dynamic_symbol.nb_external_reference_symbols());

  process(dynamic_symbol.indirect_symbol_offset());
  process(dynamic_symbol.nb_indirect_symbols());

  process(dynamic_symbol.external_relocation_offset());
  process(dynamic_symbol.nb_external_relocations());

  process(dynamic_symbol.local_relocation_offset());
  process(dynamic_symbol.nb_local_relocations());
}

void Hash::visit(const DylinkerCommand& dylinker) {
  visit(*dylinker.as<LoadCommand>());
  process(dylinker.name());
}

void Hash::visit(const DylibCommand& dylib) {
  visit(*dylib.as<LoadCommand>());

  process(dylib.name());
  process(dylib.timestamp());
  process(dylib.current_version());
  process(dylib.compatibility_version());
}

void Hash::visit(const ThreadCommand& threadcmd) {
  visit(*threadcmd.as<LoadCommand>());
  process(threadcmd.flavor());
  process(threadcmd.count());
  process(threadcmd.state());
}

void Hash::visit(const RPathCommand& rpath) {
  visit(*rpath.as<LoadCommand>());
  process(rpath.path());
}

void Hash::visit(const Routine& routine) {
  visit(*routine.as<LoadCommand>());
}

void Hash::visit(const Symbol& symbol) {
  process(symbol.name());
  process(symbol.value());
  process(symbol.size());

  process(symbol.type());
  process(symbol.numberof_sections());
  process(symbol.description());

  //if (symbol.has_binding_info()) {
  //  process(symbol.binding_info());
  //}

  //if (symbol.has_export_info()) {
  //  process(symbol.export_info());
  //}
}

void Hash::visit(const Relocation& relocation) {

  process(relocation.size());
  process(relocation.address());
  process(static_cast<size_t>(relocation.is_pc_relative()));
  process(relocation.type());
  process(relocation.origin());

  if (relocation.has_symbol()) {
    process(relocation.symbol()->name());
  }
}

void Hash::visit(const RelocationObject& robject) {

  visit(*robject.as<Relocation>());
  process(static_cast<size_t>(robject.is_scattered()));
  if (robject.is_scattered()) {
    process(robject.value());
  }
}

void Hash::visit(const RelocationDyld& rdyld) {
  visit(*rdyld.as<Relocation>());
}

void Hash::visit(const RelocationFixup& fixup) {
  visit(*fixup.as<Relocation>());
  process(fixup.target());
}

void Hash::visit(const BindingInfo& binding) {

  process(binding.library_ordinal());
  process(binding.addend());
  process(static_cast<size_t>(binding.is_weak_import()));
  process(binding.address());

  if (binding.has_symbol()) {
    process(binding.symbol()->name());
  }

  if (binding.has_library()) {
    process(*binding.library());
  }
}

void Hash::visit(const DyldBindingInfo& binding) {
  visit(*binding.as<BindingInfo>());
  process(binding.binding_class());
  process(binding.binding_type());
}

void Hash::visit(const ChainedBindingInfo& binding) {
  visit(*binding.as<BindingInfo>());
  process(binding.format());
}

void Hash::visit(const DyldExportsTrie& trie) {
  visit(*trie.as<LoadCommand>());
  process(trie.data_offset());
  process(trie.data_size());
  process(trie.content());
}

void Hash::visit(const ExportInfo& einfo) {
  process(einfo.node_offset());
  process(einfo.flags());
  process(einfo.address());

  if (einfo.has_symbol()) {
    process(einfo.symbol()->name());
  }
}

void Hash::visit(const FunctionStarts& fs) {
  visit(*fs.as<LoadCommand>());
  process(fs.data_offset());
  process(fs.data_size());
  process(fs.functions());

}

void Hash::visit(const CodeSignature& cs) {
  visit(*cs.as<LoadCommand>());
  process(cs.data_offset());
  process(cs.data_size());
}

void Hash::visit(const DataInCode& dic) {
  visit(*dic.as<LoadCommand>());
  process(dic.data_offset());
  process(dic.data_size());
  process(std::begin(dic.entries()), std::end(dic.entries()));
}

void Hash::visit(const DataCodeEntry& dce) {
  process(dce.offset());
  process(dce.length());
  process(dce.type());
}

void Hash::visit(const VersionMin& vmin) {
  visit(*vmin.as<LoadCommand>());
  process(vmin.version());
  process(vmin.sdk());
}

void Hash::visit(const UnknownCommand& ukn) {
  visit(*ukn.as<LoadCommand>());
  process(ukn.original_command());
}

void Hash::visit(const SourceVersion& sv) {
  visit(*sv.as<LoadCommand>());
  process(sv.version());
}

void Hash::visit(const SegmentSplitInfo& ssi) {
  visit(*ssi.as<LoadCommand>());
  process(ssi.data_offset());
  process(ssi.data_size());
}

void Hash::visit(const SubFramework& sf) {
  visit(*sf.as<LoadCommand>());
  process(sf.umbrella());
}

void Hash::visit(const DyldEnvironment& de) {
  visit(*de.as<LoadCommand>());
  process(de.value());
}

void Hash::visit(const EncryptionInfo& e) {
  visit(*e.as<LoadCommand>());
  process(e.crypt_offset());
  process(e.crypt_size());
  process(e.crypt_id());
}

void Hash::visit(const BuildVersion& e) {
  BuildVersion::tools_list_t tools = e.tools();

  visit(*e.as<LoadCommand>());
  process(e.platform());
  process(e.minos());
  process(e.sdk());
  process(std::begin(tools), std::end(tools));
}

void Hash::visit(const BuildToolVersion& e) {
  process(e.tool());
  process(e.version());
}

void Hash::visit(const FilesetCommand& e) {
  process(e.name());
  process(e.virtual_address());
  process(e.file_offset());
}

void Hash::visit(const CodeSignatureDir& e) {
  process(e.data_offset());
  process(e.data_size());
  process(e.content());
}

void Hash::visit(const TwoLevelHints& e) {
  process(e.content());
}

void Hash::visit(const LinkerOptHint& e) {
  process(e.data_offset());
  process(e.data_size());
  process(e.content());
}





} // namespace MachO
} // namespace LIEF

