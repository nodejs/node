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
#include "LIEF/PE/signature/attributes/PKCS9CounterSignature.hpp"
#include "LIEF/Visitor.hpp"

#include <sstream>
namespace LIEF {
namespace PE {

void PKCS9CounterSignature::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::string PKCS9CounterSignature::print() const {
  std::ostringstream oss;
  oss << signer() << "\n";
  return oss.str();
}


}
}
