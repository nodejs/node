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
#include <sstream>

#include <spdlog/fmt/fmt.h>

#include "LIEF/Visitor.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/LoadConfigurations/LoadConfiguration.hpp"
#include "LIEF/PE/LoadConfigurations/VolatileMetadata.hpp"
#include "LIEF/PE/LoadConfigurations/EnclaveConfiguration.hpp"
#include "LIEF/PE/LoadConfigurations/DynamicRelocation/DynamicRelocationV1.hpp"
#include "LIEF/PE/LoadConfigurations/CHPEMetadata/Metadata.hpp"

#include "PE/Structures.hpp"

#include "overload_cast.hpp"
#include "logging.hpp"
#include "frozen.hpp"
#include "internal_utils.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::PE::LoadConfiguration::IMAGE_GUARD, LIEF::PE::to_string);

namespace LIEF {
namespace PE {

LoadConfiguration::LoadConfiguration() = default;
LoadConfiguration::LoadConfiguration(LoadConfiguration&&) = default;
LoadConfiguration& LoadConfiguration::operator=(LoadConfiguration&&) = default;
LoadConfiguration::~LoadConfiguration() = default;

static constexpr std::array IMAGE_GUARD_LIST = {
  LoadConfiguration::IMAGE_GUARD::NONE,
  LoadConfiguration::IMAGE_GUARD::CF_INSTRUMENTED,
  LoadConfiguration::IMAGE_GUARD::CFW_INSTRUMENTED,
  LoadConfiguration::IMAGE_GUARD::CF_FUNCTION_TABLE_PRESENT,
  LoadConfiguration::IMAGE_GUARD::SECURITY_COOKIE_UNUSED,
  LoadConfiguration::IMAGE_GUARD::PROTECT_DELAYLOAD_IAT,
  LoadConfiguration::IMAGE_GUARD::DELAYLOAD_IAT_IN_ITS_OWN_SECTION,
  LoadConfiguration::IMAGE_GUARD::CF_EXPORT_SUPPRESSION_INFO_PRESENT,
  LoadConfiguration::IMAGE_GUARD::CF_ENABLE_EXPORT_SUPPRESSION,
  LoadConfiguration::IMAGE_GUARD::CF_LONGJUMP_TABLE_PRESENT,
  LoadConfiguration::IMAGE_GUARD::RF_INSTRUMENTED,
  LoadConfiguration::IMAGE_GUARD::RF_ENABLE,
  LoadConfiguration::IMAGE_GUARD::RF_STRICT,
  LoadConfiguration::IMAGE_GUARD::RETPOLINE_PRESENT,
  LoadConfiguration::IMAGE_GUARD::EH_CONTINUATION_TABLE_PRESENT,
};

inline bool should_quit(uint64_t start, BinaryStream& stream, uint32_t size) {
  return (stream.pos() - start) >= size;
}

template<class T, class Func>
inline bool read_lc(Func f, LoadConfiguration* thiz, BinaryStream& stream,
                    uint32_t start, uint32_t size)
{
  auto value = stream.read<T>();
  if (!value) {
    return false;
  }

  std::mem_fn(f)(thiz, *value);

  return !should_quit(start, stream, size);
}

template<class PE_T>
std::unique_ptr<LoadConfiguration>
  LoadConfiguration::parse(Parser& ctx, BinaryStream& stream)
{
  using ptr_t = typename PE_T::uint;

  const uint64_t start_off = stream.pos();

  auto Characteristics = stream.read<uint32_t>();
  if (!Characteristics) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto TimeDateStamp = stream.read<uint32_t>();
  if (!TimeDateStamp) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto MajorVersion = stream.read<uint16_t>();
  if (!MajorVersion) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto MinorVersion = stream.read<uint16_t>();
  if (!MinorVersion) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto GlobalFlagsClear = stream.read<uint32_t>();
  if (!GlobalFlagsClear) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto GlobalFlagsSet = stream.read<uint32_t>();
  if (!GlobalFlagsSet) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto CriticalSectionDefaultTimeout = stream.read<uint32_t>();
  if (!CriticalSectionDefaultTimeout) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto DeCommitFreeBlockThreshold = stream.read<ptr_t>();
  if (!DeCommitFreeBlockThreshold) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto DeCommitTotalFreeThreshold = stream.read<ptr_t>();
  if (!DeCommitTotalFreeThreshold) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto LockPrefixTable = stream.read<ptr_t>();
  if (!LockPrefixTable) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto MaximumAllocationSize = stream.read<ptr_t>();
  if (!MaximumAllocationSize) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto VirtualMemoryThreshold = stream.read<ptr_t>();
  if (!VirtualMemoryThreshold) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto ProcessAffinityMask = stream.read<ptr_t>();
  if (!ProcessAffinityMask) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto ProcessHeapFlags = stream.read<uint32_t>();
  if (!ProcessHeapFlags) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto CSDVersion = stream.read<uint16_t>();
  if (!CSDVersion) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto DependentLoadFlags = stream.read<uint16_t>();
  if (!DependentLoadFlags) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto EditList = stream.read<ptr_t>();
  if (!EditList) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto SecurityCookie = stream.read<ptr_t>();
  if (!SecurityCookie) {
    LIEF_DEBUG("LoadConfiguration error (line={})", __LINE__);
    return nullptr;
  }

  auto config = std::make_unique<LoadConfiguration>();

  (*config)
    .characteristics(*Characteristics)
    .timedatestamp(*TimeDateStamp)
    .major_version(*MajorVersion)
    .minor_version(*MinorVersion)
    .global_flags_clear(*GlobalFlagsClear)
    .global_flags_set(*GlobalFlagsSet)
    .critical_section_default_timeout(*CriticalSectionDefaultTimeout)
    .decommit_free_block_threshold(*DeCommitFreeBlockThreshold)
    .decommit_total_free_threshold(*DeCommitTotalFreeThreshold)
    .lock_prefix_table(*LockPrefixTable)
    .maximum_allocation_size(*MaximumAllocationSize)
    .virtual_memory_threshold(*VirtualMemoryThreshold)
    .process_affinity_mask(*ProcessAffinityMask)
    .process_heap_flags(*ProcessHeapFlags)
    .csd_version(*CSDVersion)
    .dependent_load_flags(*DependentLoadFlags)
    .editlist(*EditList)
    .security_cookie(*SecurityCookie)
  ;

  if (should_quit(start_off, stream, *Characteristics)) {
    return config;
  }

  /* V0 */ {
    if (!read_lc<ptr_t>(overload_cast<uint64_t>(&LoadConfiguration::se_handler_table),
                        config.get(), stream, start_off, *Characteristics))
    {
      return config;
    }

    if (!read_lc<ptr_t>(overload_cast<uint64_t>(&LoadConfiguration::se_handler_count),
                        config.get(), stream, start_off, *Characteristics))
    {
      return config;
    }
  }

  /* V1 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_cf_check_function_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_cf_dispatch_function_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_cf_function_table),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_cf_function_count),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<uint32_t>(
          overload_cast<uint32_t>(&LoadConfiguration::guard_flags),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V2 */ {
    auto code_integrity = CodeIntegrity::parse(ctx, stream);
    if (!code_integrity) {
      return config;
    }

    config->code_integrity(std::move(*code_integrity));

    if (should_quit(start_off, stream, *Characteristics)) {
      return config;
    }
  }

  /* V3 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_address_taken_iat_entry_table),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_address_taken_iat_entry_count),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_long_jump_target_table),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_long_jump_target_count),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V4 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::dynamic_value_reloc_table),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::hybrid_metadata_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V5 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_rf_failure_routine),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_rf_failure_routine_function_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<uint32_t>(
          overload_cast<uint32_t>(&LoadConfiguration::dynamic_value_reloctable_offset),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<uint16_t>(
          overload_cast<uint16_t>(&LoadConfiguration::dynamic_value_reloctable_section),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<uint16_t>(
          overload_cast<uint16_t>(&LoadConfiguration::reserved2),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V6 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_rf_verify_stackpointer_function_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<uint32_t>(
          overload_cast<uint32_t>(&LoadConfiguration::hotpatch_table_offset),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V7 */ {
    if (!read_lc<uint32_t>(
          overload_cast<uint32_t>(&LoadConfiguration::reserved3),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::enclave_configuration_ptr),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V8 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::volatile_metadata_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V9 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_eh_continuation_table),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_eh_continuation_count),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V10 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_xfg_check_function_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_xfg_dispatch_function_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }

    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_xfg_table_dispatch_function_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V11 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::cast_guard_os_determined_failure_mode),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V12 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::guard_memcpy_function_pointer),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }

  /* V13 */ {
    if (!read_lc<ptr_t>(
          overload_cast<uint64_t>(&LoadConfiguration::uma_function_pointers),
          config.get(), stream, start_off, *Characteristics)
    ) {
      return config;
    }
  }
  return config;
}

