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
#include "LIEF/runtime/windows/LdrDataTableEntry.hpp"
#include "LIEF/runtime/windows/PEB.hpp"
#include "LIEF/runtime/windows/Host.hpp"
#include "LIEF/optional.hpp"
#include "LIEF/utils.hpp"

#include "internal_utils.hpp"

#include <spdlog/fmt/fmt.h>

#include <cstddef>
#include <cstdint>
#include <sstream>

namespace LIEF::runtime::windows {
namespace details {
// Primitive-typed taken from winternl.h

// LIST_ENTRY
struct list_entry {
  uintptr_t Flink;
  uintptr_t Blink;
};

// UNICODE_STRING.
struct unicode_string {
  uint16_t Length;
  uint16_t MaximumLength;
  uintptr_t Buffer;
};

// PEB_LDR_DATA
struct peb_ldr_data {
  /// Size of the structure
  uint32_t Length;
  /// If the loader data section is initialized
  uint8_t Initialized;

  /// Handle to the subsystem
  uintptr_t SsHandle;

  /// Doubly linked list (Load order)
  list_entry InLoadOrderModuleList;

  /// Doubly linked list (Memory placement order)
  list_entry InMemoryOrderModuleList;

  /// Doubly linked list (Initialization order)
  list_entry InInitializationOrderModuleList;

  /// Pointer to module currently being loaded
  uintptr_t EntryInProgress;

  /// Indicates if process shutdown has started
  uintptr_t ShutdownInProgress;

  /// Thread ID performing the shutdown
  uintptr_t ShutdownThreadId;
};

// RTL_BALANCED_NODE: node of the red-black trees that the Windows 8+ loader
// uses to index modules by base address and by mapping. Three pointer-sized
// words on every architecture.
struct rtl_balanced_node {
  uintptr_t Left;
  uintptr_t Right;
  uintptr_t ParentValue;
};

// LDR_DATA_TABLE_ENTRY

// clang-format off
struct ldr_data_table_entry {
  list_entry InLoadOrderLinks;           // 0x00
  list_entry InMemoryOrderLinks;         // 0x10
  list_entry InInitializationOrderLinks; // 0x20
  uintptr_t DllBase;                     // 0x30
  uintptr_t EntryPoint;                  // 0x38
  uint32_t SizeOfImage;                  // 0x40
  unicode_string FullDllName;            // 0x48
  unicode_string BaseDllName;            // 0x58

  // struct {
  //     ULONG PackagedBinary : 1;
  //     ULONG MarkedForRemoval : 1;
  //     ULONG ImageDll : 1;
  //     ULONG LoadNotificationsSent : 1;
  //     ULONG TelemetryEntryProcessed : 1;
  //     ULONG ProcessStaticImport : 1;
  //     ULONG InLegacyLists : 1;
  //     ULONG InIndexes : 1;
  //     ULONG ShimDll : 1;
  //     ULONG InExceptionTable : 1;
  //     ULONG LoadInProgress : 1;
  //     ULONG LoadConfigProcessed : 1;
  //     ULONG EntryProcessed : 1;
  //     ULONG ProtectDelayLoad : 1;
  //     ULONG ReservedFlags3 : 2;
  //     ULONG DontCallForThreads : 1;
  //     ULONG ProcessAttachCalled : 1;
  //     ULONG ProcessAttachFailed : 1;
  //     ULONG CorDeferredValidate : 1;
  //     ULONG CorImage : 1;
  //     ULONG DontRelocate : 1;
  //     ULONG CorILOnly : 1;
  //     ULONG ChpeImage : 1;
  //     ULONG ChpeEmulatorImage : 1;
  //     ULONG ReservedFlags5 : 1;
  //     ULONG Redirected : 1;
  //     ULONG ReservedFlags6 : 2;
  //     ULONG CompatDatabaseProcessed : 1;
  // };
  // TODO(romain): we might want to expose this flag as the bitfield
  //               defined above.
  uint32_t Flags;                        // 0x68
  uint16_t ObsoleteLoadCount;            // 0x6c
  uint16_t TlsIndex;                     // 0x6e
  list_entry HashLinks;                  // 0x70
  uint32_t TimeDateStamp;                // 0x80
  uintptr_t EntryPointActivationContext; // 0x88
  uintptr_t Lock;                        // 0x90

  // Modern dependency graph and loading structures introduced in Windows 8.
  uintptr_t DdagNode;                    // 0x98
  list_entry NodeModuleLink;             // 0xa0
  uintptr_t LoadContext;                 // 0xb0
  uintptr_t ParentDllBase;               // 0xb8
  uintptr_t SwitchBackContext;           // 0xc0

