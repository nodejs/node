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
#ifndef LIEF_RUNTIME_WINDOWS_PEB_H
#define LIEF_RUNTIME_WINDOWS_PEB_H
#include <cstdint>
#include <memory>
#include <LIEF/visibility.h>
#include <LIEF/iterators.hpp>
#include <LIEF/runtime/windows/LdrDataTableEntry.hpp>

namespace LIEF {
namespace runtime {
namespace windows {
class Process;

namespace details {
class peb;
}

/// This class exposes a user-friendly interface over the Process Environment
/// Block (PEB) of the current process.
///
/// An instance can be created through LIEF::runtime::windows::Process::peb().
///
/// ```cpp
/// if (auto peb = LIEF::runtime::windows::Process::peb()) {
///   if (peb->being_debugged()) {
///     // A debugger is attached to the current process
///   }
/// }
/// ```
class LIEF_API PEB {
  public:
  friend class Process;

  PEB() = delete;
  PEB(const PEB&) = delete;
  PEB& operator=(const PEB&) = delete;

  PEB(PEB&&) noexcept;
  PEB& operator=(PEB&&) noexcept;

  /// Iterator over the LdrDataTableEntry referenced by the loader data.
  using entries_it = iterator_range<LdrDataTableEntry::Iterator>;

  /// Whether the current process is being debugged.
  bool being_debugged() const;

  /// Address of the loader data structure (`PEB_LDR_DATA`).
  uintptr_t ldr() const;

  /// Address of the process parameters (`RTL_USER_PROCESS_PARAMETERS`).
  uintptr_t process_parameters() const;

  /// Address of the per-process ATL thunk SList (single-linked list).
  uintptr_t atl_thunk_slist_ptr() const;

  /// 32-bit value of the ATL thunk SList pointer.
  uint32_t atl_thunk_slist_ptr32() const;

  /// Address of the routine called once the process completed its
  /// initialization (`PostProcessInitRoutine`).
  uintptr_t post_process_init_routine() const;

  /// Session ID associated with the current process.
  uint32_t session_id() const;

  /// Return a bidirectional iterator over the modules referenced by the
  /// loader data (`Ldr`).
  ///
  /// ```cpp
  /// if (auto peb = LIEF::runtime::windows::Process::peb()) {
  ///   for (const LdrDataTableEntry& entry : peb->entries()) {
  ///     // entry.base_dll_name(), entry.dll_base(), ...
  ///   }
  /// }
  /// ```
  entries_it entries() const;

  ~PEB();

  private:
  static std::unique_ptr<PEB> create();
  PEB(std::unique_ptr<details::peb> impl);
  std::unique_ptr<details::peb> impl_;
};

}
}
}
#endif
