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
#ifndef LIEF_PE_BUILDER_H
#define LIEF_PE_BUILDER_H

#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/iostream.hpp"

#include "LIEF/errors.hpp"

namespace LIEF {
namespace PE {
class Binary;
class ResourceNode;
class ResourceDirectory;
class ResourceData;
class DosHeader;
class Header;
class OptionalHeader;
class DataDirectory;
class Section;
class Import;
class ImportEntry;

/// Class that is used to rebuild a raw PE binary from a PE::Binary object
class LIEF_API Builder {
  public:
  /// This structure is used to configure the build operation.
  ///
  /// The default value of these attributes is set to `false` if the
  /// operation modifies the binary layout even though nothing changed.
  /// For instance, building the import table **always** requires relocating the
  /// table to another place. Thus, the default value is false and must
  /// be explicitly set to true.
  struct config_t {
    /// Whether the builder should reconstruct the imports table. This option
    /// should be turned on if you modify imports.
    ///
    /// Please check LIEF website for more details
    bool imports = false;

    /// Whether the builder should reconstruct the export table This option
    /// should be turned on if you modify exports.
    ///
    /// Please check LIEF website for more details
    bool exports = false;

    /// Whether the builder should regenerate the resources tree
    bool resources =  true;

    /// Whether the builder should regenerate relocations
    bool relocations = true;

    /// Whether the builder should regenerate the load config
    bool load_configuration = true;

    /// Whether the builder should regenerate the TLS info
    bool tls = true;

    /// Whether the builder should write back any overlay data
    bool overlay = true;

    /// Whether the builder should regenerate debug entries
    bool debug = true;

    /// Whether the builder should write back dos stub (including the rich
    /// header)
    bool dos_stub = true;

    /// If the resources tree needs to be relocated, this defines the name of
    /// the new section that contains the relocated tree
    std::string rsrc_section = ".rsrc";

    /// Section that holds the relocated import table (IAT/ILT)
    std::string idata_section = ".idata";

    /// Section that holds the relocated TLS info
    std::string tls_section = ".tls";

    /// Section that holds the relocated relocations
    std::string reloc_section = ".reloc";

    /// Section that holds the export table
    std::string export_section = ".edata";

    /// Section that holds the debug entries
    std::string debug_section = ".debug";

    using resolved_iat_cbk_t =
      std::function<void(Binary*, const Import*, const ImportEntry*, uint32_t)>;
    resolved_iat_cbk_t resolved_iat_cbk = nullptr;

    /// \private
    bool force_relocating = false;
  };

  Builder() = delete;
  Builder(Binary& binary, const config_t& config) :
    binary_(&binary),
    config_(config)
  {}

  ~Builder();

  /// Perform the build process
  ok_error_t build();

  /// Return the build result
  const std::vector<uint8_t>& get_build() {
    return ios_.raw();
  }

  /// Write the build result into the `output` file
  void write(const std::string& filename) const;

  /// Write the build result into the `os` stream
  void write(std::ostream& os) const;

  ok_error_t build(const DosHeader& dos_header);
  ok_error_t build(const Header& header);
  ok_error_t build(const OptionalHeader& optional_header);
  ok_error_t build(const DataDirectory& data_directory);
  ok_error_t build(const Section& section);
  ok_error_t build_overlay();

  ok_error_t build_relocations();
  ok_error_t build_resources();
  ok_error_t build_debug_info();

  ok_error_t build_exports();

  template<typename PE_T>
  ok_error_t build_imports();

  template<typename PE_T>
  ok_error_t build_tls();

  template<typename PE_T>
  ok_error_t build_load_config();

  const std::vector<uint8_t>& rsrc_data() const {
    return rsrc_data_;
  }

  protected:
  struct tls_data_t {
    std::vector<uint8_t> header;
    std::vector<uint8_t> callbacks;
  };

  struct rsrc_sizing_info_t {
    uint32_t header_size = 0;
    uint32_t data_size = 0;
    uint32_t name_size = 0;
  };

  struct rsrc_build_context_t {
    uint32_t offset_header = 0;
    uint32_t offset_data = 0;
    uint32_t offset_name = 0;
    uint32_t depth = 0;
    std::vector<uint64_t> rva_fixups;
  };

  template<typename PE_T>
  ok_error_t build_optional_header(const OptionalHeader& optional_header);

  static ok_error_t compute_resources_size(const ResourceNode& node,
                                           rsrc_sizing_info_t& info);

  static ok_error_t construct_resource(
      vector_iostream& ios, ResourceNode& node, rsrc_build_context_t& ctx);

  static ok_error_t construct_resource(
      vector_iostream& ios, ResourceDirectory& dir, rsrc_build_context_t& ctx);

  static ok_error_t construct_resource(
      vector_iostream& ios, ResourceData& dir, rsrc_build_context_t& ctx);

  mutable vector_iostream ios_;
  Binary* binary_ = nullptr;
  config_t config_;
  std::vector<uint8_t> reloc_data_;
  std::vector<uint8_t> rsrc_data_;
  tls_data_t tls_data_;
};

}
}
#endif