  // Red-black trees for fast memory/mapping lookups.
  rtl_balanced_node BaseAddressIndexNode; // 0xc8
  rtl_balanced_node MappingInfoIndexNode; // 0xe0

  uintptr_t OriginalBase;                 // 0xf8
  int64_t LoadTime;                       // 0x100 (LARGE_INTEGER)
  uint32_t BaseNameHashValue;             // 0x108
  int32_t LoadReason;                     // 0x10c (LDR_DLL_LOAD_REASON)
  uint32_t ImplicitPathOptions;           // 0x110
  uint32_t ReferenceCount;                // 0x114
  uint32_t DependentLoadFlags;            // 0x118

  // Security and integrity (Windows 10+).
  uint8_t SigningLevel;                   // 0x11c
  uint32_t CheckSum;                      // 0x120

  // Hot-patching (Windows 11+).
  uintptr_t ActivePatchImageBase;         // 0x128
  uint32_t HotPatchState;                 // 0x130 (LDR_HOT_PATCH_STATE)
};
// clang-format on


inline constexpr bool is_x64 = sizeof(uintptr_t) == 8;
static_assert(offsetof(ldr_data_table_entry, InLoadOrderLinks) == 0);

static_assert(offsetof(peb_ldr_data, InLoadOrderModuleList) ==
                  (is_x64 ? 0x10 : 0x0C),
              "Invalid layout for InLoadOrderModuleList");

static_assert(!is_x64 || offsetof(ldr_data_table_entry, HotPatchState) == 0x130,
              "Invalid 64-bit layout for HotPatchState");

static_assert(is_x64 || offsetof(ldr_data_table_entry, BaseDllName) == 0x2C,
              "Invalid 32-bit layout for BaseDllName");

class ldr_entry {
  public:
  explicit ldr_entry(const ldr_data_table_entry* raw) :
    raw_(raw) {}

  const ldr_data_table_entry& raw() const {
    return *raw_;
  }

  private:
  const ldr_data_table_entry* raw_ = nullptr;
};

class ldr_entry_it {
  public:
  ldr_entry_it() = default;

  explicit ldr_entry_it(uintptr_t node) :
    node_(node) {}

  bool eq(const ldr_entry_it& other) const {
    return node_ == other.node_;
  }

  std::unique_ptr<ldr_entry_it> clone() const {
    return std::make_unique<ldr_entry_it>(*this);
  }

  void increment() {
    node_ = reinterpret_cast<const list_entry*>(node_)->Flink;
  }

  void decrement() {
    node_ = reinterpret_cast<const list_entry*>(node_)->Blink;
  }

  std::unique_ptr<LdrDataTableEntry> get() const {
    auto impl = std::make_unique<ldr_entry>(
        reinterpret_cast<const ldr_data_table_entry*>(node_)
    );
    return std::make_unique<LdrDataTableEntry>(std::move(impl));
  }

  private:
  uintptr_t node_ = 0;
};

}

inline std::string to_u8(const details::unicode_string& str) {
  if (str.Buffer == 0 || str.Length == 0) {
    return "";
  }
  return u16tou8(reinterpret_cast<const char16_t*>(str.Buffer),
                 str.Length / sizeof(char16_t), /*remove_null_char=*/true);
}


inline bool host_at_least(uint32_t major, uint32_t minor, uint32_t build) {
  return Host::version() >= Host::version_t(major, minor, build);
}

// Windows 8 (6.2)
inline bool has_win8_fields() {
  return host_at_least(6, 2, 9200);
}

// Windows 10 (10.0).
inline bool has_win10_fields() {
  return host_at_least(10, 0, 10240);
}

// Windows 11 (build 22000)
inline bool has_win11_fields() {
  return host_at_least(10, 0, 22000);
}

// ----------------------------------------------------------------------------
// LdrDataTableEntry
// ----------------------------------------------------------------------------
LdrDataTableEntry::LdrDataTableEntry(std::unique_ptr<details::ldr_entry> impl) :
  impl_(std::move(impl)) {}

LdrDataTableEntry::LdrDataTableEntry(LdrDataTableEntry&&) noexcept = default;
LdrDataTableEntry&
    LdrDataTableEntry::operator=(LdrDataTableEntry&&) noexcept = default;

LdrDataTableEntry::~LdrDataTableEntry() = default;

uintptr_t LdrDataTableEntry::dll_base() const {
  return impl_->raw().DllBase;
}

uintptr_t LdrDataTableEntry::entry_point() const {
  return impl_->raw().EntryPoint;
}

uint32_t LdrDataTableEntry::size_of_image() const {
  return impl_->raw().SizeOfImage;
}

std::string LdrDataTableEntry::full_dll_name() const {
  return to_u8(impl_->raw().FullDllName);
}

std::string LdrDataTableEntry::base_dll_name() const {
  return to_u8(impl_->raw().BaseDllName);
}

uint32_t LdrDataTableEntry::flags() const {
  return impl_->raw().Flags;
}

uint16_t LdrDataTableEntry::obsolete_load_count() const {
  return impl_->raw().ObsoleteLoadCount;
}

uint16_t LdrDataTableEntry::tls_index() const {
  return impl_->raw().TlsIndex;
}

uint32_t LdrDataTableEntry::time_date_stamp() const {
  return impl_->raw().TimeDateStamp;
}

uintptr_t LdrDataTableEntry::entry_point_activation_context() const {
  return impl_->raw().EntryPointActivationContext;
}

uintptr_t LdrDataTableEntry::lock() const {
  return impl_->raw().Lock;
}

optional<uintptr_t> LdrDataTableEntry::ddag_node() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().DdagNode;
}

