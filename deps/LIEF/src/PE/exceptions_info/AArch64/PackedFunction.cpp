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
#include "LIEF/PE/exceptions_info/AArch64/PackedFunction.hpp"
#include "logging.hpp"

#include "LIEF/PE/exceptions_info/internal_arm64.hpp"

namespace LIEF::PE::unwind_aarch64 {
std::unique_ptr<PackedFunction> PackedFunction::parse(
  Parser& /*ctx*/, BinaryStream& /*strm*/, uint32_t rva, uint32_t unwind_data
) {
  static constexpr auto WIDTH = 20;
  auto packed_info = details::arm64_packed_t{unwind_data};

  LIEF_DEBUG("Parsing packed function 0x{:08x}", rva);
  const auto flag = PACKED_FLAGS(unwind_data & 0x3);
  auto func = std::make_unique<PackedFunction>(
    rva, packed_info.function_length(), flag
  );

  LIEF_DEBUG("  {:{}}: 0x{:08x}", "Raw Data", WIDTH, unwind_data);
  LIEF_DEBUG("  {:{}}: 0x{:04x}", "Function RVA", WIDTH, rva);
  LIEF_DEBUG("  {:{}}: 0x{:04x}", "Function Length", WIDTH, packed_info.function_length());
  LIEF_DEBUG("  {:{}}: 0x{:04x}", "Frame size", WIDTH, packed_info.frame_size());

  (*func)
    .reg_F(packed_info.RF())
    .reg_I(packed_info.RI())
    .H(packed_info.H())
    .CR(packed_info.CR())
    .frame_size(packed_info.frame_size() * 16)
  ;
  return func;
}

std::string PackedFunction::to_string() const {
  using namespace fmt;
  std::ostringstream oss;
  oss << "Runtime Packed AArch64 Function {\n"
      << format("  Range(RVA): 0x{:08x} - 0x{:08x}\n", rva_start(), rva_end())
      << format("  Length={} FrameSize=0x{:02x} RegF={} RegI={} H={} CR={}\n",
                length(), frame_size(), reg_F(), reg_I(), H(), CR());
  oss << '}';
  return oss.str();
}
}
