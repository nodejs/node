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
#ifndef LIEF_MACHO_FUNCTION_VARIANTS_COMMAND_H
#define LIEF_MACHO_FUNCTION_VARIANTS_COMMAND_H
#include <vector>
#include <ostream>

#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
class SpanStream;
class BinaryStream;

namespace MachO {
class BinaryParser;
class LinkEdit;

namespace details {
struct linkedit_data_command;
struct runtime_table_entry_t;
}

/// Class representing the `LC_FUNCTION_VARIANTS` load command.
///
/// Introduced publicly in `dyld-1284.13` (April 2025), this command supports
/// **function multiversioning**, the ability to associate multiple implementations
/// of the same function, each optimized for a specific platform, architecture,
/// or runtime context.
///
/// At runtime, the system dispatches the most appropriate variant based on
/// hardware capabilities or execution environment.
///
/// For example:
///
/// ```cpp
/// FUNCTION_VARIANT_TABLE(my_function,
///   { (void*)my_function$Rosetta,  "rosetta" }, // Rosetta translation
///   { (void*)my_function$Haswell,  "haswell" }, // Haswell-optimized
///   { (void*)my_function$Base,     "default" }  // Default fallback
/// );
/// ```
class LIEF_API FunctionVariants : public LoadCommand {
  friend class BinaryParser;
  friend class LinkEdit;

  public:
  /// This class exposes information about a given implementation.
  class LIEF_API RuntimeTableEntry {
    public:
    static constexpr uint32_t F_BIT = 20;
    static constexpr uint32_t F_MASK = (uint32_t(1) << F_BIT) - 1;

    static constexpr uint32_t F_PER_PROCESS = uint32_t(1) << F_BIT;
    static constexpr uint32_t F_SYSTEM_WIDE = uint32_t(2) << F_BIT;
    static constexpr uint32_t F_ARM64       = uint32_t(3) << F_BIT;
    static constexpr uint32_t F_X86_64      = uint32_t(4) << F_BIT;

    /// Flags describing the target platform, environment, or architecture
    /// for a given function implementation.
    ///
    /// These are encoded as a `uint32_t`, where high bits determine the namespace
    /// (`KIND`), and the lower bits encode the specific capability.
    enum class FLAGS : uint32_t {
      UNKNOWN = 0,

      #define FUNCTION_VARIANT_FLAG(name, value, _) name = (value | F_ARM64),
        #include "LIEF/MachO/FunctionVariants/Arm64.def"
      #undef FUNCTION_VARIANT_FLAG

      #define FUNCTION_VARIANT_FLAG(name, value, _) name = (value | F_X86_64),
        #include "LIEF/MachO/FunctionVariants/X86_64.def"
      #undef FUNCTION_VARIANT_FLAG

      #define FUNCTION_VARIANT_FLAG(name, value, _) name = (value | F_SYSTEM_WIDE),
        #include "LIEF/MachO/FunctionVariants/SystemWide.def"
      #undef FUNCTION_VARIANT_FLAG

      #define FUNCTION_VARIANT_FLAG(name, value, _) name = (value | F_PER_PROCESS),
        #include "LIEF/MachO/FunctionVariants/PerProcess.def"
      #undef FUNCTION_VARIANT_FLAG
    };

    static uint8_t get_raw(FLAGS f) {
      return (uint8_t)f;
    }

    RuntimeTableEntry() = default;
    RuntimeTableEntry(const details::runtime_table_entry_t& entry);

    RuntimeTableEntry(const RuntimeTableEntry&) = default;
    RuntimeTableEntry& operator=(const RuntimeTableEntry&) = default;

    RuntimeTableEntry(RuntimeTableEntry&&) noexcept = default;
    RuntimeTableEntry& operator=(RuntimeTableEntry&&) noexcept = default;

    ~RuntimeTableEntry() = default;

    /// The relative address of the implementation or an index if
    /// another_table() is set.
    uint32_t impl() const {
      return impl_;
    }

    /// Indicates whether impl() refers to an entry in another runtime table,
    /// rather than a direct function implementation address.
    bool another_table() const {
      return another_table_;
    }

    /// The `flagBitNums` value as a slice of bytes
    span<const uint8_t> flag_bit_nums() const {
      return flag_bit_nums_;
    }

    /// Return the **interpreted** flag_bit_nums()
    const std::vector<FLAGS>& flags() const {
      return flags_;
    }

    std::string to_string() const;

    LIEF_API friend
      std::ostream& operator<<(std::ostream& os, const RuntimeTableEntry& entry)
    {
      os << entry.to_string();
      return os;
    }

    /// \private
    LIEF_LOCAL void set_flags(std::vector<FLAGS> flags) {
      flags_ = std::move(flags);
    }
    private:
    bool another_table_;
    uint32_t impl_ = 0;
    std::array<uint8_t, 4> flag_bit_nums_ = {};
    std::vector<FLAGS> flags_;
  };

