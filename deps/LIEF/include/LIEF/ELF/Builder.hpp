/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 * Copyright 2017 - 2021, NVIDIA CORPORATION. All rights reserved
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
#ifndef LIEF_ELF_BUIDLER_H
#define LIEF_ELF_BUIDLER_H

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "LIEF/errors.hpp"

#include "LIEF/visibility.h"
#include "LIEF/iostream.hpp"

namespace LIEF {
namespace ELF {
class Binary;
class Layout;
class Header;
class Note;
class DynamicEntryArray;
class DynamicEntry;
class Section;
class ExeLayout;
class ObjectFileLayout;
class Layout;
class Relocation;

/// Class which takes an ELF::Binary object and reconstructs a valid binary
///
/// This interface assumes that the layout of input ELF binary is correct (i.e.
/// the binary can run).
class LIEF_API Builder {
  friend class ObjectFileLayout;
  friend class Layout;
  friend class ExeLayout;

  public:
  /// Configuration options to tweak the building process
  struct config_t {
    bool dt_hash         = true;  /// Rebuild DT_HASH
    bool dyn_str         = true;  /// Rebuild DT_STRTAB
    bool dynamic_section = true;  /// Rebuild PT_DYNAMIC segment
    bool fini_array      = true;  /// Rebuild DT_FINI_ARRAY
    bool gnu_hash        = true;  /// Rebuild DT_GNU_HASH
    bool init_array      = true;  /// Rebuild DT_INIT_ARRAY
    bool interpreter     = true;  /// Rebuild PT_INTERPRETER
    bool jmprel          = true;  /// Rebuild DT_JMPREL
    bool notes           = false; /// Disable note building since it can break the default layout
    bool preinit_array   = true;  /// Rebuild DT_PREINIT_ARRAY
    bool relr            = true;  /// Rebuild DT_RELR
    bool android_rela    = true;  /// Rebuild DT_ANDROID_REL[A]
    bool rela            = true;  /// Rebuild DT_REL[A]
    bool static_symtab   = true;  /// Rebuild `.symtab`
    bool sym_verdef      = true;  /// Rebuild DT_VERDEF
    bool sym_verneed     = true;  /// Rebuild DT_VERNEED
    bool sym_versym      = true;  /// Rebuild DT_VERSYM
    bool symtab          = true;  /// Rebuild DT_SYMTAB
    bool coredump_notes  = true;  /// Rebuild the Coredump notes
    bool force_relocate  = false; /// Force to relocating all the ELF structures that are supported by LIEF (mostly for testing)
    bool skip_dynamic    = false; /// Skip relocating the PT_DYNAMIC segment (only relevant if force_relocate is set)

    /// Remove entries in `.gnu.version_r` if they are not associated with at
    /// least one version
    bool keep_empty_version_requirement = false;
  };

  Builder(Binary& binary, const config_t& config);
  Builder(Binary& binary) :
    Builder(binary, config_t())
  {}

  Builder() = delete;
  ~Builder();

  /// Perform the build of the provided ELF binary
  void build();

  config_t& config() {
    return config_;
  }

  /// Return the built ELF binary as a byte vector
  const std::vector<uint8_t>& get_build();

  /// Write the built ELF binary in the ``filename`` given in parameter
  void write(const std::string& filename) const;

  /// Write the built ELF binary in the stream ``os`` given in parameter
  void write(std::ostream& os) const;

  protected:
  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_relocatable();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_exe_lib();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build(const Header& header);

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_sections();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_segments();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_symtab_symbols();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_dynamic();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_dynamic_section();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_dynamic_symbols();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_obj_symbols();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_dynamic_relocations();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_relative_relocations();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_android_relocations();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_pltgot_relocations();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_section_relocations();

  LIEF_LOCAL uint32_t sort_dynamic_symbols();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_hash_table();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_symbol_hash();

  LIEF_LOCAL ok_error_t build_empty_symbol_gnuhash();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_symbol_requirement();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_symbol_definition();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_symbol_version();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_interpreter();

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_notes();

  LIEF_LOCAL ok_error_t update_note_section(const Note& note, std::set<const Note*>& notes);

  template<typename ELF_T>
  LIEF_LOCAL ok_error_t build_overlay();

  LIEF_LOCAL bool should_swap() const;

  template<class ELF_T>
  LIEF_LOCAL ok_error_t process_object_relocations();

  LIEF_LOCAL bool should_build_notes() const;

  config_t config_;
  mutable vector_iostream ios_;
  Binary* binary_ = nullptr;
  std::unique_ptr<Layout> layout_;
};

} // namespace ELF
} // namespace LIEF




#endif
