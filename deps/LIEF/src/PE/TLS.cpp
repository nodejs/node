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
#include "LIEF/Visitor.hpp"

#include "LIEF/PE/TLS.hpp"
#include "LIEF/PE/Section.hpp"
#include "PE/Structures.hpp"

#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>


namespace LIEF::PE {

TLS::TLS(const details::pe32_tls& header) :
  va_rawdata_{header.RawDataStartVA, header.RawDataEndVA},
  addressof_index_{header.AddressOfIndex},
  addressof_callbacks_{header.AddressOfCallback},
  sizeof_zero_fill_{header.SizeOfZeroFill},
  characteristics_{header.Characteristics} {}

TLS::TLS(const details::pe64_tls& header) :
  va_rawdata_{header.RawDataStartVA, header.RawDataEndVA},
  addressof_index_{header.AddressOfIndex},
  addressof_callbacks_{header.AddressOfCallback},
  sizeof_zero_fill_{header.SizeOfZeroFill},
  characteristics_{header.Characteristics} {}

void TLS::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const TLS& entry) {
  os << fmt::format("Address of index:     {:#018x}\n", entry.addressof_index())
     << fmt::format("Address of callbacks: {:#018x}\n",
                    entry.addressof_callbacks())
     << fmt::format("Address of raw data:  [{:#018x}, {:#018x}] ({} bytes)\n",
                    entry.addressof_raw_data().first,
                    entry.addressof_raw_data().second,
                    entry.addressof_raw_data().second -
                        entry.addressof_raw_data().first)
     << fmt::format("Size of zerofill:     {:#x}\n", entry.sizeof_zero_fill())
     << fmt::format("Characteristics:      {:#x}\n", entry.characteristics());

  if (const Section* section = entry.section()) {
    os << fmt::format("Section:              '{}'\n", section->name());
  }

  if (const std::vector<uint64_t>& cbk = entry.callbacks(); !cbk.empty()) {
    os << fmt::format("Callbacks (#{}):\n", cbk.size());
    for (size_t i = 0; i < cbk.size(); ++i) {
      os << fmt::format("  [{:02d}]: {:#018x}\n", i, cbk[i]);
    }
  }

  return os;
}

} // namespace LIEF::PE
