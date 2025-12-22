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
#ifndef LIEF_PE_LOAD_CONFIGURATION_H
#define LIEF_PE_LOAD_CONFIGURATION_H
#include <ostream>
#include <cstdint>

#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/optional.hpp"
#include "LIEF/iterators.hpp"

#include "LIEF/PE/CodeIntegrity.hpp"

namespace LIEF {
class BinaryStream;

namespace PE {
class Parser;
class CHPEMetadata;
class DynamicRelocation;
class EnclaveConfiguration;
class VolatileMetadata;

/// This class represents the load configuration data associated with the
/// `IMAGE_LOAD_CONFIG_DIRECTORY`.
///
/// This structure is frequently updated by Microsoft to add new metadata.
///
/// Reference: https://github.com/MicrosoftDocs/sdk-api/blob/cbeab4d371e8bc7e352c4d3a4c5819caa08c6a1c/sdk-api-src/content/winnt/ns-winnt-image_load_config_directory64.md#L2
class LIEF_API LoadConfiguration : public Object {
  public:
  friend class Parser;

  enum class IMAGE_GUARD : uint32_t {
    NONE = 0x00000000,
    /// Module performs control flow integrity checks using system-supplied
    /// support.
    CF_INSTRUMENTED = 0x100,

    /// Module performs control flow and write integrity checks.
    CFW_INSTRUMENTED = 0x200,

    /// Module contains valid control flow target metadata.
    CF_FUNCTION_TABLE_PRESENT = 0x400,

    /// Module does not make use of the /GS security cookie.
    SECURITY_COOKIE_UNUSED = 0x800,

    /// Module supports read only delay load IAT.
    PROTECT_DELAYLOAD_IAT = 0x1000,

    /// Delayload import table in its own .didat section (with nothing else in it)
    /// that can be freely reprotected.
    DELAYLOAD_IAT_IN_ITS_OWN_SECTION = 0x2000,

    /// Module contains suppressed export information. This also infers that the
    /// address taken IAT table is also present in the load config.
    CF_EXPORT_SUPPRESSION_INFO_PRESENT = 0x4000,

    /// Module enables suppression of exports.
    CF_ENABLE_EXPORT_SUPPRESSION = 0x8000,

    /// Module contains longjmp target information.
    CF_LONGJUMP_TABLE_PRESENT = 0x10000,

    /// Module contains return flow instrumentation and metadata
    RF_INSTRUMENTED = 0x00020000,

    /// Module requests that the OS enable return flow protection
    RF_ENABLE = 0x00040000,

    /// Module requests that the OS enable return flow protection in strict mode
    RF_STRICT = 0x00080000,

    /// Module was built with retpoline support
    RETPOLINE_PRESENT = 0x00100000,

    /// Module contains EH continuation target information.
    EH_CONTINUATION_TABLE_PRESENT = 0x400000,

    /// Module was built with xfg (deprecated)
    XFG_ENABLED = 0x00800000,

    /// Module has CastGuard instrumentation present
    CASTGUARD_PRESENT = 0x01000000,

    /// Module has Guarded Memcpy instrumentation present
    MEMCPY_PRESENT = 0x02000000,
  };

  struct guard_function_t {
    uint32_t rva = 0;
    uint32_t extra = 0;
  };

  using guard_functions_t = std::vector<guard_function_t>;
  using it_guard_functions = ref_iterator<guard_functions_t&>;
  using it_const_guard_functions = const_ref_iterator<const guard_functions_t&>;

  using dynamic_relocations_t = std::vector<std::unique_ptr<DynamicRelocation>>;
  using it_dynamic_relocations_t = ref_iterator<dynamic_relocations_t&, DynamicRelocation*>;
  using it_const_dynamic_relocations_t = const_ref_iterator<const dynamic_relocations_t&, const DynamicRelocation*>;

  template<class PE_T>
  static LIEF_LOCAL
    std::unique_ptr<LoadConfiguration> parse(Parser& ctx, BinaryStream& stream);

