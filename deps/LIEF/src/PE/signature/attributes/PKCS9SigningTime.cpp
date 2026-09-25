/* Copyright 2021 - 2025 R. Thomas
 * Copyright 2021 - 2025 Quarkslab
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
#include "LIEF/Visitor.hpp"
#include "LIEF/PE/signature/attributes/PKCS9SigningTime.hpp"
namespace LIEF {
namespace PE {

void PKCS9SigningTime::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::string PKCS9SigningTime::print() const {
  const time_t& time = this->time();
  return fmt::format("{}/{}/{} - {}:{}:{}",
                       time[0], time[1], time[2], time[3], time[4], time[5]);
}

}
}
