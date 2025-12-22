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
#include "LIEF/PE/LoadConfigurations/CHPEMetadata/MetadataARM64.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"

namespace LIEF::PE {
std::unique_ptr<CHPEMetadataARM64>
  CHPEMetadataARM64::parse(Parser& ctx, BinaryStream& stream, uint32_t version)
{
  // The structure defining Hybrid metadata for ARM64(EC) is located in
  // `ntimage.h` and named: `_IMAGE_ARM64EC_METADATA`
  //
  // ```
  // typedef struct _IMAGE_ARM64EC_METADATA {
  //     ULONG  Version;
  //     ULONG  CodeMap;
  //     ULONG  CodeMapCount;
  //     ULONG  CodeRangesToEntryPoints;
  //     ULONG  RedirectionMetadata;
  //     ULONG  tbd__os_arm64x_dispatch_call_no_redirect;
  //     ULONG  tbd__os_arm64x_dispatch_ret;
  //     ULONG  tbd__os_arm64x_dispatch_call;
  //     ULONG  tbd__os_arm64x_dispatch_icall;
  //     ULONG  tbd__os_arm64x_dispatch_icall_cfg;
  //     ULONG  AlternateEntryPoint;
  //     ULONG  AuxiliaryIAT;
  //     ULONG  CodeRangesToEntryPointsCount;
  //     ULONG  RedirectionMetadataCount;
  //     ULONG  GetX64InformationFunctionPointer;
  //     ULONG  SetX64InformationFunctionPointer;
  //     ULONG  ExtraRFETable;
  //     ULONG  ExtraRFETableSize;
  //     ULONG  __os_arm64x_dispatch_fptr;
  //     ULONG  AuxiliaryIATCopy;
  // } IMAGE_ARM64EC_METADATA;
  // ```
  auto CodeMap = stream.read<uint32_t>();
  if (!CodeMap) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: CodeMap");
    return nullptr;
  }