  LoadConfiguration();

  LoadConfiguration& operator=(const LoadConfiguration&);
  LoadConfiguration(const LoadConfiguration&);

  LoadConfiguration(LoadConfiguration&&);
  LoadConfiguration& operator=(LoadConfiguration&&);

  /// Characteristics of the structure which is defined by its size
  uint32_t characteristics() const {
    return characteristics_;
  }

  /// Size of the current structure
  uint32_t size() const {
    return characteristics_;
  }

  /// The date and time stamp value
  uint32_t timedatestamp() const {
    return timedatestamp_;
  }

  /// Major version
  uint16_t major_version() const {
    return major_version_;
  }

  /// Minor version
  uint16_t minor_version() const {
    return minor_version_;
  }

  /// The global flags that control system behavior. For more information, see
  /// `Gflags.exe`.
  uint32_t global_flags_clear() const {
    return global_flags_clear_;
  }

  /// The global flags that control system behavior. For more information, see
  /// `Gflags.exe`.
  uint32_t global_flags_set() const {
    return global_flags_set_;
  }

  /// The critical section default time-out value.
  uint32_t critical_section_default_timeout() const {
    return critical_section_default_timeout_;
  }

  /// The size of the minimum block that must be freed before it is freed
  /// (de-committed), in bytes. This value is advisory.
  uint64_t decommit_free_block_threshold() const {
    return decommit_free_block_threshold_;
  }

  /// The size of the minimum total memory that must be freed in the process
  /// heap before it is freed (de-committed), in bytes. This value is advisory.
  uint64_t decommit_total_free_threshold() const {
    return decommit_total_free_threshold_;
  }

  /// The VA of a list of addresses where the `LOCK` prefix is used. These will
  /// be replaced by `NOP` on single-processor systems. This member is available
  /// only for x86.
  uint64_t lock_prefix_table() const {
    return lock_prefix_table_;
  }

  /// The maximum allocation size, in bytes. This member is obsolete and is
  /// used only for debugging purposes.
  uint64_t maximum_allocation_size() const {
    return maximum_allocation_size_;
  }

  /// The maximum block size that can be allocated from heap segments, in bytes.
  uint64_t virtual_memory_threshold() const {
    return virtual_memory_threshold_;
  }

  /// The process affinity mask. For more information, see
  /// `GetProcessAffinityMask`. This member is available only for `.exe` files.
  uint64_t process_affinity_mask() const {
    return process_affinity_mask_;
  }

  /// The process heap flags. For more information, see `HeapCreate`.
  uint32_t process_heap_flags() const {
    return process_heap_flags_;
  }

  /// The service pack version.
  uint16_t csd_version() const {
    return csd_version_;
  }

  /// See: dependent_load_flags()
  uint16_t reserved1() const {
    return reserved1_;
  }

  /// Alias for reserved1().
  ///
  /// The default load flags used when the operating system resolves the
  /// statically linked imports of a module. For more information, see
  /// `LoadLibraryEx`.
  uint16_t dependent_load_flags() const {
    return reserved1_;
  }

  /// Reserved for use by the system.
  uint64_t editlist() const {
    return editlist_;
  }

  /// A pointer to a cookie that is used by Visual C++ or GS implementation.
  uint64_t security_cookie() const {
    return security_cookie_;
  }

  /// The VA of the sorted table of RVAs of each valid, unique handler in the
  /// image. This member is available only for x86.
  optional<uint64_t> se_handler_table() const {
    return se_handler_table_;
  }

  /// The count of unique handlers in the table. This member is available only
  /// for x86.
  optional<uint64_t> se_handler_count() const {
    return se_handler_count_;
  }

  /// Return the list of the function RVA in the SEH table (if any)
  const std::vector<uint32_t>& seh_functions() const {
    return seh_rva_;
  }

  /// The VA where Control Flow Guard check-function pointer is stored.
  optional<uint64_t> guard_cf_check_function_pointer() const {
    return guard_cf_check_function_pointer_;
  }

