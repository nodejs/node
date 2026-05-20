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

#include <cstdint>
#include <vector>

namespace LIEF {

template<typename T>
std::vector<size_t> Section::search_all_(const T& v) const {
  std::vector<size_t> result;
  size_t pos = search(v, 0);

  if (pos == Section::npos) {
    return result;
  }

  do {
    result.push_back(pos);
    pos = search(v, pos + 1);
  } while(pos != Section::npos);

  return result;
}

}
