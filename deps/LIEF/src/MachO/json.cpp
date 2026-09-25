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


#include "MachO/json_internal.hpp"
#include "LIEF/MachO.hpp"
#include "Object.tcc"
#include "MachO/Binary.tcc"

namespace LIEF {
namespace MachO {


template<class T>
inline void process_command(json& node, const Binary& bin, const char* key) {
  auto* cmd = bin.command<T>();
  if (cmd != nullptr) {
    JsonVisitor v;
    v(*cmd);
    node[key] = v.get();
  }
}


void JsonVisitor::visit(const Binary& binary) {
  JsonVisitor header_visitor;
  header_visitor(binary.header());

  // Sections
  std::vector<json> sections;
  for (const Section& section : binary.sections()) {
    JsonVisitor visitor;
    visitor(section);
    sections.emplace_back(visitor.get());
  }

  // Segments
  std::vector<json> segments;
  for (const SegmentCommand& segment : binary.segments()) {
    JsonVisitor visitor;
    visitor(segment);
    segments.emplace_back(visitor.get());
  }

  // Symbols
  std::vector<json> symbols;
  for (const Symbol& sym : binary.symbols()) {
    JsonVisitor visitor;
    visitor(sym);
    symbols.emplace_back(visitor.get());
  }

  // Relocations
  std::vector<json> relocations;
  for (const Relocation& r : binary.relocations()) {
    JsonVisitor visitor;
    visitor(r);
    relocations.emplace_back(visitor.get());
  }

  std::vector<json> libraries;
  for (const DylibCommand& lib : binary.libraries()) {
    JsonVisitor visitor;
    visitor(lib);
    libraries.emplace_back(visitor.get());
  }


  node_["header"]      = header_visitor.get();
  node_["sections"]    = sections;
  node_["segments"]    = segments;
  node_["symbols"]     = symbols;
  node_["relocations"] = relocations;
  node_["libraries"]   = libraries;

  process_command<UUIDCommand>(node_, binary, "uuid");
  process_command<MainCommand>(node_, binary, "main_command");
  process_command<DylinkerCommand>(node_, binary, "dylinker");
  process_command<DyldInfo>(node_, binary, "dyld_info");
  process_command<FunctionStarts>(node_, binary, "function_starts");
  process_command<SourceVersion>(node_, binary, "source_version");
  process_command<VersionMin>(node_, binary, "version_min");
  process_command<ThreadCommand>(node_, binary, "thread_command");
  process_command<RPathCommand>(node_, binary, "rpath");
  process_command<Routine>(node_, binary, "routine");
  process_command<SymbolCommand>(node_, binary, "symbol_command");
  process_command<DynamicSymbolCommand>(node_, binary, "dynamic_symbol_command");
  process_command<CodeSignature>(node_, binary, "code_signature");
  process_command<DataInCode>(node_, binary, "data_in_code");
  process_command<EncryptionInfo>(node_, binary, "encryption_info");
  process_command<BuildVersion>(node_, binary, "build_verison");
}


void JsonVisitor::visit(const Header& header) {

  std::vector<json> flags;
  for (Header::FLAGS f : header.flags_list()) {
    flags.emplace_back(to_string(f));
  }
  node_["magic"]       = to_string(header.magic());
  node_["cpu_type"]    = to_string(header.cpu_type());
  node_["cpu_subtype"] = header.cpu_subtype();
  node_["file_type"]   = to_string(header.file_type());
  node_["nb_cmds"]     = header.nb_cmds();
  node_["sizeof_cmds"] = header.sizeof_cmds();
  node_["reserved"]    = header.reserved();
  node_["flags"]       = flags;
}


void JsonVisitor::visit(const LoadCommand& cmd) {
  node_["command"]        = to_string(cmd.command());
  node_["command_size"]   = cmd.size();
  node_["command_offset"] = cmd.command_offset();
  node_["data_hash"]      = LIEF::hash(cmd.data());
}

void JsonVisitor::visit(const UUIDCommand& uuid) {
  visit(*uuid.as<LoadCommand>());
  node_["uuid"] = uuid.uuid();
}

void JsonVisitor::visit(const SymbolCommand& symbol) {
  visit(*symbol.as<LoadCommand>());
  node_["symbol_offset"]    = symbol.symbol_offset();
  node_["numberof_symbols"] = symbol.numberof_symbols();
  node_["strings_offset"]   = symbol.strings_offset();
  node_["strings_size"]     = symbol.strings_size();
}

void JsonVisitor::visit(const SegmentCommand& segment) {

  std::vector<json> sections;
  for (const Section& section : segment.sections()) {
    sections.emplace_back(section.name());
  }

  visit(*segment.as<LoadCommand>());
  node_["virtual_address"]   = segment.virtual_address();
  node_["virtual_size"]      = segment.virtual_size();
  node_["file_size"]         = segment.file_size();
  node_["file_offset"]       = segment.file_offset();
  node_["max_protection"]    = segment.max_protection();
  node_["init_protection"]   = segment.init_protection();
  node_["numberof_sections"] = segment.numberof_sections();
  node_["flags"]             = segment.flags();
  node_["sections"]          = sections;
  node_["content_hash"]      = LIEF::hash(segment.content());
}

void JsonVisitor::visit(const Section& section) {

  std::vector<json> flags;
  for (Section::FLAGS f : section.flags_list()) {
    flags.emplace_back(to_string(f));
  }
  node_["name"]                 = section.name();
  node_["virtual_address"]      = section.virtual_address();
  node_["offset"]               = section.offset();
  node_["size"]                 = section.size();
  node_["alignment"]            = section.alignment();
  node_["relocation_offset"]    = section.relocation_offset();
  node_["numberof_relocations"] = section.numberof_relocations();
  node_["flags"]                = section.flags();
  node_["type"]                 = to_string(section.type());
  node_["reserved1"]            = section.reserved1();
  node_["reserved2"]            = section.reserved2();
  node_["reserved3"]            = section.reserved3();
  node_["content_hash"]         = LIEF::hash(section.content());
}

void JsonVisitor::visit(const MainCommand& maincmd) {
  visit(*maincmd.as<LoadCommand>());

  node_["entrypoint"] = maincmd.entrypoint();
  node_["stack_size"] = maincmd.stack_size();
}


void JsonVisitor::visit(const NoteCommand& note) {
  visit(*note.as<LoadCommand>());
}

void JsonVisitor::visit(const DynamicSymbolCommand& dynamic_symbol) {
  visit(*dynamic_symbol.as<LoadCommand>());

  node_["idx_local_symbol"]                 = dynamic_symbol.idx_local_symbol();
  node_["nb_local_symbols"]                 = dynamic_symbol.nb_local_symbols();
  node_["idx_external_define_symbol"]       = dynamic_symbol.idx_external_define_symbol();
  node_["nb_external_define_symbols"]       = dynamic_symbol.nb_external_define_symbols();
  node_["idx_undefined_symbol"]             = dynamic_symbol.idx_undefined_symbol();
  node_["nb_undefined_symbols"]             = dynamic_symbol.nb_undefined_symbols();
  node_["toc_offset"]                       = dynamic_symbol.toc_offset();
  node_["nb_toc"]                           = dynamic_symbol.nb_toc();
  node_["module_table_offset"]              = dynamic_symbol.module_table_offset();
  node_["nb_module_table"]                  = dynamic_symbol.nb_module_table();
  node_["external_reference_symbol_offset"] = dynamic_symbol.external_reference_symbol_offset();
  node_["nb_external_reference_symbols"]    = dynamic_symbol.nb_external_reference_symbols();
  node_["indirect_symbol_offset"]           = dynamic_symbol.indirect_symbol_offset();
  node_["nb_indirect_symbols"]              = dynamic_symbol.nb_indirect_symbols();
  node_["external_relocation_offset"]       = dynamic_symbol.external_relocation_offset();
  node_["nb_external_relocations"]          = dynamic_symbol.nb_external_relocations();
  node_["local_relocation_offset"]          = dynamic_symbol.local_relocation_offset();
  node_["nb_local_relocations"]             = dynamic_symbol.nb_local_relocations();
}

void JsonVisitor::visit(const DylinkerCommand& dylinker) {
  visit(*dylinker.as<LoadCommand>());

  node_["name"] = dylinker.name();
}

void JsonVisitor::visit(const DylibCommand& dylib) {
  visit(*dylib.as<LoadCommand>());

  node_["name"]                  = dylib.name();
  node_["timestamp"]             = dylib.timestamp();
  node_["current_version"]       = dylib.current_version();
  node_["compatibility_version"] = dylib.compatibility_version();
}

void JsonVisitor::visit(const ThreadCommand& threadcmd) {
  visit(*threadcmd.as<LoadCommand>());

  node_["flavor"] = threadcmd.flavor();
  node_["count"]  = threadcmd.count();
  node_["pc"]     = threadcmd.pc();
}

void JsonVisitor::visit(const RPathCommand& rpath) {
  visit(*rpath.as<LoadCommand>());

  node_["path"] = rpath.path();
}

void JsonVisitor::visit(const Routine& routine) {
  visit(*routine.as<LoadCommand>());

  node_["init_address"] = routine.init_address();
  node_["init_module"] = routine.init_module();
  node_["reserved1"] = routine.reserved1();
  node_["reserved2"] = routine.reserved2();
  node_["reserved3"] = routine.reserved3();
  node_["reserved4"] = routine.reserved4();
  node_["reserved5"] = routine.reserved5();
  node_["reserved6"] = routine.reserved6();
}

void JsonVisitor::visit(const Symbol& symbol) {
  node_["value"]             = symbol.value();
  node_["size"]              = symbol.size();
  node_["name"]              = symbol.name();

  node_["type"]              = symbol.type();
  node_["numberof_sections"] = symbol.numberof_sections();
  node_["description"]       = symbol.description();
  node_["origin"]            = to_string(symbol.origin());
  node_["is_external"]       = symbol.is_external();

  if (symbol.has_export_info()) {
    JsonVisitor v;
    v(*symbol.export_info());
    node_["export_info"] = v.get();
  }

  if (symbol.has_binding_info()) {
    JsonVisitor v;
    v(*symbol.binding_info());
    node_["binding_info"] = v.get();
  }
}

void JsonVisitor::visit(const Relocation& relocation) {

  node_["is_pc_relative"] = relocation.is_pc_relative();
  node_["architecture"]   = to_string(relocation.architecture());
  node_["origin"]         = to_string(relocation.origin());
  if (relocation.has_symbol()) {
    node_["symbol"] = relocation.symbol()->name();
  }

  if (relocation.has_section()) {
    node_["section"] = relocation.section()->name();
  }

  if (relocation.has_segment()) {
    node_["segment"] = relocation.segment()->name();
  }
}

void JsonVisitor::visit(const RelocationObject& robject) {
  visit(*robject.as<Relocation>());

  node_["value"]        = robject.value();
  node_["is_scattered"] = robject.is_scattered();
}

void JsonVisitor::visit(const RelocationDyld& rdyld) {
  visit(*rdyld.as<Relocation>());
}

void JsonVisitor::visit(const RelocationFixup& fixup) {
  visit(*fixup.as<Relocation>());
  node_["target"] = fixup.target();
}

void JsonVisitor::visit(const BindingInfo& binding) {
  node_["address"]         = binding.address();
  node_["library_ordinal"] = binding.library_ordinal();
  node_["addend"]          = binding.addend();
  node_["is_weak_import"]  = binding.is_weak_import();

  if (binding.has_symbol()) {
    node_["symbol"] = binding.symbol()->name();
  }

  if (binding.has_segment()) {
    node_["segment"] = binding.segment()->name();
  }

  if (binding.has_library()) {
    node_["library"] = binding.library()->name();
  }
}

void JsonVisitor::visit(const DyldBindingInfo& binding) {
  visit(*binding.as<BindingInfo>());
  node_["binding_class"]   = to_string(binding.binding_class());
  node_["binding_type"]    = to_string(binding.binding_type());
  node_["original_offset"] = binding.original_offset();
}

void JsonVisitor::visit(const ChainedBindingInfo& binding) {
  visit(*binding.as<BindingInfo>());
  node_["format"] = to_string(binding.format());
}

void JsonVisitor::visit(const DyldExportsTrie& trie) {
  visit(*trie.as<LoadCommand>());
  node_["data_offset"] = trie.data_offset();
  node_["data_size"]   = trie.data_size();
}

void JsonVisitor::visit(const ExportInfo& einfo) {

  node_["flags"]   = einfo.flags();
  node_["address"] = einfo.address();

  if (einfo.has_symbol()) {
    node_["symbol"] = einfo.symbol()->name();
  }
}

void JsonVisitor::visit(const FunctionStarts& fs) {
  visit(*fs.as<LoadCommand>());

  node_["data_offset"] = fs.data_offset();
  node_["data_size"]   = fs.data_size();
  node_["functions"]   = fs.functions();
}

void JsonVisitor::visit(const CodeSignature& cs) {
  visit(*cs.as<LoadCommand>());
  node_["data_offset"] = cs.data_offset();
  node_["data_size"]   = cs.data_size();
}

void JsonVisitor::visit(const DataInCode& dic) {
  visit(*dic.as<LoadCommand>());
  std::vector<json> entries;
  for (const DataCodeEntry& e : dic.entries()) {
    JsonVisitor v;
    v(e);
    entries.emplace_back(v.get());
  }

  node_["data_offset"] = dic.data_offset();
  node_["data_size"]   = dic.data_size();
  node_["entries"]     = entries;
}

void JsonVisitor::visit(const DataCodeEntry& dce) {
  node_["offset"] = dce.offset();
  node_["length"] = dce.length();
  node_["type"]   = to_string(dce.type());
}


void JsonVisitor::visit(const SourceVersion& sv) {
  visit(*sv.as<LoadCommand>());
  node_["version"] = sv.version();
}


void JsonVisitor::visit(const VersionMin& vmin) {
  visit(*vmin.as<LoadCommand>());

  node_["version"] = vmin.version();
  node_["sdk"]     = vmin.sdk();
}

void JsonVisitor::visit(const UnknownCommand& ukn) {
  visit(*ukn.as<LoadCommand>());

  node_["original_command"] = ukn.original_command();
}

void JsonVisitor::visit(const SegmentSplitInfo& ssi) {
  visit(*ssi.as<LoadCommand>());
  node_["data_offset"] = ssi.data_offset();
  node_["data_size"]   = ssi.data_size();
}

void JsonVisitor::visit(const SubFramework& sf) {
  visit(*sf.as<LoadCommand>());
  node_["umbrella"] = sf.umbrella();
}

void JsonVisitor::visit(const DyldEnvironment& dv) {
  visit(*dv.as<LoadCommand>());
  node_["value"] = dv.value();
}


void JsonVisitor::visit(const EncryptionInfo& e) {
  visit(*e.as<LoadCommand>());
  node_["crypt_offset"] = e.crypt_offset();
  node_["crypt_size"]   = e.crypt_size();
  node_["crypt_id"]     = e.crypt_id();
}


void JsonVisitor::visit(const BuildVersion& e) {
  visit(*e.as<LoadCommand>());

  node_["platform"] = to_string(e.platform());
  node_["minos"]    = e.minos();
  node_["sdk"]      = e.sdk();

  std::vector<json> tools;

  for (const BuildToolVersion& toolv : e.tools()) {
    JsonVisitor v;
    v(toolv);
    tools.emplace_back(v.get());
  }
  node_["tools"] = tools;
}


void JsonVisitor::visit(const BuildToolVersion& e) {
  node_["tool"]    = to_string(e.tool());
  node_["version"] = e.version();
}

void JsonVisitor::visit(const FilesetCommand& e) {
  node_["name"]            = e.name();
  node_["file_offset"]     = e.file_offset();
  node_["virtual_address"] = e.virtual_address();
}

void JsonVisitor::visit(const CodeSignatureDir& e) {
  node_["data_size"]   = e.data_size();
  node_["data_offset"] = e.data_offset();
}

void JsonVisitor::visit(const LinkerOptHint& e) {
  node_["data_size"]   = e.data_size();
  node_["data_offset"] = e.data_offset();
}

void JsonVisitor::visit(const TwoLevelHints& e) {
  auto it_hints = e.hints();
  std::vector<uint32_t> hints = {std::begin(it_hints), std::end(it_hints)};
  node_["offset"] = e.offset();
  node_["hints"]  = hints;
}



} // namespace MachO
} // namespace LIEF