  /// The VA where Control Flow Guard dispatch-function pointer is stored.
  optional<uint64_t> guard_cf_dispatch_function_pointer() const {
    return guard_cf_dispatch_function_pointer_;
  }

  /// The VA of the sorted table of RVAs of each Control Flow Guard function in
  /// the image.
  optional<uint64_t> guard_cf_function_table() const {
    return guard_cf_function_table_;
  }

  /// The count of unique RVAs in the guard_cf_function_table() table.
  optional<uint64_t> guard_cf_function_count() const {
    return guard_cf_function_count_;
  }

  /// Iterator over the Control Flow Guard functions referenced by
  /// guard_cf_function_table()
  it_const_guard_functions guard_cf_functions() const {
    return guard_cf_functions_;
  }

  it_guard_functions guard_cf_functions() {
    return guard_cf_functions_;
  }

  /// Control Flow Guard related flags.
  optional<uint32_t> guard_flags() const {
    return flags_;
  }

  /// Check if the given flag is present
  bool has(IMAGE_GUARD flag) const {
    if (!flags_) {
      return false;
    }
    return (*flags_ & (uint32_t)flag) != 0;
  }

  /// List of flags
  std::vector<IMAGE_GUARD> guard_cf_flags_list() const;

  /// Code integrity information.
  const CodeIntegrity* code_integrity() const {
    return code_integrity_ ? &*code_integrity_ : nullptr;
  }

  CodeIntegrity* code_integrity() {
    return code_integrity_ ? &*code_integrity_ : nullptr;
  }

  /// The VA where Control Flow Guard address taken IAT table is stored.
  optional<uint64_t> guard_address_taken_iat_entry_table() const {
    return guard_address_taken_iat_entry_table_;
  }

  /// The count of unique RVAs in the table pointed by
  /// guard_address_taken_iat_entry_table().
  optional<uint64_t> guard_address_taken_iat_entry_count() const {
    return guard_address_taken_iat_entry_count_;
  }

  /// List of RVA pointed by guard_address_taken_iat_entry_table()
  it_const_guard_functions guard_address_taken_iat_entries() const {
    return guard_address_taken_iat_entries_;
  }

  it_guard_functions guard_address_taken_iat_entries() {
    return guard_address_taken_iat_entries_;
  }

  /// The VA where Control Flow Guard long jump target table is stored.
  optional<uint64_t> guard_long_jump_target_table() const {
    return guard_long_jump_target_table_;
  }

  /// The count of unique RVAs in the table pointed by
  /// guard_long_jump_target_table.
  optional<uint64_t> guard_long_jump_target_count() const {
    return guard_long_jump_target_count_;
  }

  /// List of RVA pointed by guard_address_taken_iat_entry_table()
  it_const_guard_functions guard_long_jump_targets() const {
    return guard_long_jump_targets_;
  }

  it_guard_functions guard_long_jump_targets() {
    return guard_long_jump_targets_;
  }

  /// VA pointing to a `IMAGE_DYNAMIC_RELOCATION_TABLE`
  optional<uint64_t> dynamic_value_reloc_table() const {
    return dynamic_value_reloc_table_;
  }

  /// Return an iterator over the Dynamic relocations (DVRT)
  it_dynamic_relocations_t dynamic_relocations() {
    return dynamic_relocs_;
  }

  it_const_dynamic_relocations_t dynamic_relocations() const {
    return dynamic_relocs_;
  }

  /// Alias for chpe_metadata_pointer()
  optional<uint64_t> hybrid_metadata_pointer() const {
    return hybrid_metadata_pointer_;
  }

  /// VA to the extra Compiled Hybrid Portable Executable (CHPE) metadata.
  optional<uint64_t> chpe_metadata_pointer() const {
    return hybrid_metadata_pointer();
  }

  /// Compiled Hybrid Portable Executable (CHPE) metadata (if any)
  const CHPEMetadata* chpe_metadata() const {
    return chpe_.get();
  }

  CHPEMetadata* chpe_metadata() {
    return chpe_.get();
  }

