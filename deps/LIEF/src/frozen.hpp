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
#ifndef LIEF_FROZEN_H
#define LIEF_FROZEN_H
#include "LIEF/config.h"
#include "compiler_support.h"

#ifdef LIEF_FROZEN_ENABLED
# include <frozen/map.h>
# define CONST_MAP(KEY, VAL, NUM) constexpr frozen::map<KEY, VAL, NUM>
# define STRING_MAP constexpr frozen::map
# define CONST_MAP_ALT constexpr frozen::map
namespace frozen {
template <class Key, class Value, class... Args>
map(std::pair<Key, Value>, Args...) -> map<Key, Value, sizeof...(Args) + 1>;
}
#else // !LIEF_FROZEN_ENABLED
# include <unordered_map>
# define CONST_MAP(KEY, VAL, NUM) static const std::unordered_map<KEY, VAL>
# define STRING_MAP static const std::unordered_map
# define CONST_MAP_ALT static const std::unordered_map
#endif

#endif
