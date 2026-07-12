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
#include <string>
#include "LIEF/VDEX/Header.hpp"

namespace LIEF {
namespace VDEX {

template<class T>
Header::Header(const T* header) :
  magic_{},
  version_{0},
  nb_dex_files_{header->number_of_dex_files},
  dex_size_{header->dex_size},
  verifier_deps_size_{header->verifier_deps_size},
  quickening_info_size_{header->quickening_info_size}
{

  std::copy(
      std::begin(header->magic),
      std::end(header->magic),
      std::begin(magic_)
  );

  version_ = static_cast<vdex_version_t>(std::stoi(std::string{reinterpret_cast<const char*>(header->version), sizeof(header->version)}));

}


} // namespace VDEX
} // namespace LIEF
