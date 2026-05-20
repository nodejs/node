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
#include "LIEF/PE/LoadConfigurations/CHPEMetadata/MetadataX86.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"

namespace LIEF::PE {
std::unique_ptr<CHPEMetadataX86>
  CHPEMetadataX86::parse(Parser&/*ctx*/, BinaryStream& stream, uint32_t version)
{
  // The structure defining Hybrid metadata for X86 is located in
  // `ntimage.h` and named: `IMAGE_CHPE_METADATA_X86`
  //
  // typedef struct _IMAGE_CHPE_METADATA_X86 {
  //     ULONG  Version;
  //     ULONG  CHPECodeAddressRangeOffset;
  //     ULONG  CHPECodeAddressRangeCount;
  //     ULONG  WowA64ExceptionHandlerFunctionPointer;
  //     ULONG  WowA64DispatchCallFunctionPointer;
  //     ULONG  WowA64DispatchIndirectCallFunctionPointer;
  //     ULONG  WowA64DispatchIndirectCallCfgFunctionPointer;
  //     ULONG  WowA64DispatchRetFunctionPointer;
  //     ULONG  WowA64DispatchRetLeafFunctionPointer;
  //     ULONG  WowA64DispatchJumpFunctionPointer;
  //     ULONG  CompilerIATPointer;         // Present if Version >= 2
  //     ULONG  WowA64RdtscFunctionPointer; // Present if Version >= 3
  // } IMAGE_CHPE_METADATA_X86, *PIMAGE_CHPE_METADATA_X86;

  auto CHPECodeAddressRangeOffset = stream.read<uint32_t>();
  if (!CHPECodeAddressRangeOffset) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: CHPECodeAddressRangeOffset");
    return nullptr;
  }

  auto CHPECodeAddressRangeCount = stream.read<uint32_t>();
  if (!CHPECodeAddressRangeCount) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: CHPECodeAddressRangeCount");
    return nullptr;
  }

  auto WowA64ExceptionHandlerFunctionPointer = stream.read<uint32_t>();
  if (!WowA64ExceptionHandlerFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: WowA64ExceptionHandlerFunctionPointer");
    return nullptr;
  }

  auto WowA64DispatchCallFunctionPointer = stream.read<uint32_t>();
  if (!WowA64DispatchCallFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: WowA64DispatchCallFunctionPointer");
    return nullptr;
  }

  auto WowA64DispatchIndirectCallFunctionPointer = stream.read<uint32_t>();
  if (!WowA64DispatchIndirectCallFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: WowA64DispatchIndirectCallFunctionPointer");
    return nullptr;
  }

  auto WowA64DispatchIndirectCallCfgFunctionPointer = stream.read<uint32_t>();
  if (!WowA64DispatchIndirectCallCfgFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: WowA64DispatchIndirectCallCfgFunctionPointer");
    return nullptr;
  }

  auto WowA64DispatchRetFunctionPointer = stream.read<uint32_t>();
  if (!WowA64DispatchRetFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: WowA64DispatchRetFunctionPointer");
    return nullptr;
  }

  auto WowA64DispatchRetLeafFunctionPointer = stream.read<uint32_t>();
  if (!WowA64DispatchRetLeafFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: WowA64DispatchRetLeafFunctionPointer");
    return nullptr;
  }

  auto WowA64DispatchJumpFunctionPointer = stream.read<uint32_t>();
  if (!WowA64DispatchJumpFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: WowA64DispatchJumpFunctionPointer");
    return nullptr;
  }

  auto metadata = std::make_unique<CHPEMetadataX86>(version);

  (*metadata)
    .chpe_code_address_range_offset(*CHPECodeAddressRangeOffset)
    .chpe_code_address_range_count(*CHPECodeAddressRangeCount)
    .wowa64_exception_handler_function_pointer(*WowA64ExceptionHandlerFunctionPointer)
    .wowa64_dispatch_call_function_pointer(*WowA64DispatchCallFunctionPointer)
    .wowa64_dispatch_indirect_call_function_pointer(*WowA64DispatchIndirectCallFunctionPointer)
    .wowa64_dispatch_indirect_call_cfg_function_pointer(*WowA64DispatchIndirectCallCfgFunctionPointer)
    .wowa64_dispatch_ret_function_pointer(*WowA64DispatchRetFunctionPointer)
    .wowa64_dispatch_ret_leaf_function_pointer(*WowA64DispatchRetLeafFunctionPointer)
    .wowa64_dispatch_jump_function_pointer(*WowA64DispatchJumpFunctionPointer)
  ;

  if (version < 2) {
    return metadata;
  }

  auto CompilerIATPointer = stream.read<uint32_t>();
  if (!CompilerIATPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: CompilerIATPointer");
    return metadata;
  }

  if (version < 3) {
    return metadata;
  }

  auto WowA64RdtscFunctionPointer = stream.read<uint32_t>();
  if (!WowA64RdtscFunctionPointer) {
    LIEF_DEBUG("Failed to read Compiled Hybrid Metadata[X86]: WowA64RdtscFunctionPointer");
    return metadata;
  }
  return metadata;
}

std::string CHPEMetadataX86::to_string() const {
  using namespace fmt;
  static constexpr auto WIDTH = 19;
  std::ostringstream oss;
  oss << format("{:>{}} Version\n", version(), WIDTH);
  oss << format("{:>#{}x} WowA64 exception handler function pointer\n",
                wowa64_exception_handler_function_pointer(), WIDTH)
      << format("{:>#{}x} WowA64 dispatch call function pointer\n",
                wowa64_dispatch_call_function_pointer(), WIDTH)
      << format("{:>#{}x} WowA64 dispatch indirect call function pointer\n",
                wowa64_dispatch_indirect_call_function_pointer(), WIDTH)
      << format("{:>#{}x} WowA64 dispatch indirect call function pointer (with CFG check)\n",
                wowa64_dispatch_indirect_call_cfg_function_pointer(), WIDTH)
      << format("{:>#{}x} WowA64 dispatch return function pointer\n",
                wowa64_dispatch_ret_function_pointer(), WIDTH)
      << format("{:>#{}x} WowA64 dispatch leaf return function pointer\n",
                wowa64_dispatch_ret_leaf_function_pointer(), WIDTH)
      << format("{:>#{}x} WowA64 dispatch jump function pointer\n",
                wowa64_dispatch_jump_function_pointer(), WIDTH)
      << format("{:>{}} Hybrid code address range\n",
                format("{:#x}[{:#x}]", chpe_code_address_range_offset(),
                                       chpe_code_address_range_count()), WIDTH);

  if (auto opt = compiler_iat_pointer()) {
    oss << format("{:>#{}x} WowA64 auxiliary import address table pointer\n",
                  *opt, WIDTH);
  }

  if (auto opt = wowa64_rdtsc_function_pointer()) {
    oss << format("{:>#{}x} WowA64 RDTSC function pointer\n",
                  *opt, WIDTH);
  }
  std::string output = oss.str();
  output.pop_back();
  return output;
}


}
