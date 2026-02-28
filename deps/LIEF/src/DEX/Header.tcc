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
#include "LIEF/DEX/Header.hpp"
namespace LIEF {
namespace DEX {

template<class T>
Header::Header(const T& header) :
  magic_{},
  checksum_{header.checksum},
  signature_{},
  file_size_{header.file_size},
  header_size_{header.header_size},
  endian_tag_{header.endian_tag},

  link_size_{header.link_size},
  link_off_{header.link_off},

  map_off_{header.map_off},

  string_ids_size_{header.string_ids_size},
  string_ids_off_{header.string_ids_off},

  type_ids_size_{header.type_ids_size},
  type_ids_off_{header.type_ids_off},

  proto_ids_size_{header.proto_ids_size},
  proto_ids_off_{header.proto_ids_off},

  field_ids_size_{header.field_ids_size},
  field_ids_off_{header.field_ids_off},

  method_ids_size_{header.method_ids_size},
  method_ids_off_{header.method_ids_off},

  class_defs_size_{header.class_defs_size},
  class_defs_off_{header.class_defs_off},

  data_size_{header.data_size},
  data_off_{header.data_off}
{
  std::copy(std::begin(header.magic), std::end(header.magic),
            std::begin(magic_));

  std::copy(std::begin(header.signature), std::end(header.signature),
            std::begin(signature_));
}


} // namespace DEX
} // namespace LIEF