  /// VA of the failure routine
  optional<uint64_t> guard_rf_failure_routine() const {
    return guard_rf_failure_routine_;
  }

  /// VA of the failure routine `fptr`.
  optional<uint64_t> guard_rf_failure_routine_function_pointer() const {
    return guard_rf_failure_routine_function_pointer_;
  }

  /// Offset of dynamic relocation table relative to the relocation table
  optional<uint32_t> dynamic_value_reloctable_offset() const {
    return dynamic_value_reloctable_offset_;
  }

  /// The section index of the dynamic value relocation table
  optional<uint16_t> dynamic_value_reloctable_section() const {
    return dynamic_value_reloctable_section_;
  }

  /// Must be zero
  optional<uint16_t> reserved2() const {
    return reserved2_;
  }

  /// VA of the Function verifying the stack pointer
  optional<uint64_t> guard_rf_verify_stackpointer_function_pointer() const {
    return guardrf_verify_stackpointer_function_pointer_;
  }

  /// Offset to the *hotpatch* table
  optional<uint32_t> hotpatch_table_offset() const {
    return hotpatch_table_offset_;
  }

  optional<uint32_t> reserved3() const {
    return reserved3_;
  }

  optional<uint64_t> enclave_configuration_ptr() const {
    return enclave_configuration_ptr_;
  }

  const EnclaveConfiguration* enclave_config() const {
    return enclave_config_.get();
  }

  EnclaveConfiguration* enclave_config() {
    return enclave_config_.get();
  }

  optional<uint64_t> volatile_metadata_pointer() const {
    return volatile_metadata_pointer_;
  }

  const VolatileMetadata* volatile_metadata() const {
    return volatile_metadata_.get();
  }

  VolatileMetadata* volatile_metadata() {
    return volatile_metadata_.get();
  }

  optional<uint64_t> guard_eh_continuation_table() const {
    return guard_eh_continuation_table_;
  }

  optional<uint64_t> guard_eh_continuation_count() const {
    return guard_eh_continuation_count_;
  }

  it_const_guard_functions guard_eh_continuation_functions() const {
    return guard_eh_continuation_functions_;
  }

  it_guard_functions guard_eh_continuation_functions() {
    return guard_eh_continuation_functions_;
  }

  optional<uint64_t> guard_xfg_check_function_pointer() const {
    return guard_xfg_check_function_pointer_;
  }

  optional<uint64_t> guard_xfg_dispatch_function_pointer() const {
    return guard_xfg_dispatch_function_pointer_;
  }

  optional<uint64_t> guard_xfg_table_dispatch_function_pointer() const {
    return guard_xfg_table_dispatch_function_pointer_;
  }

  optional<uint64_t> cast_guard_os_determined_failure_mode() const {
    return cast_guard_os_determined_failure_mode_;
  }

  optional<uint64_t> guard_memcpy_function_pointer() const {
    return guard_memcpy_function_pointer_;
  }

  optional<uint64_t> uma_function_pointers() const {
    return uma_function_pointers_;
  }

  LoadConfiguration& characteristics(uint32_t characteristics) {
    characteristics_ = characteristics;
    return *this;
  }

  LoadConfiguration& size(uint32_t value) {
    return characteristics(value);
  }

  LoadConfiguration& timedatestamp(uint32_t timedatestamp) {
    timedatestamp_ = timedatestamp;
    return *this;
  }

  LoadConfiguration& major_version(uint16_t major_version) {
    major_version_ = major_version;
    return *this;
  }

  LoadConfiguration& minor_version(uint16_t minor_version) {
    minor_version_ = minor_version;
    return *this;
  }

  LoadConfiguration& global_flags_clear(uint32_t global_flags_clear) {
    global_flags_clear_ = global_flags_clear;
    return *this;
  }

  LoadConfiguration& global_flags_set(uint32_t global_flags_set) {
    global_flags_set_ = global_flags_set;
    return *this;
  }

