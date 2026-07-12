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
#ifndef LIEF_MACHO_EXPORT_INFO_COMMAND_H
#define LIEF_MACHO_EXPORT_INFO_COMMAND_H
#include <vector>
#include <ostream>
#include <cstdint>

#include "LIEF/visibility.h"
#include "LIEF/enums.hpp"
#include "LIEF/Object.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;
class Symbol;
class DylibCommand;
class Binary;

/// Class that provides an interface over the Dyld export info
///
/// This class does not represent a structure that exists in the Mach-O format
/// specification but provides a *view* on an entry of the Dyld export trie.
class LIEF_API ExportInfo : public Object {

  friend class BinaryParser;
  friend class Binary;

  public:
  enum class KIND: uint64_t  {
    REGULAR           = 0x00u,
    THREAD_LOCAL_KIND = 0x01u,
    ABSOLUTE_KIND     = 0x02u
  };

  enum class FLAGS: uint64_t  {
    WEAK_DEFINITION     = 0x04u,
    REEXPORT            = 0x08u,
    STUB_AND_RESOLVER   = 0x10u,
    STATIC_RESOLVER     = 0x20u,
  };

  using flag_list_t = std::vector<FLAGS>;

  ExportInfo() = default;
  ExportInfo(uint64_t address, uint64_t flags, uint64_t offset = 0) :
    node_offset_(offset),
    flags_(flags),
    address_(address)
  {}

  ExportInfo& operator=(ExportInfo copy);
  ExportInfo(const ExportInfo& copy);
  void swap(ExportInfo& other) noexcept;

  /// Original offset in the export Trie
  uint64_t node_offset() const {
    return node_offset_;
  }

  /// Some information (ExportInfo::FLAGS) about the export.
  /// (like weak export, reexport, ...)
  uint64_t flags() const {
    return flags_;
  }

  void flags(uint64_t flags) {
    flags_ = flags;
  }

  /// The export flags() as a list
  flag_list_t flags_list() const;

  /// Check if the current entry contains the provided ExportInfo::FLAGS
  bool has(FLAGS flag) const;

  /// The export's kind (regular, thread local, absolute, ...)
  KIND kind() const {
    static constexpr auto MASK = uint64_t(3);
    return KIND(flags_ & MASK);
  }

  uint64_t other() const {
    return other_;
  }

  /// The address of the export
  uint64_t address() const {
    return address_;
  }
  void address(uint64_t addr) {
    address_ = addr;
  }

  /// Check if a symbol is associated with this export
  bool has_symbol() const {
    return symbol() != nullptr;
  }

  /// MachO::Symbol associated with this export or a nullptr if no symbol
  const Symbol* symbol() const {
    return symbol_;
  }
  Symbol* symbol() {
    return symbol_;
  }

  /// If the export is a ExportInfo::FLAGS::REEXPORT,
  /// this returns the (optional) MachO::Symbol
  Symbol* alias() {
    return alias_;
  }
  const Symbol* alias() const {
    return alias_;
  }

  /// If the export is a ExportInfo::FLAGS::REEXPORT,
  /// this returns the (optional) library (MachO::DylibCommand)
  DylibCommand* alias_library() {
    return alias_location_;
  }
  const DylibCommand* alias_library() const {
    return alias_location_;
  }

  ~ExportInfo() override = default;

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const ExportInfo& export_info);

  private:
  uint64_t node_offset_ = 0;
  uint64_t flags_ = 0;
  uint64_t address_ = 0;
  uint64_t other_ = 0;
  Symbol* symbol_ = nullptr;

  Symbol* alias_ = nullptr;
  DylibCommand* alias_location_ = nullptr;
};

LIEF_API const char* to_string(ExportInfo::KIND kind);
LIEF_API const char* to_string(ExportInfo::FLAGS flags);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::MachO::ExportInfo::FLAGS);

#endif
