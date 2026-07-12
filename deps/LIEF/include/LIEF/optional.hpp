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
#ifndef LIEF_OPTIONAL_H
#define LIEF_OPTIONAL_H
#include "LIEF/errors.hpp"

namespace LIEF {

template<class T>
class optional : public result<T> {
  public:
  using result<T>::result;

  optional() :
    result<T>(tl::make_unexpected(lief_errors::not_found))
  {}

  void reset() noexcept {
    new (this) optional();
  }
};

inline tl::unexpected<lief_errors> nullopt() {
  return make_error_code(lief_errors::not_found);
}

}

#endif