  auto CodeMapCount = stream.read<uint32_t>();
  if (!CodeMapCount) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: CodeMapCount");
    return nullptr;
  }

  auto CodeRangesToEntryPoints = stream.read<uint32_t>();
  if (!CodeRangesToEntryPoints) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: CodeRangesToEntryPoints");
    return nullptr;
  }

  auto RedirectionMetadata = stream.read<uint32_t>();
  if (!RedirectionMetadata) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: RedirectionMetadata");
    return nullptr;
  }

  auto __os_arm64x_dispatch_call_no_redirect = stream.read<uint32_t>();
  if (!__os_arm64x_dispatch_call_no_redirect) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: __os_arm64x_dispatch_call_no_redirect");
    return nullptr;
  }

  auto __os_arm64x_dispatch_ret = stream.read<uint32_t>();
  if (!__os_arm64x_dispatch_ret) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: __os_arm64x_dispatch_ret");
    return nullptr;
  }

  auto __os_arm64x_dispatch_call = stream.read<uint32_t>();
  if (!__os_arm64x_dispatch_call) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: __os_arm64x_dispatch_call");
    return nullptr;
  }

  auto __os_arm64x_dispatch_icall = stream.read<uint32_t>();
  if (!__os_arm64x_dispatch_icall) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: __os_arm64x_dispatch_icall");
    return nullptr;
  }

  auto __os_arm64x_dispatch_icall_cfg = stream.read<uint32_t>();
  if (!__os_arm64x_dispatch_icall_cfg) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: __os_arm64x_dispatch_icall_cfg");
    return nullptr;
  }

  auto AlternateEntryPoint = stream.read<uint32_t>();
  if (!AlternateEntryPoint) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: AlternateEntryPoint");
    return nullptr;
  }

  auto AuxiliaryIAT = stream.read<uint32_t>();
  if (!AuxiliaryIAT) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: AuxiliaryIAT");
    return nullptr;
  }

  auto CodeRangesToEntryPointsCount = stream.read<uint32_t>();
  if (!CodeRangesToEntryPointsCount) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: CodeRangesToEntryPointsCount");
    return nullptr;
  }

  auto RedirectionMetadataCount = stream.read<uint32_t>();
  if (!RedirectionMetadataCount) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: RedirectionMetadataCount");
    return nullptr;
  }

  auto GetX64InformationFunctionPointer = stream.read<uint32_t>();
  if (!GetX64InformationFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: GetX64InformationFunctionPointer");
    return nullptr;
  }

  auto SetX64InformationFunctionPointer = stream.read<uint32_t>();
  if (!SetX64InformationFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: SetX64InformationFunctionPointer");
    return nullptr;
  }

  auto ExtraRFETable = stream.read<uint32_t>();
  if (!ExtraRFETable) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: ExtraRFETable");
    return nullptr;
  }

  auto ExtraRFETableSize = stream.read<uint32_t>();
  if (!ExtraRFETableSize) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: ExtraRFETableSize");
    return nullptr;
  }

  auto __os_arm64x_dispatch_fptr = stream.read<uint32_t>();
  if (!__os_arm64x_dispatch_fptr) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: __os_arm64x_dispatch_fptr");
    return nullptr;
  }

  auto AuxiliaryIATCopy = stream.read<uint32_t>();
  if (!AuxiliaryIATCopy) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: AuxiliaryIATCopy");
    return nullptr;
  }

  auto AuxiliaryDelayImport = stream.read<uint32_t>();
  if (!AuxiliaryDelayImport) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: AuxiliaryDelayImport");
    return nullptr;
  }

  auto AuxiliaryDelayImportCopy = stream.read<uint32_t>();
  if (!AuxiliaryDelayImportCopy) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: AuxiliaryDelayImportCopy");
    return nullptr;
  }

  auto BitfieldInfo = stream.read<uint32_t>();
  if (!BitfieldInfo) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[ARM64]: BitfieldInfo");
    return nullptr;
  }

  auto metadata = std::make_unique<CHPEMetadataARM64>(version);

  (*metadata)
    .code_map(*CodeMap)
    .code_map_count(*CodeMapCount)
    .code_ranges_to_entrypoints(*CodeRangesToEntryPoints)
    .redirection_metadata(*RedirectionMetadata)
    .os_arm64x_dispatch_call_no_redirect(*__os_arm64x_dispatch_call_no_redirect)
    .os_arm64x_dispatch_ret(*__os_arm64x_dispatch_ret)
    .os_arm64x_dispatch_call(*__os_arm64x_dispatch_call)
    .os_arm64x_dispatch_icall(*__os_arm64x_dispatch_icall)
    .os_arm64x_dispatch_icall_cfg(*__os_arm64x_dispatch_icall_cfg)
    .alternate_entry_point(*AlternateEntryPoint)
    .auxiliary_iat(*AuxiliaryIAT)
    .code_ranges_to_entry_points_count(*CodeRangesToEntryPointsCount)
    .redirection_metadata_count(*RedirectionMetadataCount)
    .get_x64_information_function_pointer(*GetX64InformationFunctionPointer)
    .set_x64_information_function_pointer(*SetX64InformationFunctionPointer)
    .extra_rfe_table(*ExtraRFETable)
    .extra_rfe_table_size(*ExtraRFETableSize)
    .os_arm64x_dispatch_fptr(*__os_arm64x_dispatch_fptr)
    .auxiliary_iat_copy(*AuxiliaryIATCopy)
    .auxiliary_delay_import(*AuxiliaryDelayImport)
    .auxiliary_delay_import_copy(*AuxiliaryDelayImportCopy)
    .bitfield_info(*BitfieldInfo)
  ;

  if (metadata->code_map_count() > 0) {
    if (!parse_code_map(ctx, *metadata)) {
      LIEF_DEBUG("Code map parsing finished with error");
    }
  }

  if (metadata->redirection_metadata_count() > 0) {
    if (!parse_redirections(ctx, *metadata)) {
      LIEF_DEBUG("Redirection metadata parsing finished with error");
    }
  }

  if (metadata->code_ranges_to_entry_points_count() > 0) {
    if (!parse_code_ranges_to_entry_points(ctx, *metadata)) {
      LIEF_DEBUG("Code ranges to entrypoint parsing finished with error");
    }
  }

  return metadata;
}


