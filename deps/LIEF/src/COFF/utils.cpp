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
#include "LIEF/COFF/utils.hpp"
#include "COFF/structures.hpp"

namespace LIEF::COFF {

bool is_coff(const std::string& file) {
  result<FileStream> fs = LIEF::FileStream::from_file(file);
  return fs ? is_coff(*fs) : false;
}

Header::KIND get_kind(BinaryStream& stream) {
  ScopedStream scoped(stream, 0);

  auto chunk_1 = scoped->peek<uint16_t>();
  if (!chunk_1) {
    return Header::KIND::UNKNOWN;
  }

  if ((Header::MACHINE_TYPES)*chunk_1 == Header::MACHINE_TYPES::UNKNOWN) {
    auto bigobj = scoped->peek<details::bigobj_header>();
    if (!bigobj) {
      return Header::KIND::UNKNOWN;
    }

    if (bigobj->sig2 != 0xFFFF || bigobj->version < 2) {
      return Header::KIND::UNKNOWN;
    }
    return Header::KIND::BIGOBJ;
  }

  auto regular = scoped->peek<details::header>();
  if (!regular) {
    return Header::KIND::UNKNOWN;
  }

  return LIEF::PE::Header::is_known_machine(regular->machine) ?
         Header::KIND::REGULAR : Header::KIND::UNKNOWN;
}

}
