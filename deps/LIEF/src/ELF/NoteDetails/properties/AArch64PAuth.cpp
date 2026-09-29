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
#include "LIEF/ELF/NoteDetails/properties/AArch64PAuth.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

namespace LIEF::ELF {

std::unique_ptr<AArch64PAuth> AArch64PAuth::create(BinaryStream& stream) {
  uint64_t platform = stream.read<uint64_t>().value_or(0);
  uint64_t version = stream.read<uint64_t>().value_or(0);

  return std::unique_ptr<AArch64PAuth>(new AArch64PAuth(platform, version));
}

void AArch64PAuth::dump(std::ostream& os) const {
  os << fmt::format("Platform: {:#06x}, Version: {:#06x}", platform(), version());
}


}