ok_error_t CHPEMetadataARM64::parse_code_map(Parser& ctx,
                                             CHPEMetadataARM64& metadata)
{
  uint32_t offset = ctx.bin().rva_to_offset(metadata.code_map());
  const size_t count = metadata.code_map_count();

  ScopedStream stream(ctx.stream(), offset);

  // std::min to prevent corrupted count
  size_t reserved = std::min<size_t>(count, 15);
  metadata.range_entries_.reserve(reserved);

  for (size_t i = 0; i < count; ++i) {
    auto start_offset = stream->read<uint32_t>();
    if (!start_offset) {
      return make_error_code(start_offset.error());
    }

    auto length = stream->read<uint32_t>();
    if (!length) {
      return make_error_code(length.error());
    }

    metadata.range_entries_.push_back(range_entry_t{*start_offset, *length});
  }

  return ok();
}


ok_error_t CHPEMetadataARM64::parse_redirections(Parser& ctx,
                                                 CHPEMetadataARM64& metadata)
{
  uint32_t offset = ctx.bin().rva_to_offset(metadata.redirection_metadata());
  const size_t count = metadata.redirection_metadata_count();

  ScopedStream stream(ctx.stream(), offset);
  // std::min to prevent corrupted count
  size_t reserved = std::min<size_t>(count, 15);
  metadata.redirection_entries_.reserve(reserved);

  for (size_t i = 0; i < count; ++i) {
    auto src = stream->read<uint32_t>();
    if (!src) {
      return make_error_code(src.error());
    }

    auto dst = stream->read<uint32_t>();
    if (!dst) {
      return make_error_code(dst.error());
    }

    metadata.redirection_entries_.push_back(redirection_entry_t{*src, *dst});
  }
  return ok();
}

ok_error_t CHPEMetadataARM64::parse_code_ranges_to_entry_points(
    Parser& ctx, CHPEMetadataARM64& metadata)
{
  uint32_t offset = ctx.bin().rva_to_offset(metadata.code_ranges_to_entrypoints());
  const size_t count = metadata.code_ranges_to_entry_points_count();

  ScopedStream stream(ctx.stream(), offset);
  // std::min to prevent corrupted count
  size_t reserved = std::min<size_t>(count, 15);
  metadata.redirection_entries_.reserve(reserved);

  // Definition in ntimage.h:
  //
  // ```
  // typedef struct _IMAGE_ARM64EC_CODE_RANGE_ENTRY_POINT {
  //     ULONG StartRva;
  //     ULONG EndRva;
  //     ULONG EntryPoint;
  // } IMAGE_ARM64EC_CODE_RANGE_ENTRY_POINT;
  // ```
  for (size_t i = 0; i < count; ++i) {
    auto StartRva = stream->read<uint32_t>();
    if (!StartRva) {
      LIEF_DEBUG("Failed to read IMAGE_ARM64EC_CODE_RANGE_ENTRY_POINT.StartRva");
      return make_error_code(StartRva.error());
    }

    auto EndRva = stream->read<uint32_t>();
    if (!EndRva) {
      LIEF_DEBUG("Failed to read IMAGE_ARM64EC_CODE_RANGE_ENTRY_POINT.EndRva");
      return make_error_code(EndRva.error());
    }

    auto EntryPoint = stream->read<uint32_t>();
    if (!EntryPoint) {
      LIEF_DEBUG("Failed to read IMAGE_ARM64EC_CODE_RANGE_ENTRY_POINT.EntryPoint");
      return make_error_code(EntryPoint.error());
    }

    metadata.code_range_entry_point_entries_.push_back(
      code_range_entry_point_t{*StartRva, *EndRva, *EntryPoint}
    );
  }
  return ok();
}


