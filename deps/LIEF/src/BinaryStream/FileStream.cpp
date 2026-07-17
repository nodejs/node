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
#include "logging.hpp"

#include "LIEF/BinaryStream/FileStream.hpp"
namespace LIEF {

result<FileStream> FileStream::from_file(const std::string& file) {
  std::ifstream ifs(file, std::ios::in | std::ios::binary);
  if (!ifs) {
    LIEF_ERR("Can't open '{}'", file);
    return make_error_code(lief_errors::read_error);
  }

  ifs.unsetf(std::ios::skipws);
  ifs.seekg(0, std::ios::end);
  const auto size = static_cast<uint64_t>(ifs.tellg());
  ifs.seekg(0, std::ios::beg);
  return result<FileStream>(tl::in_place, std::move(ifs), size);
}


std::vector<uint8_t> FileStream::content() const {
  std::vector<uint8_t> data(size_, 0);
  const auto pos = ifs_.tellg();

  ifs_.seekg(0);
  ifs_.read(reinterpret_cast<char*>(data.data()), data.size());
  ifs_.seekg(pos);
  return data;
}

}