ok_error_t LoadConfiguration::parse_seh_table(
    Parser& /*ctx*/, BinaryStream& stream, LoadConfiguration& config)
{
  static constexpr auto MAX_RESERVE = 1000;
  const uint64_t count = config.se_handler_count().value_or(0);
  if (count == 0) {
    return ok();
  }

  LIEF_DEBUG("SEH Table parsing (#{})", count);

  config.seh_rva_.reserve(std::min<uint64_t>(MAX_RESERVE, count));
  for (size_t i = 0; i < count; ++i) {
    auto rva = stream.read<uint32_t>();
    if (!rva) {
      return make_error_code(rva.error());
    }

    config.seh_rva_.push_back(*rva);
  }

  return ok();
}

ok_error_t LoadConfiguration::parse_guard_functions(
    Parser& /*ctx*/, BinaryStream& stream, LoadConfiguration& config,
    size_t count, guard_functions_t LoadConfiguration::* dst)
{
  static constexpr auto MAX_RESERVE = 1000;
  if (count == 0) {
    return ok();
  }

  const uint32_t stride = config.guard_flags().value_or(0) >> 28;

  LIEF_DEBUG("Parsing GF (#{}, stride={})", count, stride);

  (config.*dst).reserve(std::min<uint64_t>(MAX_RESERVE, count));

  bool once_stride = false;
  for (size_t i = 0; i < count; ++i) {
    auto rva = stream.read<uint32_t>();
    if (!rva) {
      LIEF_DEBUG("Can't read RVA index {} ({}:{})", i,
                 __FUNCTION__, __LINE__);
      return make_error_code(rva.error());
    }

    uint32_t extra = 0;

    if (stride == 1) {
      auto value = stream.read<uint8_t>();
      if (!value) {
        LIEF_WARN("Can't read stride for RVA: {:#x}", *rva);
        return make_error_code(value.error());
      }
      extra = *value;
    }

    (config.*dst).push_back({*rva, extra});

    if (stride > 1) {
      if (!once_stride) {
        LIEF_WARN("Stride '{}' is not supported", stride);
        once_stride = true;
      }
      stream.increment_pos(stride);
    }
  }

  return ok();
}