std::string CHPEMetadataARM64::to_string() const {
  using namespace fmt;
  static constexpr auto WIDTH = 19;
  std::ostringstream oss;
  oss << format("{:>{}} Version\n", version(), WIDTH);
  oss << format("{:>#{}x} Arm64X dispatch call function pointer (no redirection)\n",
                os_arm64x_dispatch_call_no_redirect(), WIDTH);
  oss << format("{:>#{}x} Arm64X dispatch return function pointer\n",
                os_arm64x_dispatch_ret(), WIDTH);
  oss << format("{:>#{}x} Arm64X dispatch call function pointer\n",
                os_arm64x_dispatch_call(), WIDTH);
  oss << format("{:>#{}x} Arm64X dispatch indirect call function pointer\n",
                os_arm64x_dispatch_icall(), WIDTH);
  oss << format("{:>#{}x} Arm64X dispatch indirect call function pointer (with CFG check)\n",
                os_arm64x_dispatch_icall_cfg(), WIDTH);
  oss << format("{:>#{}x} Arm64X alternative entry point\n",
                alternate_entry_point(), WIDTH);
  oss << format("{:>#{}x} Arm64X auxiliary import address table\n",
                auxiliary_iat(), WIDTH);
  oss << format("{:>#{}x} Get x64 information function pointer\n",
                get_x64_information_function_pointer(), WIDTH);
  oss << format("{:>#{}x} Set x64 information function pointer\n",
                set_x64_information_function_pointer(), WIDTH);
  oss << format("{:>{}} Arm64X x64 code ranges to entry points table\n",
                format("{:#x}[{}]", code_ranges_to_entrypoints(),
                                    code_ranges_to_entry_points_count()), WIDTH);
  oss << format("{:>{}} Arm64X arm64x redirection metadata table\n",
                format("{:#x}[{}]", redirection_metadata(),
                                    redirection_metadata_count()), WIDTH);
  oss << format("{:>{}} Arm64X extra RFE table\n",
                format("{:#x}[{:#x}]", extra_rfe_table(),
                                       extra_rfe_table_size()), WIDTH);
  oss << format("{:>#{}x} Arm64X dispatch function pointer\n",
                os_arm64x_dispatch_fptr(), WIDTH);
  oss << format("{:>#{}x} Arm64X copy of auxiliary import address table\n",
                auxiliary_iat_copy(), WIDTH);
  oss << format("{:>#{}x} Arm64X auxiliary delayload import address table\n",
                auxiliary_delay_import(), WIDTH);
  oss << format("{:>#{}x} Arm64X auxiliary delayload import address table copy\n",
                auxiliary_delay_import_copy(), WIDTH);
  oss << format("{:>#{}x} Arm64X hybrid image info bitfield\n",
                bitfield_info(), WIDTH);

  if (!range_entries_.empty()) {
    oss << "Address Range:\n";
    for (const range_entry_t& entry : code_ranges()) {
      oss << format("{:>10} [0x{:08x}, 0x{:08x}]\n",
                    LIEF::PE::to_string(entry.type()), entry.start(), entry.end());
    }
  }

  if (!redirection_entries_.empty()) {
    oss << "Arm64X Redirection Metadata Table:\n";
    for (const redirection_entry_t& entry : redirections()) {
      oss << format("  0x{:08x} --> 0x{:08x}\n", entry.src, entry.dst);
    }
  }

  if (!code_range_entry_point_entries_.empty()) {
    oss << "Arm64X X64 Code Ranges to Entrypoint:\n";
    for (const code_range_entry_point_t& entry : code_range_entry_point()) {
      oss << format("  [0x{:08x}, 0x{:08x}] --> 0x{:08x}\n",
          entry.start_rva, entry.end_rva, entry.entrypoint);
    }
  }

  return oss.str();
}


const char* to_string(CHPEMetadataARM64::range_entry_t::TYPE e) {
  using TYPE = CHPEMetadataARM64::range_entry_t::TYPE;
  switch (e) {
    case TYPE::AMD64: return "AMD64";
    case TYPE::ARM64: return "ARM64";
    case TYPE::ARM64EC: return "ARM64EC";
  }
  return "UNKNOWN";
}

}
