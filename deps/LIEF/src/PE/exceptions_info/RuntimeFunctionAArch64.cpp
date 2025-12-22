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
#include "LIEF/PE/exceptions_info/RuntimeFunctionAArch64.hpp"
#include "LIEF/PE/exceptions_info/AArch64/PackedFunction.hpp"
#include "LIEF/PE/exceptions_info/AArch64/UnpackedFunction.hpp"
#include "LIEF/PE/exceptions_info/internal_arm64.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"

#include "logging.hpp"

#include "PE/exceptions_info/UnwindAArch64Decoder.hpp"

namespace LIEF::PE {

std::unique_ptr<RuntimeFunctionAArch64>
  RuntimeFunctionAArch64::parse(Parser& ctx, BinaryStream& strm)
{
  auto rva_start = strm.read<uint32_t>();
  if (!rva_start) {
    LIEF_WARN("Can't read exception info RVA start (line: {})", __LINE__);
    return nullptr;
  }

  auto unwind_data = strm.read<uint32_t>();
  if (!unwind_data) {
    LIEF_DEBUG("Can't read exception info unwind data (line: {}, RVA=0x{:08x}, pos={}, size={})",
               __LINE__, *rva_start, strm.pos(), strm.size());
    return nullptr;
  }

  auto flag = PACKED_FLAGS(*unwind_data & 0x3);

  if (flag == PACKED_FLAGS::PACKED || flag == PACKED_FLAGS::PACKED_FRAGMENT) {
    return unwind_aarch64::PackedFunction::parse(ctx, strm, *rva_start,
                                                 *unwind_data);
  }

  if (flag == PACKED_FLAGS::UNPACKED) {
    uint32_t xdata_rva = details::xdata_unpacked_rva(*unwind_data);
    uint32_t xdata_offset = ctx.bin().rva_to_offset(xdata_rva);
    ScopedStream xdata_strm(ctx.stream(), xdata_offset);
    return unwind_aarch64::UnpackedFunction::parse(ctx, *xdata_strm, xdata_rva,
                                                   *rva_start);
  }

  return std::make_unique<RuntimeFunctionAArch64>(*rva_start, /*length*/0, flag);
}

std::string RuntimeFunctionAArch64::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << "Runtime Unpacked AArch64 Function {\n"
      << format("  Start (RVA): 0x{:08x}\n", rva_start())
      << format("  Flag: {}\n", (int)flag());
  oss << "}\n";
  return oss.str();
}
}