ok_error_t LoadConfiguration::parse_dyn_relocs(Parser& ctx,
                                               LoadConfiguration& lconf)
{
  auto dyn_reloc_off = lconf.dynamic_value_reloctable_offset();
  uint16_t dyn_reloc_sec = lconf.dynamic_value_reloctable_section().value_or(0);

  if (dyn_reloc_sec < 1) {
    return ok();
  }

  if (!dyn_reloc_off) {
    return make_error_code(lief_errors::corrupted);
  }

  const uint16_t sec_idx = dyn_reloc_sec - 1;
  auto sections = ctx.bin().sections();
  if (sec_idx >= sections.size()) {
    return make_error_code(lief_errors::corrupted);
  }
  Section& sec = sections[sec_idx];

  span<const uint8_t> content = sec.content();
  if (content.empty()) {
    return make_error_code(lief_errors::corrupted);
  }

  if (*dyn_reloc_off >= content.size()) {
    return make_error_code(lief_errors::corrupted);
  }

  span<const uint8_t> payload = content.subspan(*dyn_reloc_off);
  SpanStream strm(payload);

  auto version = strm.read<uint32_t>();
  auto size = strm.read<uint32_t>();

  if (!version || !size) {
    return make_error_code(lief_errors::corrupted);
  }

  LIEF_DEBUG("Dynamic relocations. arch={}, version={}, size={:#x}",
             PE::to_string(ctx.bin().header().machine()), *version, *size);
  const bool supported_version = *version == 1 || *version == 2;
  if (!supported_version) {
    LIEF_WARN("Dynamic relocations version {} is not supported", *version);
    return make_error_code(lief_errors::not_supported);
  }

  if (ctx.bin().type() == PE_TYPE::PE32) {
    return *version == 1 ?
            parse_dyn_relocs_entries</*version=*/1, details::PE32>(ctx, strm,
                                                                   lconf, *size) :
            parse_dyn_relocs_entries</*version=*/2, details::PE32>(ctx, strm,
                                                                   lconf, *size);
  }
  assert(ctx.bin().type() == PE_TYPE::PE32_PLUS);

  return *version == 1 ?
          parse_dyn_relocs_entries</*version=*/1, details::PE64>(ctx, strm,
                                                                 lconf, *size) :
          parse_dyn_relocs_entries</*version=*/2, details::PE64>(ctx, strm,
                                                                 lconf, *size);
}