  /// Represents a runtime table of function variants sharing a common namespace
  /// (referred to internally as `FunctionVariantsRuntimeTable` in `dyld`).
  ///
  /// Each table holds multiple RuntimeTableEntry instances that map to
  /// function implementations optimized for a given KIND.
  class LIEF_API RuntimeTable {
    public:
    using entries_t = std::vector<RuntimeTableEntry>;

    /// Iterator that output RuntimeTableEntry&
    using it_entries = ref_iterator<entries_t&>;

    /// Iterator that output const RuntimeTableEntry&
    using it_const_entries = const_ref_iterator<const entries_t&>;

    /// Enumeration describing the namespace or category of a function variant.
    ///
    /// Each FunctionVariants::RuntimeTable is associated with one KIND,
    /// which indicates the domain or context under which its variant entries
    /// should be considered valid or applicable.
    ///
    /// These categories map to the runtime dispatch logic used by `dyld`
    /// when selecting the optimal function variant.
    enum class KIND : uint32_t {
      /// Fallback/default kind when the category is not recognized.
      UNKNOWN = 0,

      /// Variants that apply on a per-process basis
      PER_PROCESS = 1,

      /// Variants that are selected based on system-wide capabilities or configurations.
      SYSTEM_WIDE = 2,

      /// Variants optimized for the ARM64 architecture.
      ARM64 = 3,

      /// Variants optimized for the x86-64 architecture.
      X86_64 = 4
    };

    RuntimeTable() = default;
    RuntimeTable(KIND kind, uint32_t offset) :
      kind_(kind), offset_(offset)
    {}
    RuntimeTable(const RuntimeTable&) = default;
    RuntimeTable& operator=(const RuntimeTable&) = default;

    RuntimeTable(RuntimeTable&&) noexcept = default;
    RuntimeTable& operator=(RuntimeTable&&) noexcept = default;

    ~RuntimeTable() = default;

    /// Kind of this runtime table
    KIND kind() const {
      return kind_;
    }

    /// Original offset in the payload
    uint32_t offset() const {
      return offset_;
    }

    /// Iterator over the different RuntimeTableEntry entries
    it_entries entries() {
      return entries_;
    }

    it_const_entries entries() const {
      return entries_;
    }

    void add(RuntimeTableEntry entry) {
      entries_.push_back(std::move(entry));
    }

    std::string to_string() const;

    LIEF_API friend
      std::ostream& operator<<(std::ostream& os, const RuntimeTable& table)
    {
      os << table.to_string();
      return os;
    }

    private:
    KIND kind_ = KIND::UNKNOWN;
    uint32_t offset_ = 0;
    entries_t entries_;
  };

  using runtime_table_t = std::vector<RuntimeTable>;

  /// Iterator that outputs RuntimeTable&
  using it_runtime_table = ref_iterator<runtime_table_t&>;

  /// Iterator that outputs const RuntimeTable&
  using it_const_runtime_table = const_ref_iterator<const runtime_table_t&>;

  FunctionVariants() = default;
  FunctionVariants(const details::linkedit_data_command& cmd);

  FunctionVariants& operator=(const FunctionVariants& copy) = default;
  FunctionVariants(const FunctionVariants& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<FunctionVariants>(new FunctionVariants(*this));
  }

  /// Offset in the `__LINKEDIT` SegmentCommand where the payload starts
  uint32_t data_offset() const {
    return data_offset_;
  }

  /// Size of the payload
  uint32_t data_size() const {
    return data_size_;
  }

  void data_offset(uint32_t offset) {
    data_offset_ = offset;
  }

  void data_size(uint32_t size) {
    data_size_ = size;
  }

  /// Return the data slice in the `__LINKEDIT` segment referenced by
  /// data_offset and data_size;
  span<const uint8_t> content() const {
    return content_;
  }

  span<uint8_t> content() {
    return content_;
  }

  /// Iterator over the different RuntimeTable entries located in the content
  /// of this `__LINKEDIT` command
  it_runtime_table runtime_table() {
    return runtime_table_;
  }

  it_const_runtime_table runtime_table() const {
    return runtime_table_;
  }

  ~FunctionVariants() override = default;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::FUNCTION_VARIANTS;
  }

  LIEF_LOCAL static std::vector<RuntimeTable> parse_payload(SpanStream& stream);
  LIEF_LOCAL static result<RuntimeTable> parse_entry(BinaryStream& stream);

  private:
  uint32_t data_offset_ = 0;
  uint32_t data_size_ = 0;
  span<uint8_t> content_;
  std::vector<RuntimeTable> runtime_table_;
};

LIEF_API const char* to_string(FunctionVariants::RuntimeTable::KIND kind);
LIEF_API const char* to_string(FunctionVariants::RuntimeTableEntry::FLAGS kind);

}
}
#endif
