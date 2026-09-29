/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#ifndef LIEF_RUNTIME_WINDOWS_LDR_DATA_TABLE_ENTRY_H
#define LIEF_RUNTIME_WINDOWS_LDR_DATA_TABLE_ENTRY_H
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include <LIEF/visibility.h>
#include <LIEF/compiler_attributes.hpp>
#include <LIEF/iterators.hpp>
#include <LIEF/optional.hpp>

namespace LIEF {
namespace runtime {
namespace windows {

namespace details {
class ldr_entry;
class ldr_entry_it;
}

/// This class exposes a user-friendly interface over a `LDR_DATA_TABLE_ENTRY`,
/// the structure used by the Windows loader to describe a module loaded in the
/// current process.
class LIEF_API LdrDataTableEntry {
  public:
  /// Bidirectional iterator over the LdrDataTableEntry mirroring the
  /// doubly-linked list used by Windows.
  class Iterator final
    : public iterator_facade_base<Iterator, std::bidirectional_iterator_tag,
                                  LdrDataTableEntry, std::ptrdiff_t,
                                  const LdrDataTableEntry*,
                                  const LdrDataTableEntry&> {
    public:
    using implementation = details::ldr_entry_it;
    using iterator_facade_base::operator++;
    using iterator_facade_base::operator--;

    LIEF_API Iterator();

    LIEF_API Iterator(std::unique_ptr<details::ldr_entry_it> impl);

    LIEF_API Iterator(const Iterator&);
    LIEF_API Iterator& operator=(const Iterator&);

    LIEF_API Iterator(Iterator&&) noexcept;
    LIEF_API Iterator& operator=(Iterator&&) noexcept;

    LIEF_API ~Iterator();

    friend LIEF_API bool operator==(const Iterator& LHS, const Iterator& RHS);
    friend bool operator!=(const Iterator& LHS, const Iterator& RHS) {
      return !(LHS == RHS);
    }

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator++() LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API Iterator& operator--() LIEF_LIFETIMEBOUND;

    LIEF_API const LdrDataTableEntry& operator*() const LIEF_LIFETIMEBOUND;

    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
    LIEF_API const LdrDataTableEntry* operator->() const LIEF_LIFETIMEBOUND;

    /// Transfer ownership of the entry at the current position to the caller.
    /// Returns `nullptr` if the iterator is past-the-end.
    LIEF_API std::unique_ptr<LdrDataTableEntry> yield();

    private:
    void load() const;

    std::unique_ptr<details::ldr_entry_it> impl_;
    mutable std::unique_ptr<LdrDataTableEntry> cached_;
  };

  LdrDataTableEntry() = delete;
  LdrDataTableEntry(std::unique_ptr<details::ldr_entry> impl);

  LdrDataTableEntry(const LdrDataTableEntry&) = delete;
  LdrDataTableEntry& operator=(const LdrDataTableEntry&) = delete;

  LdrDataTableEntry(LdrDataTableEntry&&) noexcept;
  LdrDataTableEntry& operator=(LdrDataTableEntry&&) noexcept;

  /// Base address at which the module is mapped in memory (`DllBase`).
  uintptr_t dll_base() const;

  /// Address of the entry point of the module (`EntryPoint`).
  uintptr_t entry_point() const;

  /// Size (in bytes) of the module's image in memory (`SizeOfImage`).
  uint32_t size_of_image() const;

  /// Full path of the module (`FullDllName`),
  /// e.g. `C:\Windows\System32\ntdll.dll`.
  std::string full_dll_name() const;

  /// Base name of the module (`BaseDllName`), e.g. `ntdll.dll`.
  std::string base_dll_name() const;

  /// Loader flags describing the state of the module (`Flags`).
  uint32_t flags() const;

  /// Legacy load count of the module (`ObsoleteLoadCount`). Superseded by
  /// `reference_count()` on Windows 8 and later.
  uint16_t obsolete_load_count() const;

  /// TLS slot index assigned to the module, or `0` when it has no TLS
  /// (`TlsIndex`).
  uint16_t tls_index() const;

  /// `TimeDateStamp` of the module as cached by the loader.
  uint32_t time_date_stamp() const;

  /// Address of the activation context associated with the module's entry
  /// point.
  uintptr_t entry_point_activation_context() const;

  /// Address of the per-entry loader lock.
  uintptr_t lock() const;

  /// Address of the dependency-graph node of the module (`DdagNode`).
  ///
  /// \note Available on Windows 8 and later.
  optional<uintptr_t> ddag_node() const;

  /// Address of the loader context used while the module is being snapped
  ///
  /// \note Available on Windows 8 and later.
  optional<uintptr_t> load_context() const;

  /// Base address of the module that triggered the load of this one.
  ///
  /// \note Available on Windows 8 and later.
  optional<uintptr_t> parent_dll_base() const;

  /// Address of the CHPE switch-back context.
  ///
  /// \note Available on Windows 8 and later.
  optional<uintptr_t> switch_back_context() const;

  /// Preferred base address recorded in the PE headers
  ///
  /// \note Available on Windows 8 and later.
  optional<uintptr_t> original_base() const;

  /// Time at which the module was loaded.
  ///
  /// \note Available on Windows 8 and later.
  optional<int64_t> load_time() const;

  /// Hash of the module's base name used to index the loader tables
  ///
  /// \note Available on Windows 8 and later.
  optional<uint32_t> base_name_hash_value() const;

  /// Reason why the module was loaded, as a `LDR_DLL_LOAD_REASON` value
  ///
  /// \note Available on Windows 8 and later.
  optional<int32_t> load_reason() const;

  /// Path-search options implied when the module was resolved
  ///
  /// \note Available on Windows 8 and later.
  optional<uint32_t> implicit_path_options() const;

  /// Number of references currently held on the module.
  ///
  /// \note Available on Windows 8 and later.
  optional<uint32_t> reference_count() const;

  /// Flags controlling how the statically-linked dependencies of the module
  /// are loaded.
  ///
  /// \note Available on Windows 8 and later.
  optional<uint32_t> dependent_load_flags() const;

  /// Signing level of the module's image, as a `SE_SIGNING_LEVEL` value
  ///
  /// \note Available on Windows 10 and later.
  optional<uint8_t> signing_level() const;

  /// Image checksum cached by the loader
  ///
  /// \note Available on Windows 10 and later.
  optional<uint32_t> check_sum() const;

  /// Base address of the active hot-patch image, if any.
  ///
  /// \note Available on Windows 11 and later.
  optional<uintptr_t> active_patch_image_base() const;

  /// State of the hot-patch engine for this module, as a `LDR_HOT_PATCH_STATE`
  /// value.
  ///
  /// \note Available on Windows 11 and later.
  optional<uint32_t> hot_patch_state() const;

  /// Pretty-printed representation of this entry.
  std::string to_string() const;

  LIEF_API friend std::ostream& operator<<(std::ostream& os,
                                           const LdrDataTableEntry& entry) {
    os << entry.to_string();
    return os;
  }

  ~LdrDataTableEntry();

  private:
  std::unique_ptr<details::ldr_entry> impl_;
};

}
}
}
#endif