template<uint8_t version, class PE_T>
ok_error_t LoadConfiguration::parse_dyn_relocs_entries(
  Parser& ctx, BinaryStream& stream, LoadConfiguration& config, size_t size)
{
  const size_t stream_start = stream.pos();
  while (stream && stream.pos() < (stream_start + size)) {
    if constexpr (version == 1) {
      auto v1 = DynamicRelocationV1::parse<PE_T>(ctx, stream);
      if (v1 == nullptr) {
        LIEF_WARN("Dynamic (v1) relocation parsing failed at offset=0x{:04x}",
                  stream.pos());
        break;
      }
      config.dynamic_relocs_.push_back(std::move(v1));
      continue;
    }

    if constexpr (version == 2) {
      continue;
    }
  }
  return ok();
}

template<class PE_T>
ok_error_t LoadConfiguration::parse_enclave_config(
  Parser& ctx, LoadConfiguration& config)
{
  const uint64_t enclave_config_ptr = config.enclave_configuration_ptr().value_or(0);
  if (enclave_config_ptr == 0) {
    return ok();
  }

  const uint64_t imagebase = ctx.bin().optional_header().imagebase();

  if (enclave_config_ptr < imagebase) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::corrupted);
  }

  uint64_t offset = ctx.bin().va_to_offset(enclave_config_ptr);

  ScopedStream stream(ctx.stream(), offset);
  auto enclave_config = EnclaveConfiguration::parse<PE_T>(ctx, *stream);
  if (enclave_config == nullptr) {
    return make_error_code(lief_errors::read_error);
  }

  config.enclave_config_ = std::move(enclave_config);
  return ok();
}


ok_error_t LoadConfiguration::parse_volatile_metadata(
  Parser& ctx, LoadConfiguration& config)
{
  const uint64_t volatile_metadata = config.volatile_metadata_pointer().value_or(0);
  if (volatile_metadata == 0) {
    return ok();
  }

  const uint64_t imagebase = ctx.bin().optional_header().imagebase();

  if (volatile_metadata < imagebase) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return make_error_code(lief_errors::corrupted);
  }

  uint64_t offset = ctx.bin().va_to_offset(volatile_metadata);

  ScopedStream stream(ctx.stream(), offset);
  auto metadata = VolatileMetadata::parse(ctx, *stream);
  if (metadata == nullptr) {
    return make_error_code(lief_errors::read_error);
  }

  config.volatile_metadata_ = std::move(metadata);
  return ok();
}

LoadConfiguration::LoadConfiguration(const LoadConfiguration& other) :
  Object(other),
  characteristics_(other.characteristics_),
  timedatestamp_(other.timedatestamp_),
  major_version_(other.major_version_),
  minor_version_(other.minor_version_),
  global_flags_clear_(other.global_flags_clear_),
  global_flags_set_(other.global_flags_set_),
  critical_section_default_timeout_(other.critical_section_default_timeout_),
  decommit_free_block_threshold_(other.decommit_free_block_threshold_),
  decommit_total_free_threshold_(other.decommit_total_free_threshold_),
  lock_prefix_table_(other.lock_prefix_table_),
  maximum_allocation_size_(other.maximum_allocation_size_),
  virtual_memory_threshold_(other.virtual_memory_threshold_),
  process_affinity_mask_(other.process_affinity_mask_),
  process_heap_flags_(other.process_heap_flags_),
  csd_version_(other.csd_version_),
  reserved1_(other.reserved1_),
  editlist_(other.editlist_),
  security_cookie_(other.security_cookie_),
  se_handler_table_(other.se_handler_table_),
  se_handler_count_(other.se_handler_count_),
  guard_cf_check_function_pointer_(other.guard_cf_check_function_pointer_),
  guard_cf_dispatch_function_pointer_(other.guard_cf_dispatch_function_pointer_),
  guard_cf_function_table_(other.guard_cf_function_table_),
  guard_cf_function_count_(other.guard_cf_function_count_),
  flags_(other.flags_),
  code_integrity_(other.code_integrity_),
  guard_address_taken_iat_entry_table_(other.guard_address_taken_iat_entry_table_),
  guard_address_taken_iat_entry_count_(other.guard_address_taken_iat_entry_count_),
  guard_long_jump_target_table_(other.guard_long_jump_target_table_),
  guard_long_jump_target_count_(other.guard_long_jump_target_count_),
  dynamic_value_reloc_table_(other.dynamic_value_reloc_table_),
  hybrid_metadata_pointer_(other.hybrid_metadata_pointer_),
  guard_rf_failure_routine_(other.guard_rf_failure_routine_),
  guard_rf_failure_routine_function_pointer_(other.guard_rf_failure_routine_function_pointer_),
  dynamic_value_reloctable_offset_(other.dynamic_value_reloctable_offset_),
  dynamic_value_reloctable_section_(other.dynamic_value_reloctable_section_),
  reserved2_(other.reserved2_),
  guardrf_verify_stackpointer_function_pointer_(other.guardrf_verify_stackpointer_function_pointer_),
  hotpatch_table_offset_(other.hotpatch_table_offset_),
  reserved3_(other.reserved3_),
  enclave_configuration_ptr_(other.enclave_configuration_ptr_),
  volatile_metadata_pointer_(other.volatile_metadata_pointer_),
  guard_eh_continuation_table_(other.guard_eh_continuation_table_),
  guard_eh_continuation_count_(other.guard_eh_continuation_count_),
  guard_xfg_check_function_pointer_(other.guard_xfg_check_function_pointer_),
  guard_xfg_dispatch_function_pointer_(other.guard_xfg_dispatch_function_pointer_),
  guard_xfg_table_dispatch_function_pointer_(other.guard_xfg_table_dispatch_function_pointer_),
  cast_guard_os_determined_failure_mode_(other.cast_guard_os_determined_failure_mode_),
  guard_memcpy_function_pointer_(other.guard_memcpy_function_pointer_),
  uma_function_pointers_(other.uma_function_pointers_),

  seh_rva_(other.seh_rva_),
  guard_cf_functions_(other.guard_cf_functions_),
  guard_address_taken_iat_entries_(other.guard_address_taken_iat_entries_),
  guard_long_jump_targets_(other.guard_long_jump_targets_),
  guard_eh_continuation_functions_(other.guard_eh_continuation_functions_)
{
  if (other.chpe_ != nullptr) {
    chpe_ = other.chpe_->clone();
  }

  if (other.enclave_config_ != nullptr) {
    enclave_config_ = other.enclave_config_->clone();
  }

  if (other.volatile_metadata_ != nullptr) {
    volatile_metadata_ = other.volatile_metadata_->clone();
  }

  if (other.dynamic_relocs_.empty()) {
    dynamic_relocs_.reserve(other.dynamic_relocs_.size());
    std::transform(other.dynamic_relocs_.begin(), other.dynamic_relocs_.end(),
                   std::back_inserter(dynamic_relocs_),
      [] (const std::unique_ptr<DynamicRelocation>& R) { return R->clone(); }
    );
  }
}

