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

#include "LIEF/runtime/windows/Host.hpp"

#include <tuple>
#include <spdlog/fmt/fmt.h>

namespace LIEF::runtime::windows {
bool Host::version_t::operator<=(const version_t& rhs) const {
  return std::tie(major, minor, build_number) <=
         std::tie(rhs.major, rhs.minor, rhs.build_number);
}

bool Host::version_t::operator>=(const version_t& rhs) const {
  return std::tie(major, minor, build_number) >=
         std::tie(rhs.major, rhs.minor, rhs.build_number);
}

bool Host::version_t::operator==(const version_t& other) const {
  return std::tie(major, minor, build_number) ==
         std::tie(other.major, other.minor, other.build_number);
}

std::string Host::version_t::to_string() const {
  return fmt::format("{}.{}.{}", major, minor, build_number);
}

}
