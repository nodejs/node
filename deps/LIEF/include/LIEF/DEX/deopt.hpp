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
#ifndef LIEF_DEX_DEOPT_TYPES_H
#define LIEF_DEX_DEOPT_TYPES_H
#include <cstdint>
#include <unordered_map>

namespace LIEF {
namespace DEX {
class Class;
class Method;

// Method Index: {dex_pc: index, ...}
using dex2dex_method_info_t = std::unordered_map<uint32_t, uint32_t>;
using dex2dex_class_info_t  = std::unordered_map<Method*, dex2dex_method_info_t>;
using dex2dex_info_t        = std::unordered_map<Class*, dex2dex_class_info_t>;

}
}

#endif
