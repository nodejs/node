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

#include "LIEF/runtime/osx/Host.hpp"

#include <tuple>
#include <spdlog/fmt/fmt.h>

namespace LIEF::runtime::osx {
bool Host::version_t::operator<=(const version_t& rhs) const {
  return std::tie(major, minor, patch) <=
         std::tie(rhs.major, rhs.minor, rhs.patch);
}

bool Host::version_t::operator>=(const version_t& rhs) const {
  return std::tie(major, minor, patch) >=
         std::tie(rhs.major, rhs.minor, rhs.patch);
}

bool Host::version_t::operator==(const version_t& other) const {
  return std::tie(major, minor, patch) ==
         std::tie(other.major, other.minor, other.patch);
}

std::string Host::version_t::to_string() const {
  return fmt::format("{}.{}.{}", major, minor, patch);
}

const Host::version_t& Host::version_t::BigSur() {
  static const version_t v{11, 0, 0};
  return v;
}

const Host::version_t& Host::version_t::Monterey() {
  static const version_t v{12, 0, 0};
  return v;
}

const Host::version_t& Host::version_t::Ventura() {
  static const version_t v{13, 0, 0};
  return v;
}

const Host::version_t& Host::version_t::Sonoma() {
  static const version_t v{14, 0, 0};
  return v;
}

const Host::version_t& Host::version_t::Sequoia() {
  static const version_t v{15, 0, 0};
  return v;
}

const Host::version_t& Host::version_t::Tahoe() {
  static const version_t v{26, 0, 0};
  return v;
}

}