LoadConfiguration& LoadConfiguration::operator=(const LoadConfiguration& other) {
  if (&other == this) {
    return *this;
  }
  Object::operator=(other);
  characteristics_ = other.characteristics_;
  timedatestamp_ = other.timedatestamp_;
  major_version_ = other.major_version_;
  minor_version_ = other.minor_version_;
  global_flags_clear_ = other.global_flags_clear_;
  global_flags_set_ = other.global_flags_set_;
  critical_section_default_timeout_ = other.critical_section_default_timeout_;
  decommit_free_block_threshold_ = other.decommit_free_block_threshold_;
  decommit_total_free_threshold_ = other.decommit_total_free_threshold_;
  lock_prefix_table_ = other.lock_prefix_table_;
  maximum_allocation_size_ = other.maximum_allocation_size_;
  virtual_memory_threshold_ = other.virtual_memory_threshold_;
  process_affinity_mask_ = other.process_affinity_mask_;
  process_heap_flags_ = other.process_heap_flags_;
  csd_version_ = other.csd_version_;
  reserved1_ = other.reserved1_;
  editlist_ = other.editlist_;
  security_cookie_ = other.security_cookie_;
  se_handler_table_ = other.se_handler_table_;
  se_handler_count_ = other.se_handler_count_;
  guard_cf_check_function_pointer_ = other.guard_cf_check_function_pointer_;
  guard_cf_dispatch_function_pointer_ = other.guard_cf_dispatch_function_pointer_;
  guard_cf_function_table_ = other.guard_cf_function_table_;
  guard_cf_function_count_ = other.guard_cf_function_count_;
  flags_ = other.flags_;
  code_integrity_ = other.code_integrity_;
  guard_address_taken_iat_entry_table_ = other.guard_address_taken_iat_entry_table_;
  guard_address_taken_iat_entry_count_ = other.guard_address_taken_iat_entry_count_;
  guard_long_jump_target_table_ = other.guard_long_jump_target_table_;
  guard_long_jump_target_count_ = other.guard_long_jump_target_count_;
  dynamic_value_reloc_table_ = other.dynamic_value_reloc_table_;
  hybrid_metadata_pointer_ = other.hybrid_metadata_pointer_;
  guard_rf_failure_routine_ = other.guard_rf_failure_routine_;
  guard_rf_failure_routine_function_pointer_ = other.guard_rf_failure_routine_function_pointer_;
  dynamic_value_reloctable_offset_ = other.dynamic_value_reloctable_offset_;
  dynamic_value_reloctable_section_ = other.dynamic_value_reloctable_section_;
  reserved2_ = other.reserved2_;
  guardrf_verify_stackpointer_function_pointer_ = other.guardrf_verify_stackpointer_function_pointer_;
  hotpatch_table_offset_ = other.hotpatch_table_offset_;
  reserved3_ = other.reserved3_;
  enclave_configuration_ptr_ = other.enclave_configuration_ptr_;
  volatile_metadata_pointer_ = other.volatile_metadata_pointer_;
  guard_eh_continuation_table_ = other.guard_eh_continuation_table_;
  guard_eh_continuation_count_ = other.guard_eh_continuation_count_;
  guard_xfg_check_function_pointer_ = other.guard_xfg_check_function_pointer_;
  guard_xfg_dispatch_function_pointer_ = other.guard_xfg_dispatch_function_pointer_;
  guard_xfg_table_dispatch_function_pointer_ = other.guard_xfg_table_dispatch_function_pointer_;
  cast_guard_os_determined_failure_mode_ = other.cast_guard_os_determined_failure_mode_;
  guard_memcpy_function_pointer_ = other.guard_memcpy_function_pointer_;
  uma_function_pointers_ = other.uma_function_pointers_;

  seh_rva_ = other.seh_rva_;
  guard_cf_functions_ = other.guard_cf_functions_;
  guard_address_taken_iat_entries_ = other.guard_address_taken_iat_entries_;
  guard_long_jump_targets_ = other.guard_long_jump_targets_;
  guard_eh_continuation_functions_ = other.guard_eh_continuation_functions_;

  if (other.chpe_ != nullptr) {
    chpe_ = other.chpe_->clone();
  }

  if (other.enclave_config_ != nullptr) {
    enclave_config_ = other.enclave_config_->clone();
  }

  if (other.volatile_metadata_ != nullptr) {
    volatile_metadata_ = other.volatile_metadata_->clone();
  }

  if (other.dynamic_relocs_.empty()) {
    dynamic_relocs_.reserve(other.dynamic_relocs_.size());
    std::transform(other.dynamic_relocs_.begin(), other.dynamic_relocs_.end(),
                   std::back_inserter(dynamic_relocs_),
      [] (const std::unique_ptr<DynamicRelocation>& R) { return R->clone(); }
    );
  }
  return *this;
}

