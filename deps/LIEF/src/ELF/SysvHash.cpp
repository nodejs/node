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
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

#include "LIEF/ELF/hash.hpp"

#include "LIEF/ELF/SysvHash.hpp"


namespace LIEF::ELF {

void SysvHash::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const SysvHash& sysvhash) {
  os << fmt::format("Number of buckets:           {}\n", sysvhash.nbucket())
     << fmt::format("Buckets:                     [{}]\n",
                    fmt::join(sysvhash.buckets(), ", "))
     << fmt::format("Number of chains:            {}\n", sysvhash.nchain())
     << fmt::format("Chains:                      [{}]\n",
                    fmt::join(sysvhash.chains(), ", "));
  return os;
}

} // namespace LIEF::ELF