  LoadConfiguration& critical_section_default_timeout(uint32_t critical_section_default_timeout) {
    critical_section_default_timeout_ = critical_section_default_timeout;
    return *this;
  }

  LoadConfiguration& decommit_free_block_threshold(uint64_t decommit_free_block_threshold) {
    decommit_free_block_threshold_ = decommit_free_block_threshold;
    return *this;
  }

  LoadConfiguration& decommit_total_free_threshold(uint64_t decommit_total_free_threshold) {
    decommit_total_free_threshold_ = decommit_total_free_threshold;
    return *this;
  }

  LoadConfiguration& lock_prefix_table(uint64_t lock_prefix_table) {
    lock_prefix_table_ = lock_prefix_table;
    return *this;
  }

  LoadConfiguration& maximum_allocation_size(uint64_t maximum_allocation_size) {
    maximum_allocation_size_ = maximum_allocation_size;
    return *this;
  }

  LoadConfiguration& virtual_memory_threshold(uint64_t virtual_memory_threshold) {
    virtual_memory_threshold_ = virtual_memory_threshold;
    return *this;
  }

  LoadConfiguration& process_affinity_mask(uint64_t process_affinity_mask) {
    process_affinity_mask_ = process_affinity_mask;
    return *this;
  }

  LoadConfiguration& process_heap_flags(uint32_t process_heap_flagsid) {
    process_heap_flags_ = process_heap_flagsid;
    return *this;
  }

  LoadConfiguration& csd_version(uint16_t csd_version) {
    csd_version_ = csd_version;
    return *this;
  }

  LoadConfiguration& reserved1(uint16_t reserved1) {
    reserved1_ = reserved1;
    return *this;
  }

  LoadConfiguration& dependent_load_flags(uint16_t flags) {
    reserved1(flags);
    return *this;
  }

  LoadConfiguration& editlist(uint64_t editlist) {
    editlist_ = editlist;
    return *this;
  }

  LoadConfiguration& security_cookie(uint64_t security_cookie) {
    security_cookie_ = security_cookie;
    return *this;
  }

  LoadConfiguration& se_handler_table(uint64_t se_handler_table) {
    se_handler_table_ = se_handler_table;
    return *this;
  }

  LoadConfiguration& se_handler_count(uint64_t se_handler_count) {
    se_handler_count_ = se_handler_count;
    return *this;
  }

  LoadConfiguration& guard_cf_check_function_pointer(uint64_t check_pointer) {
    guard_cf_check_function_pointer_ = check_pointer;
    return *this;
  }

  LoadConfiguration& guard_cf_dispatch_function_pointer(uint64_t dispatch_pointer) {
    guard_cf_dispatch_function_pointer_ = dispatch_pointer;
    return *this;
  }

  LoadConfiguration& guard_cf_function_table(uint64_t guard_cf_function_table) {
    guard_cf_function_table_ = guard_cf_function_table;
    return *this;
  }

  LoadConfiguration& guard_cf_function_count(uint64_t guard_cf_function_count) {
    guard_cf_function_count_ = guard_cf_function_count;
    return *this;
  }

  LoadConfiguration& guard_flags(IMAGE_GUARD flags) {
    flags_ = (uint32_t)flags;
    return *this;
  }

  LoadConfiguration& guard_flags(uint32_t flags) {
    flags_ = flags;
    return *this;
  }

  LoadConfiguration& code_integrity(CodeIntegrity CI) {
    code_integrity_ = std::move(CI);
    return *this;
  }

  LoadConfiguration& guard_address_taken_iat_entry_table(uint64_t value) {
    guard_address_taken_iat_entry_table_ = value;
    return *this;
  }

  LoadConfiguration& guard_address_taken_iat_entry_count(uint64_t value) {
    guard_address_taken_iat_entry_count_ = value;
    return *this;
  }

  LoadConfiguration& guard_long_jump_target_table(uint64_t value) {
    guard_long_jump_target_table_ = value;
    return *this;
  }