void LoadConfiguration::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::string LoadConfiguration::to_string() const {
  using namespace fmt;
  static constexpr auto WIDTH = 65;
  std::ostringstream oss;
  oss << format("{:{}} 0x{:06x}\n", "Size:", WIDTH, size())
      << format("{:{}} {}\n", "Timestamp:", WIDTH, timedatestamp())
      << format("{:{}} {}.{}\n", "version:", WIDTH, major_version(), minor_version())
      << format("{:{}} {}\n", "GlobalFlags Clear:", WIDTH, global_flags_clear())
      << format("{:{}} {}\n", "GlobalFlags Set:", WIDTH, global_flags_set())
      << format("{:{}} {}\n", "Critical Section Default Timeout:", WIDTH, critical_section_default_timeout())
      << format("{:{}} {}\n", "Decommit Free Block Threshold:", WIDTH, decommit_free_block_threshold())
      << format("{:{}} {}\n", "Decommit Total Free Threshold:", WIDTH, decommit_total_free_threshold())
      << format("{:{}} {}\n", "Lock Prefix Table:", WIDTH, lock_prefix_table())
      << format("{:{}} {}\n", "Maximum Allocation Size:", WIDTH, maximum_allocation_size())
      << format("{:{}} {}\n", "Virtual Memory Threshold:", WIDTH, virtual_memory_threshold())
      << format("{:{}} {}\n", "Process Affinity Mask:", WIDTH, process_affinity_mask())
      << format("{:{}} {}\n", "Process Heap Flags:", WIDTH, process_heap_flags())
      << format("{:{}} {}\n", "CSD Version:", WIDTH, csd_version())
      << format("{:{}} {}\n", "Dependent Load Flag:", WIDTH, dependent_load_flags())
      << format("{:{}} {}\n", "Edit List:", WIDTH, editlist())
      << format("{:{}} 0x{:016x}\n", "Security Cookie:", WIDTH, security_cookie());

  if (auto val = se_handler_table()) {
    oss << format("{:{}} 0x{:06x}\n", "SEH Table:", WIDTH, *val);
  }

  if (auto val = se_handler_count()) {
    oss << format("{:{}} {}\n", "SEH Count:", WIDTH, *val);
  }

  if (auto val = guard_cf_check_function_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard CF address of check-function pointer:", WIDTH, *val);
  }

  if (auto val = guard_cf_dispatch_function_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard CF address of dispatch-function pointer:", WIDTH, *val);
  }

  if (auto val = guard_cf_function_table()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard CF function table:", WIDTH, *val);
  }

  if (auto val = guard_cf_function_count()) {
    oss << format("{:{}} {}\n", "Guard CF function count:", WIDTH, *val);
  }

  if (auto val = guard_flags(); val && *val != 0) {
    oss << format("{:{}} {}\n", "Guard Flags:", WIDTH,
                  fmt::join(guard_cf_flags_list(), ","));
  }

  if (const CodeIntegrity* CI = code_integrity()) {
    oss << format("{:{}} {}\n", "Code Integrity Flags:", WIDTH, CI->flags())
        << format("{:{}} 0x{:08x}\n", "Code Integrity Catalog:", WIDTH, CI->catalog())
        << format("{:{}} 0x{:08x}\n", "Code Integrity Catalog Offset:", WIDTH, CI->catalog_offset())
        << format("{:{}} {}\n", "Code Integrity Reserved:", WIDTH, CI->reserved());
  }

  if (auto val = guard_address_taken_iat_entry_table()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard CF address taken IAT entry table:", WIDTH, *val);
  }

  if (auto val = guard_address_taken_iat_entry_count()) {
    oss << format("{:{}} {}\n", "Guard CF address taken IAT entry count:", WIDTH, *val);
  }

  if (auto val = guard_long_jump_target_table()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard CF long jump target table:", WIDTH, *val);
  }

  if (auto val = guard_long_jump_target_count()) {
    oss << format("{:{}} {}\n", "Guard CF long jump target count:", WIDTH, *val);
  }

  if (auto val = dynamic_value_reloc_table()) {
    oss << format("{:{}} 0x{:016x}\n", "Dynamic value relocation table:", WIDTH, *val);
  }

  if (auto val = hybrid_metadata_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Hybrid metadata pointer:", WIDTH, *val);
  }

  if (auto val = guard_rf_failure_routine()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard RF address of failure-function:", WIDTH, *val);
  }

  if (auto val = guard_rf_failure_routine_function_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard RF address of failure-function pointer:", WIDTH, *val);
  }

  if (auto val = dynamic_value_reloctable_offset()) {
    oss << format("{:{}} 0x{:016x}\n", "Dynamic value relocation table offset:", WIDTH, *val);
  }

  if (auto val = dynamic_value_reloctable_section()) {
    oss << format("{:{}} {}\n", "Dynamic value relocation table section:", WIDTH, *val);
  }

  if (auto val = reserved2()) {
    oss << format("{:{}} {}\n", "Reserved2:", WIDTH, *val);
  }

  if (auto val = guard_rf_verify_stackpointer_function_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard RF address of stack pointer verification function pointer:", WIDTH, *val);
  }

  if (auto val = hotpatch_table_offset()) {
    oss << format("{:{}} 0x{:08}\n", "Hot patching table offset:", WIDTH, *val);
  }

  if (auto val = reserved3()) {
    oss << format("{:{}} {}\n", "Reserved3:", WIDTH, *val);
  }

  if (auto val = enclave_configuration_ptr()) {
    oss << format("{:{}} 0x{:016x}\n", "Enclave configuration pointer", WIDTH, *val);
  }

  if (auto val = volatile_metadata_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Volatile metadata pointer", WIDTH, *val);
  }

  if (auto val = guard_eh_continuation_table()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard EH continuation table", WIDTH, *val);
  }

  if (auto val = guard_eh_continuation_count()) {
    oss << format("{:{}} {}\n", "Guard EH continuation count", WIDTH, *val);
  }

  if (auto val = guard_xfg_check_function_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard XFG address of check-function pointer", WIDTH, *val);
  }

  if (auto val = guard_xfg_dispatch_function_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard XFG address of dispatch-function pointer", WIDTH, *val);
  }

  if (auto val = guard_xfg_table_dispatch_function_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard XFG address of dispatch-table-function pointer", WIDTH, *val);
  }

  if (auto val = cast_guard_os_determined_failure_mode()) {
    oss << format("{:{}} 0x{:016x}\n", "CastGuard OS determined failure mode", WIDTH, *val);
  }

  if (auto val = guard_memcpy_function_pointer()) {
    oss << format("{:{}} 0x{:016x}\n", "Guard memcpy function pointer", WIDTH, *val);
  }

  if (auto val = uma_function_pointers()) {
    oss << format("{:{}} 0x{:016x}\n", "UMA function pointers", WIDTH, *val);
  }

  if (const CHPEMetadata* metadata = chpe_metadata()) {
    oss << "  CHPE Metadata:\n"
        << indent(metadata->to_string(), 4);
  }

  if (!seh_rva_.empty()) {
    oss << format("  SEH Table ({}) {{\n", seh_rva_.size()) ;
    for (uint32_t RVA : seh_rva_) {
      oss << format("    0x{:08x}\n", RVA);
    }
    oss << "}\n";
  }

  if (!guard_cf_functions_.empty()) {
    oss << format("  Guard CF Function ({}) {{\n", guard_cf_functions_.size()) ;
    for (const guard_function_t& F : guard_cf_functions_) {
      oss << format("    0x{:08x} ({})\n", F.rva, F.extra);
    }
    oss << "}\n";
  }

  if (!guard_address_taken_iat_entries_.empty()) {
    oss << format("  Guard CF Address Taken IAT ({}) {{\n", guard_address_taken_iat_entries_.size()) ;
    for (const guard_function_t& F : guard_address_taken_iat_entries_) {
      oss << format("    0x{:08x} ({})\n", F.rva, F.extra);
    }
    oss << "}\n";
  }

  if (!guard_long_jump_targets_.empty()) {
    oss << format("  Guard CF Long Jump Target ({}) {{\n", guard_long_jump_targets_.size()) ;
    for (const guard_function_t& F : guard_long_jump_targets_) {
      oss << format("    0x{:08x} ({})\n", F.rva, F.extra);
    }
    oss << "}\n";
  }

  if (!guard_eh_continuation_functions_.empty()) {
    oss << format("  Guard EH Continuation ({}) {{\n", guard_eh_continuation_functions_.size()) ;
    for (const guard_function_t& F : guard_eh_continuation_functions_) {
      oss << format("    0x{:08x} ({})\n", F.rva, F.extra);
    }
    oss << "}\n";
  }

  for (const DynamicRelocation& DR : dynamic_relocations()) {
    oss << DR << '\n';
  }

  if (const EnclaveConfiguration* enclave = enclave_config()) {
    oss << "  Enclave Configuration:\n"
        << indent(enclave->to_string(), 4);
  }

  if (const VolatileMetadata* metadata = volatile_metadata()) {
    oss << "  Volatile Metadata:\n"
        << indent(metadata->to_string(), 4);
  }

  return oss.str();
}

