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
#ifndef LIEF_DWARF_ENUMS_H
#define LIEF_DWARF_ENUMS_H

namespace LIEF {
namespace dwarf {

 enum class EH_ENCODING  {
    ABSPTR   = 0x00,
    OMIT     = 0xff,
    ULEB128  = 0x01,
    UDATA2   = 0x02,
    UDATA4   = 0x03,
    UDATA8   = 0x04,
    SLEB128  = 0x09,
    SDATA2   = 0x0a,
    SDATA4   = 0x0b,
    SDATA8   = 0x0c,
    SIGNED   = 0x09,

    PCREL    = 0x10,
    INDIRECT = 0x80,
    TEXTREL  = 0x20,
    DATAREL  = 0x30,
    FUNCREL  = 0x40,
    ALIGNED  = 0x50,
 };

} // dwarf
} // LIEF

#endif