  LoadConfiguration& guard_long_jump_target_count(uint64_t value) {
    guard_long_jump_target_count_ = value;
    return *this;
  }

  LoadConfiguration& dynamic_value_reloc_table(uint64_t value) {
    dynamic_value_reloc_table_ = value;
    return *this;
  }

  LoadConfiguration& hybrid_metadata_pointer(uint64_t value) {
    hybrid_metadata_pointer_ = value;
    return *this;
  }

  LoadConfiguration& guard_rf_failure_routine(uint64_t value) {
    guard_rf_failure_routine_ = value;
    return *this;
  }

  LoadConfiguration& guard_rf_failure_routine_function_pointer(uint64_t value) {
    guard_rf_failure_routine_function_pointer_ = value;
    return *this;
  }

  LoadConfiguration& dynamic_value_reloctable_offset(uint32_t value) {
    dynamic_value_reloctable_offset_ = value;
    return *this;
  }

  LoadConfiguration& dynamic_value_reloctable_section(uint16_t value) {
    dynamic_value_reloctable_section_ = value;
    return *this;
  }

  LoadConfiguration& reserved2(uint16_t value) {
    reserved2_ = value;
    return *this;
  }

  LoadConfiguration& guard_rf_verify_stackpointer_function_pointer(uint64_t value) {
    guardrf_verify_stackpointer_function_pointer_ = value;
    return *this;
  }

  LoadConfiguration& hotpatch_table_offset(uint32_t value) {
    hotpatch_table_offset_ = value;
    return *this;
  }

  LoadConfiguration& reserved3(uint32_t value) {
    reserved3_ = value;
    return *this;
  }

  LoadConfiguration& enclave_configuration_ptr(uint64_t value) {
    enclave_configuration_ptr_ = value;
    return *this;
  }

  LoadConfiguration& volatile_metadata_pointer(uint64_t value) {
    volatile_metadata_pointer_ = value;
    return *this;
  }

  LoadConfiguration& guard_eh_continuation_table(uint64_t value) {
    guard_eh_continuation_table_ = value;
    return *this;
  }

  LoadConfiguration& guard_eh_continuation_count(uint64_t value) {
    guard_eh_continuation_count_ = value;
    return *this;
  }

  LoadConfiguration& guard_xfg_check_function_pointer(uint64_t value) {
    guard_xfg_check_function_pointer_ = value;
    return *this;
  }

  LoadConfiguration& guard_xfg_dispatch_function_pointer(uint64_t value) {
    guard_xfg_dispatch_function_pointer_ = value;
    return *this;
  }

  LoadConfiguration& guard_xfg_table_dispatch_function_pointer(uint64_t value) {
    guard_xfg_table_dispatch_function_pointer_ = value;
    return *this;
  }

  LoadConfiguration& cast_guard_os_determined_failure_mode(uint64_t value) {
    cast_guard_os_determined_failure_mode_ = value;
    return *this;
  }

  LoadConfiguration& guard_memcpy_function_pointer(uint64_t value) {
    guard_memcpy_function_pointer_ = value;
    return *this;
  }

  LoadConfiguration& uma_function_pointers(uint64_t value) {
    uma_function_pointers_ = value;
    return *this;
  }

  ~LoadConfiguration() override;

  void accept(Visitor& visitor) const override;

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const LoadConfiguration& config)
  {
    os << config.to_string();
    return os;
  }

  /// \private
  LIEF_LOCAL static ok_error_t parse_seh_table(
    Parser& ctx, BinaryStream& stream, LoadConfiguration& config);

  /// \private
  LIEF_LOCAL static ok_error_t parse_guard_functions(
    Parser& ctx, BinaryStream& stream, LoadConfiguration& config,
    size_t count, guard_functions_t LoadConfiguration::* dst);

  /// \private
  LIEF_LOCAL static ok_error_t parse_dyn_relocs(
    Parser& ctx, LoadConfiguration& config);

  /// \private
  template<uint8_t version, class PE_T>
  LIEF_LOCAL static ok_error_t parse_dyn_relocs_entries(
    Parser& ctx, BinaryStream& stream, LoadConfiguration& config,
    size_t size);

