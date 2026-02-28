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

#include "ELF/json_internal.hpp"
#include "LIEF/ELF.hpp"

namespace LIEF {
namespace ELF {

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
  for (const Segment& segment : binary.segments()) {
    JsonVisitor visitor;
    visitor(segment);
    segments.emplace_back(visitor.get());
  }

  // Dynamic entries
  std::vector<json> dynamic_entries;
  for (const DynamicEntry& entry : binary.dynamic_entries()) {
    JsonVisitor visitor;
    entry.accept(visitor);
    dynamic_entries.emplace_back(visitor.get());
  }


  // Dynamic symbols
  std::vector<json> dynamic_symbols;
  for (const Symbol& symbol : binary.dynamic_symbols()) {
    JsonVisitor visitor;
    visitor(symbol);
    dynamic_symbols.emplace_back(visitor.get());
  }


  // Symtab symbols
  std::vector<json> symtab_symbols;
  for (const Symbol& symbol : binary.symtab_symbols()) {
    JsonVisitor visitor;
    visitor(symbol);
    symtab_symbols.emplace_back(visitor.get());
  }


  // Dynamic relocations
  std::vector<json> dynamic_relocations;
  for (const Relocation& reloc : binary.dynamic_relocations()) {
    JsonVisitor visitor;
    visitor(reloc);
    dynamic_relocations.emplace_back(visitor.get());
  }


  // pltgot relocations
  std::vector<json> pltgot_relocations;
  for (const Relocation& reloc : binary.pltgot_relocations()) {
    JsonVisitor visitor;
    visitor(reloc);
    pltgot_relocations.emplace_back(visitor.get());
  }


  // Symbol version
  std::vector<json> symbols_version;
  for (const SymbolVersion& s : binary.symbols_version()) {
    JsonVisitor visitor;
    visitor(s);
    symbols_version.emplace_back(visitor.get());
  }


  // Symbols version requirement
  std::vector<json> symbols_version_requirement;
  for (const SymbolVersionRequirement& s : binary.symbols_version_requirement()) {
    JsonVisitor visitor;
    visitor(s);
    symbols_version_requirement.emplace_back(visitor.get());
  }


  // Symbols version definition
  std::vector<json> symbols_version_definition;
  for (const SymbolVersionDefinition& s : binary.symbols_version_definition()) {
    JsonVisitor visitor;
    visitor(s);
    symbols_version_definition.emplace_back(visitor.get());
  }


  // Notes
  std::vector<json> notes;
  for (const Note& note : binary.notes()) {
    JsonVisitor visitor;
    visitor(note);
    notes.emplace_back(visitor.get());
  }

  node_["entrypoint"]   = binary.entrypoint();
  node_["imagebase"]    = binary.imagebase();
  node_["virtual_size"] = binary.virtual_size();
  node_["is_pie"]       = binary.is_pie();

  if (binary.has_interpreter()) {
    node_["interpreter"] = binary.interpreter();
  }

  node_["header"]                      = header_visitor.get();
  node_["sections"]                    = sections;
  node_["segments"]                    = segments;
  node_["dynamic_entries"]             = dynamic_entries;
  node_["dynamic_symbols"]             = dynamic_symbols;
  node_["symtab_symbols"]              = symtab_symbols;
  node_["dynamic_relocations"]         = dynamic_relocations;
  node_["pltgot_relocations"]          = pltgot_relocations;
  node_["symbols_version"]             = symbols_version;
  node_["symbols_version_requirement"] = symbols_version_requirement;
  node_["symbols_version_definition"]  = symbols_version_definition;
  node_["notes"]                       = notes;

  if (binary.use_gnu_hash()) {
    JsonVisitor gnu_hash_visitor;
    gnu_hash_visitor(*binary.gnu_hash());

    node_["gnu_hash"] = gnu_hash_visitor.get();
  }

