/* Copyright 2024 - 2025 R. Thomas
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
#include "LIEF/MachO/UnknownCommand.hpp"
#include "LIEF/Visitor.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF::MachO {

void UnknownCommand::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& UnknownCommand::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("Original Command: {}", original_command());
  return os;
}

}