  /// \private
  template<class PE_T>
  LIEF_LOCAL static ok_error_t parse_enclave_config(
    Parser& ctx, LoadConfiguration& config);

  /// \private
  LIEF_LOCAL static ok_error_t parse_volatile_metadata(
    Parser& ctx, LoadConfiguration& config);

  protected:
  uint32_t characteristics_ = 0;
  uint32_t timedatestamp_ = 0;

  uint16_t major_version_ = 0;
  uint16_t minor_version_ = 0;

  uint32_t global_flags_clear_ = 0;
  uint32_t global_flags_set_ = 0;

  uint32_t critical_section_default_timeout_ = 0;

  uint64_t decommit_free_block_threshold_ = 0;
  uint64_t decommit_total_free_threshold_ = 0;

  uint64_t lock_prefix_table_ = 0;
  uint64_t maximum_allocation_size_ = 0;
  uint64_t virtual_memory_threshold_ = 0;
  uint64_t process_affinity_mask_ = 0;
  uint32_t process_heap_flags_ = 0;
  uint16_t csd_version_ = 0;
  uint16_t reserved1_ = 0;  // named DependentLoadFlags in recent headers
  uint64_t editlist_ = 0;
  uint64_t security_cookie_ = 0;

  optional<uint64_t> se_handler_table_;
  optional<uint64_t> se_handler_count_;

  optional<uint64_t> guard_cf_check_function_pointer_;
  optional<uint64_t> guard_cf_dispatch_function_pointer_;
  optional<uint64_t> guard_cf_function_table_;
  optional<uint64_t> guard_cf_function_count_;
  optional<uint32_t> flags_;

  optional<CodeIntegrity> code_integrity_;

  optional<uint64_t> guard_address_taken_iat_entry_table_;
  optional<uint64_t> guard_address_taken_iat_entry_count_;
  optional<uint64_t> guard_long_jump_target_table_;
  optional<uint64_t> guard_long_jump_target_count_;

  optional<uint64_t> dynamic_value_reloc_table_;
  optional<uint64_t> hybrid_metadata_pointer_;

  optional<uint64_t> guard_rf_failure_routine_;
  optional<uint64_t> guard_rf_failure_routine_function_pointer_;
  optional<uint32_t> dynamic_value_reloctable_offset_;
  optional<uint16_t> dynamic_value_reloctable_section_;
  optional<uint16_t> reserved2_;

  optional<uint64_t> guardrf_verify_stackpointer_function_pointer_;
  optional<uint32_t> hotpatch_table_offset_;

  optional<uint32_t> reserved3_;
  optional<uint64_t> enclave_configuration_ptr_;

  optional<uint64_t> volatile_metadata_pointer_;

  optional<uint64_t> guard_eh_continuation_table_;
  optional<uint64_t> guard_eh_continuation_count_;

  optional<uint64_t> guard_xfg_check_function_pointer_;
  optional<uint64_t> guard_xfg_dispatch_function_pointer_;
  optional<uint64_t> guard_xfg_table_dispatch_function_pointer_;

  optional<uint64_t> cast_guard_os_determined_failure_mode_;

  optional<uint64_t> guard_memcpy_function_pointer_;

  optional<uint64_t> uma_function_pointers_;

  std::unique_ptr<CHPEMetadata> chpe_;
  std::vector<uint32_t> seh_rva_;
  guard_functions_t guard_cf_functions_;
  guard_functions_t guard_address_taken_iat_entries_;
  guard_functions_t guard_long_jump_targets_;
  guard_functions_t guard_eh_continuation_functions_;
  dynamic_relocations_t dynamic_relocs_;
  std::unique_ptr<EnclaveConfiguration> enclave_config_;
  std::unique_ptr<VolatileMetadata> volatile_metadata_;
};

LIEF_API const char* to_string(LoadConfiguration::IMAGE_GUARD e);

}
}

#endif