  if (binary.use_sysv_hash()) {
    JsonVisitor sysv_hash_visitor;
    sysv_hash_visitor(*binary.sysv_hash());

    node_["sysv_hash"] = sysv_hash_visitor.get();
  }

}


void JsonVisitor::visit(const Header& header) {
  node_["file_type"]                       = to_string(header.file_type());
  node_["machine_type"]                    = to_string(header.machine_type());
  node_["object_file_version"]             = to_string(header.object_file_version());
  node_["entrypoint"]                      = header.entrypoint();
  node_["program_headers_offset"]          = header.program_headers_offset();
  node_["section_headers_offset"]          = header.section_headers_offset();
  node_["processor_flag"]                  = header.processor_flag();
  node_["header_size"]                     = header.header_size();
  node_["program_header_size"]             = header.program_header_size();
  node_["processornumberof_segments_flag"] = header.numberof_segments();
  node_["section_header_size"]             = header.section_header_size();
  node_["numberof_sections"]               = header.numberof_sections();
  node_["section_name_table_idx"]          = header.section_name_table_idx();
  node_["identity_class"]                  = to_string(header.identity_class());
  node_["identity_data"]                   = to_string(header.identity_data());
  node_["identity_version"]                = to_string(header.identity_version());
  node_["identity_os_abi"]                 = to_string(header.identity_os_abi());
  node_["identity_abi_version"]            = header.identity_abi_version();
}


void JsonVisitor::visit(const Section& section) {
  std::vector<json> flags;
  for (Section::FLAGS f : section.flags_list()) {
    flags.emplace_back(to_string(f));
  }

  node_["name"]            = section.name();
  node_["virtual_address"] = section.virtual_address();
  node_["size"]            = section.size();
  node_["offset"]          = section.offset();
  node_["alignment"]       = section.alignment();
  node_["information"]     = section.information();
  node_["entry_size"]      = section.entry_size();
  node_["link"]            = section.link();
  node_["type"]            = to_string(section.type());
  node_["flags"]           = flags;
}

void JsonVisitor::visit(const Segment& segment) {

  std::vector<json> sections;
  for (const Section& section : segment.sections()) {
    sections.emplace_back(section.name());
  }

  node_["type"]             = to_string(segment.type());
  node_["flags"]            = static_cast<size_t>(segment.flags());
  node_["file_offset"]      = segment.file_offset();
  node_["virtual_address"]  = segment.virtual_address();
  node_["physical_address"] = segment.physical_address();
  node_["physical_size"]    = segment.physical_size();
  node_["virtual_size"]     = segment.virtual_size();
  node_["alignment"]        = segment.alignment();
  node_["sections"]         = sections;

}

void JsonVisitor::visit(const DynamicEntry& entry) {
  node_["tag"]   = to_string(entry.tag());
  node_["value"] = entry.value();
}


void JsonVisitor::visit(const DynamicEntryArray& entry) {
  visit(static_cast<const DynamicEntry&>(entry));
  node_["array"] = entry.array();
}


void JsonVisitor::visit(const DynamicEntryLibrary& entry) {
  visit(static_cast<const DynamicEntry&>(entry));
  node_["library"] = entry.name();
}


void JsonVisitor::visit(const DynamicEntryRpath& entry) {
  visit(static_cast<const DynamicEntry&>(entry));
  node_["rpath"] = entry.rpath();
}


void JsonVisitor::visit(const DynamicEntryRunPath& entry) {
  visit(static_cast<const DynamicEntry&>(entry));
  node_["runpath"] = entry.runpath();
}


void JsonVisitor::visit(const DynamicSharedObject& entry) {
  visit(static_cast<const DynamicEntry&>(entry));
  node_["library"] = entry.name();
}


void JsonVisitor::visit(const DynamicEntryFlags& entry) {
  visit(static_cast<const DynamicEntry&>(entry));

  const DynamicEntryFlags::flags_list_t& flags = entry.flags();
  std::vector<std::string> flags_str;
  flags_str.reserve(flags.size());

  std::transform(
      std::begin(flags), std::end(flags),
      std::back_inserter(flags_str),
      [] (DynamicEntryFlags::FLAG f) {
        return to_string(f);
      });


  node_["flags"] = flags_str;
}

void JsonVisitor::visit(const Symbol& symbol) {
  node_["type"]        = to_string(symbol.type());
  node_["binding"]     = to_string(symbol.binding());
  node_["information"] = symbol.information();
  node_["other"]       = symbol.other();
  node_["value"]       = symbol.value();
  node_["size"]        = symbol.size();
  node_["name"]        = symbol.name();

  std::string sname = symbol.demangled_name();
  if (sname.empty()) {
    sname = symbol.name();
  }
  node_["demangled_name"] = sname;
}

void JsonVisitor::visit(const Relocation& relocation) {
  std::string relocation_type = "NOT_TO_STRING";
  std::string symbol_name;
  std::string section_name;

  const Symbol* s = relocation.symbol();
  if (s != nullptr) {
    symbol_name = s->demangled_name();
    if (symbol_name.empty()) {
      symbol_name = s->name();
    }
  }
  const Section* reloc_sec = relocation.section();
  if (reloc_sec != nullptr) {
    section_name = reloc_sec->name();
  }

  relocation_type = to_string(relocation.type());

  node_["symbol_name"] = symbol_name;
  node_["address"]     = relocation.address();
  node_["type"]        = relocation_type;
  node_["section"]     = section_name;

}

void JsonVisitor::visit(const SymbolVersion& sv) {
  node_["value"] = sv.value();
  if (sv.has_auxiliary_version()) {
   node_["symbol_version_auxiliary"] = sv.symbol_version_auxiliary()->name();
  }
}

void JsonVisitor::visit(const SymbolVersionRequirement& svr) {

  std::vector<json> svar_json;

  for (const SymbolVersionAuxRequirement& svar : svr.auxiliary_symbols()) {
    JsonVisitor visitor;
    visitor(svar);
    svar_json.emplace_back(visitor.get());
  }

  node_["version"]                              = svr.version();
  node_["name"]                                 = svr.name();
  node_["symbol_version_auxiliary_requirement"] = svar_json;

}

void JsonVisitor::visit(const SymbolVersionDefinition& svd) {

  std::vector<json> sva_json;

  for (const SymbolVersionAux& sva : svd.symbols_aux()) {
    JsonVisitor visitor;
    visitor(sva);
    sva_json.emplace_back(visitor.get());
  }

  node_["version"]                  = svd.version();
  node_["flags"]                    = svd.flags();
  node_["hash"]                     = svd.hash();
  node_["symbol_version_auxiliary"] = sva_json;
}

void JsonVisitor::visit(const SymbolVersionAux& sv) {
  node_["name"] = sv.name();
}

void JsonVisitor::visit(const SymbolVersionAuxRequirement& svar) {
  node_["hash"]  = svar.hash();
  node_["flags"] = svar.flags();
  node_["other"] = svar.other();
}

void JsonVisitor::visit(const Note& note) {
  node_["name"]          = note.name();
  node_["type"]          = to_string(note.type());
  node_["original_type"] = note.original_type();
}

void JsonVisitor::visit(const NoteAbi& note_abi) {
  visit(static_cast<const Note&>(note_abi));
  if (auto abi = note_abi.abi()) {
    node_["abi"] = to_string(*abi);
  }
  if (auto version = note_abi.version()) {
    node_["version"] = *version;
  }
}

void JsonVisitor::visit(const NoteGnuProperty& note_prop) {
  visit(static_cast<const Note&>(note_prop));
  NoteGnuProperty::properties_t props = note_prop.properties();
  std::vector<json> json_props;
  json_props.reserve(props.size());

  for (const std::unique_ptr<NoteGnuProperty::Property>& prop : props) {
    json_props.push_back(to_string(prop->type()));
  }

  node_["properties"] = std::move(json_props);
}

void JsonVisitor::visit(const CorePrPsInfo& pinfo) {
  visit(static_cast<const Note&>(pinfo));
  auto info = pinfo.info();
  if (!info) {
    return;
  }
  node_["state"]     = info->state;
  node_["sname"]     = info->sname;
  node_["zombie"]    = info->zombie;
  node_["flag"]      = info->flag;
  node_["uid"]       = info->uid;
  node_["gid"]       = info->gid;
  node_["pid"]       = info->pid;
  node_["ppid"]      = info->ppid;
  node_["pgrp"]      = info->pgrp;
  node_["sid"]       = info->sid;
  node_["filename"]  = info->filename_stripped();
  node_["args"]      = info->args_stripped();
}

void JsonVisitor::visit(const CorePrStatus& pstatus) {
  visit(static_cast<const Note&>(pstatus));
  const CorePrStatus::pr_status_t& status = pstatus.status();
  node_["current_sig"] = status.cursig;
  node_["sigpend"]     = status.sigpend;
  node_["sighold"]     = status.sighold;
  node_["pid"]         = status.pid;
  node_["ppid"]        = status.ppid;
  node_["pgrp"]        = status.pgrp;
  node_["sid"]         = status.sid;
  node_["sigpend"]     = status.sigpend;

  node_["utime"] = {
    {"tv_sec",  status.utime.sec},
    {"tv_usec", status.utime.usec}
  };

  node_["stime"] = {
    {"tv_sec",  status.stime.sec},
    {"tv_usec", status.stime.sec}
  };

  node_["stime"] = {
    {"tv_sec",  status.stime.sec},
    {"tv_usec", status.stime.usec}
  };

  std::vector<uint64_t> reg_vals = pstatus.register_values();
  if (!reg_vals.empty()) {
    json regs;
    switch (pstatus.architecture()) {
      case ARCH::I386:
        {
          for (size_t i = 0; i < reg_vals.size(); ++i) {
            regs[to_string(CorePrStatus::Registers::X86(i))] = reg_vals[i];
          }
          break;
        }
      case ARCH::X86_64:
        {
          for (size_t i = 0; i < reg_vals.size(); ++i) {
            regs[to_string(CorePrStatus::Registers::X86_64(i))] = reg_vals[i];
          }
          break;
        }
      case ARCH::ARM:
        {
          for (size_t i = 0; i < reg_vals.size(); ++i) {
            regs[to_string(CorePrStatus::Registers::ARM(i))] = reg_vals[i];
          }
          break;
        }
      case ARCH::AARCH64:
        {
          for (size_t i = 0; i < reg_vals.size(); ++i) {
            regs[to_string(CorePrStatus::Registers::AARCH64(i))] = reg_vals[i];
          }
          break;
        }
      default:
        {
          regs = reg_vals;
        }
    }
    node_["regs"] = regs;
  }
}

void JsonVisitor::visit(const CoreAuxv& auxv) {
  visit(static_cast<const Note&>(auxv));
  std::vector<json> values;
  const std::map<CoreAuxv::TYPE, uint64_t> aux_values = auxv.values();
  for (const auto& [k, v] : aux_values) {
    node_[to_string(k)] = v;
  }
}

void JsonVisitor::visit(const CoreSigInfo& siginfo) {
  visit(static_cast<const Note&>(siginfo));
  if (auto signo = siginfo.signo()) {
    node_["signo"] = *signo;
  }
  if (auto sigcode = siginfo.sigcode()) {
    node_["sigcode"] = *sigcode;
  }
  if (auto sigerrno = siginfo.sigerrno()) {
    node_["sigerrno"] = *sigerrno;
  }
}

void JsonVisitor::visit(const CoreFile& file) {
  visit(static_cast<const Note&>(file));
  std::vector<json> files;
  for (const CoreFile::entry_t& entry : file.files()) {
    const json file = {
      {"start",    entry.start},
      {"end",      entry.end},
      {"file_ofs", entry.file_ofs},
      {"path",     entry.path}
    };
    files.emplace_back(file);
  }
  node_["files"] = files;
  node_["count"] = file.count();
}

void JsonVisitor::visit(const AndroidIdent& ident) {
  visit(static_cast<const Note&>(ident));
  node_["ndk_version"] = ident.ndk_version();
  node_["sdk_verison"] = ident.sdk_version();
  node_["ndk_build_number"] = ident.ndk_build_number();
}


void JsonVisitor::visit(const QNXStack& note) {
  visit(static_cast<const Note&>(note));
  node_["stack_size"] = note.stack_size();
  node_["sdk_verison"] = note.stack_allocated();
  node_["is_executable"] = note.is_executable();
}


void JsonVisitor::visit(const GnuHash& gnuhash) {
  node_["nb_buckets"]    = gnuhash.nb_buckets();
  node_["symbol_index"]  = gnuhash.symbol_index();
  node_["shift2"]        = gnuhash.shift2();
  node_["maskwords"]     = gnuhash.maskwords();
  node_["bloom_filters"] = gnuhash.bloom_filters();
  node_["buckets"]       = gnuhash.buckets();
  node_["hash_values"]   = gnuhash.hash_values();
}


void JsonVisitor::visit(const SysvHash& sysvhash) {
  node_["nbucket"] = sysvhash.nbucket();
  node_["nchain"]  = sysvhash.nchain();
  node_["buckets"] = sysvhash.buckets();
  node_["chains"]  = sysvhash.chains();
}



} // namespace ELF
} // namespace LIEF