optional<uintptr_t> LdrDataTableEntry::load_context() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().LoadContext;
}

optional<uintptr_t> LdrDataTableEntry::parent_dll_base() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().ParentDllBase;
}

optional<uintptr_t> LdrDataTableEntry::switch_back_context() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().SwitchBackContext;
}

optional<uintptr_t> LdrDataTableEntry::original_base() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().OriginalBase;
}

optional<int64_t> LdrDataTableEntry::load_time() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().LoadTime;
}

optional<uint32_t> LdrDataTableEntry::base_name_hash_value() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().BaseNameHashValue;
}

optional<int32_t> LdrDataTableEntry::load_reason() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().LoadReason;
}

optional<uint32_t> LdrDataTableEntry::implicit_path_options() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().ImplicitPathOptions;
}

optional<uint32_t> LdrDataTableEntry::reference_count() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().ReferenceCount;
}

optional<uint32_t> LdrDataTableEntry::dependent_load_flags() const {
  if (!has_win8_fields()) {
    return nullopt();
  }
  return impl_->raw().DependentLoadFlags;
}

optional<uint8_t> LdrDataTableEntry::signing_level() const {
  if (!has_win10_fields()) {
    return nullopt();
  }
  return impl_->raw().SigningLevel;
}

optional<uint32_t> LdrDataTableEntry::check_sum() const {
  if (!has_win10_fields()) {
    return nullopt();
  }
  return impl_->raw().CheckSum;
}

optional<uintptr_t> LdrDataTableEntry::active_patch_image_base() const {
  if (!has_win11_fields()) {
    return nullopt();
  }
  return impl_->raw().ActivePatchImageBase;
}

optional<uint32_t> LdrDataTableEntry::hot_patch_state() const {
  if (!has_win11_fields()) {
    return nullopt();
  }
  return impl_->raw().HotPatchState;
}

std::string LdrDataTableEntry::to_string() const {
  static constexpr auto WIDTH = 40;
  std::ostringstream oss;
  oss << fmt::format("{:{}} {:#018x}\n", "Base address (DllBase):", WIDTH,
                     dll_base())
      << fmt::format("{:{}} {:#018x}\n", "Entry point:", WIDTH, entry_point())
      << fmt::format("{:{}} {:#08x}\n", "Size of image:", WIDTH, size_of_image())
      << fmt::format("{:{}} {}\n", "Full DLL name:", WIDTH, full_dll_name())
      << fmt::format("{:{}} {}\n", "Base DLL name:", WIDTH, base_dll_name())
      << fmt::format("{:{}} {:#010x}\n", "Flags:", WIDTH, flags())
      << fmt::format("{:{}} {}\n", "Obsolete load count:", WIDTH,
                     obsolete_load_count())
      << fmt::format("{:{}} {}\n", "TLS index:", WIDTH, tls_index())
      << fmt::format("{:{}} {:#010x}\n", "Timestamp:", WIDTH, time_date_stamp())
      << fmt::format("{:{}} {:#018x}\n", "Entry point activation context:", WIDTH,
                     entry_point_activation_context())
      << fmt::format("{:{}} {:#018x}\n", "Lock:", WIDTH, lock());

  if (auto val = ddag_node()) {
    oss << fmt::format("{:{}} {:#018x}\n", "DDAG node:", WIDTH, *val);
  }

  if (auto val = load_context()) {
    oss << fmt::format("{:{}} {:#018x}\n", "Load context:", WIDTH, *val);
  }

  if (auto val = parent_dll_base()) {
    oss << fmt::format("{:{}} {:#018x}\n", "Parent DLL base:", WIDTH, *val);
  }

  if (auto val = switch_back_context()) {
    oss << fmt::format("{:{}} {:#018x}\n", "Switch-back context:", WIDTH, *val);
  }

  if (auto val = original_base()) {
    oss << fmt::format("{:{}} {:#018x}\n", "Original base:", WIDTH, *val);
  }

  if (auto val = load_time()) {
    oss << fmt::format("{:{}} {}\n", "Load time:", WIDTH, *val);
  }

  if (auto val = base_name_hash_value()) {
    oss << fmt::format("{:{}} {:#010x}\n", "Base name hash value:", WIDTH, *val);
  }

  if (auto val = load_reason()) {
    oss << fmt::format("{:{}} {}\n", "Load reason:", WIDTH, *val);
  }

  if (auto val = implicit_path_options()) {
    oss << fmt::format("{:{}} {:#010x}\n", "Implicit path options:", WIDTH, *val);
  }

  if (auto val = reference_count()) {
    oss << fmt::format("{:{}} {}\n", "Reference count:", WIDTH, *val);
  }

  if (auto val = dependent_load_flags()) {
    oss << fmt::format("{:{}} {:#010x}\n", "Dependent load flags:", WIDTH, *val);
  }

  if (auto val = signing_level()) {
    oss << fmt::format("{:{}} {}\n", "Signing level:", WIDTH, *val);
  }

  if (auto val = check_sum()) {
    oss << fmt::format("{:{}} {:#010x}\n", "Checksum:", WIDTH, *val);
  }

  if (auto val = active_patch_image_base()) {
    oss << fmt::format("{:{}} {:#018x}\n", "Active patch image base:", WIDTH,
                       *val);
  }

  if (auto val = hot_patch_state()) {
    oss << fmt::format("{:{}} {:#010x}\n", "Hot patch state:", WIDTH, *val);
  }

  return oss.str();
}

