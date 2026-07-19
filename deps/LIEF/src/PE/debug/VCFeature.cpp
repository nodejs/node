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
#include "LIEF/PE/debug/VCFeature.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"

#include "logging.hpp"

namespace LIEF::PE {

std::unique_ptr<VCFeature> VCFeature::parse(
  const details::pe_debug& hdr, Section* section, span<uint8_t> payload)
{
  SpanStream stream(payload);

  auto pre_vc = stream.read<uint32_t>().value_or(0);
  auto c_cpp = stream.read<uint32_t>().value_or(0);
  auto gs = stream.read<uint32_t>().value_or(0);
  auto sdk = stream.read<uint32_t>().value_or(0);
  auto guardn = stream.read<uint32_t>().value_or(0);

  return std::make_unique<VCFeature>(hdr, section, pre_vc, c_cpp, gs, sdk,
                                     guardn);
}

std::string VCFeature::to_string() const {
  std::ostringstream os;
  os << Debug::to_string() << '\n'
     << fmt::format("  Counts: Pre-VC++ 11.00={}, C/C++={}, /GS={}, /sdl={}, "
                    "guardN={}", pre_vcpp(), c_cpp(), gs(), sdl(), guards());
  return os.str();
}

}