const char* to_string(LoadConfiguration::IMAGE_GUARD e) {
  #define ENTRY(X) std::pair(LoadConfiguration::IMAGE_GUARD::X, #X)
  STRING_MAP enums2str {
    ENTRY(NONE),
    ENTRY(CF_INSTRUMENTED),
    ENTRY(CFW_INSTRUMENTED),
    ENTRY(CF_FUNCTION_TABLE_PRESENT),
    ENTRY(SECURITY_COOKIE_UNUSED),
    ENTRY(PROTECT_DELAYLOAD_IAT),
    ENTRY(DELAYLOAD_IAT_IN_ITS_OWN_SECTION),
    ENTRY(CF_EXPORT_SUPPRESSION_INFO_PRESENT),
    ENTRY(CF_ENABLE_EXPORT_SUPPRESSION),
    ENTRY(CF_LONGJUMP_TABLE_PRESENT),
    ENTRY(RF_INSTRUMENTED),
    ENTRY(RF_ENABLE),
    ENTRY(RF_STRICT),
    ENTRY(RETPOLINE_PRESENT),
    ENTRY(EH_CONTINUATION_TABLE_PRESENT),
    ENTRY(XFG_ENABLED),
    ENTRY(CASTGUARD_PRESENT),
    ENTRY(MEMCPY_PRESENT),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

std::vector<LoadConfiguration::IMAGE_GUARD>
  LoadConfiguration::guard_cf_flags_list() const
{
  std::vector<IMAGE_GUARD> flags_list;
  flags_list.reserve(3);
  std::copy_if(IMAGE_GUARD_LIST.begin(), IMAGE_GUARD_LIST.end(),
               std::back_inserter(flags_list),
               [this] (IMAGE_GUARD f) { return has(f); });

  return flags_list;

}

template std::unique_ptr<LoadConfiguration>
  LoadConfiguration::parse<details::PE32>(Parser& ctx, BinaryStream& stream);

template std::unique_ptr<LoadConfiguration>
  LoadConfiguration::parse<details::PE64>(Parser& ctx, BinaryStream& stream);

template ok_error_t LoadConfiguration::parse_enclave_config<details::PE32>(
  Parser& ctx, LoadConfiguration& config);

template ok_error_t LoadConfiguration::parse_enclave_config<details::PE64>(
  Parser& ctx, LoadConfiguration& config);

} // namespace PE
} // namespace LIEF