// ----------------------------------------------------------------------------
// LdrDataTableEntry::Iterator
// ----------------------------------------------------------------------------
LdrDataTableEntry::Iterator::Iterator() = default;

LdrDataTableEntry::Iterator::Iterator(
    std::unique_ptr<details::ldr_entry_it> impl
) :
  impl_(std::move(impl)) {}

LdrDataTableEntry::Iterator::Iterator(const Iterator& other) :
  impl_(other.impl_ ? other.impl_->clone() : nullptr) {}

LdrDataTableEntry::Iterator&
    LdrDataTableEntry::Iterator::operator=(const Iterator& other) {
  if (this != &other) {
    impl_ = other.impl_ ? other.impl_->clone() : nullptr;
    cached_.reset();
  }
  return *this;
}

LdrDataTableEntry::Iterator::Iterator(Iterator&&) noexcept = default;
LdrDataTableEntry::Iterator&
    LdrDataTableEntry::Iterator::operator=(Iterator&&) noexcept = default;

LdrDataTableEntry::Iterator::~Iterator() = default;

bool operator==(const LdrDataTableEntry::Iterator& LHS,
                const LdrDataTableEntry::Iterator& RHS) {
  return LHS.impl_->eq(*RHS.impl_);
}

LdrDataTableEntry::Iterator& LdrDataTableEntry::Iterator::operator++() {
  impl_->increment();
  cached_.reset();
  return *this;
}

LdrDataTableEntry::Iterator& LdrDataTableEntry::Iterator::operator--() {
  impl_->decrement();
  cached_.reset();
  return *this;
}

void LdrDataTableEntry::Iterator::load() const {
  if (cached_ == nullptr) {
    cached_ = impl_->get();
  }
}

const LdrDataTableEntry& LdrDataTableEntry::Iterator::operator*() const {
  load();
  return *cached_;
}

const LdrDataTableEntry* LdrDataTableEntry::Iterator::operator->() const {
  load();
  return cached_.get();
}

std::unique_ptr<LdrDataTableEntry> LdrDataTableEntry::Iterator::yield() {
  load();
  return std::move(cached_);
}

PEB::entries_it PEB::entries() const {
  const uintptr_t ldr_addr = ldr();
  if (ldr_addr == 0) {
    return make_empty_iterator<LdrDataTableEntry>();
  }

  const auto* ldr = reinterpret_cast<const details::peb_ldr_data*>(ldr_addr);

  // The list head lives inside PEB_LDR_DATA and acts as the past-the-end
  // sentinel of the circular list.
  const uintptr_t head =
      ldr_addr + offsetof(details::peb_ldr_data, InLoadOrderModuleList);

  auto begin = LdrDataTableEntry::Iterator(
      std::make_unique<details::ldr_entry_it>(ldr->InLoadOrderModuleList.Flink)
  );
  auto end =
      LdrDataTableEntry::Iterator(std::make_unique<details::ldr_entry_it>(head));

  return make_range(std::move(begin), std::move(end));
}
}
