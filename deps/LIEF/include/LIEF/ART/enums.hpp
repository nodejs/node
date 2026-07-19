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
#ifndef LIEF_ART_ENUMS_H
#define LIEF_ART_ENUMS_H

namespace LIEF {
namespace ART {

enum STORAGE_MODES {
  STORAGE_UNCOMPRESSED = 0,
  STORAGE_LZ4          = 1,
  STORAGE_LZ4HC        = 2,
};

namespace ART_17 {

enum IMAGE_METHODS {
  RESOLUTION_METHOD         = 0,
  IMT_CONFLICT_METHOD       = 1,
  IMT_UNIMPLEMENTED_METHOD  = 2,
  CALLEE_SAVE_METHOD        = 3,
  REFS_ONLY_SAVE_METHOD     = 4,
  REFS_AND_ARGS_SAVE_METHOD = 5,
};

enum IMAGE_SECTIONS {
  SECTION_OBJECTS           = 0,
  SECTION_ART_FIELDS        = 1,
  SECTION_ART_METHODS       = 2,
  SECTION_INTERNED_STRINGS  = 3,
  SECTION_IMAGE_BITMAP      = 4,
};

enum IMAGE_ROOTS {
  DEX_CACHES   = 0,
  CLASS_ROOTS  = 1,
};


} // Namespace ART_17


namespace ART_29 {

using ART_17::IMAGE_METHODS;
using ART_17::IMAGE_ROOTS;

enum IMAGE_SECTIONS {
  SECTION_OBJECTS              = 0,
  SECTION_ART_FIELDS           = 1,
  SECTION_ART_METHODS          = 2,
  SECTION_RUNTIME_METHODS      = 3, // New in ART 29
  SECTION_IMT_CONFLICT_TABLES  = 4, // New in ART 29
  SECTION_DEX_CACHE_ARRAYS     = 5, // New in ART 29
  SECTION_INTERNED_STRINGS     = 6,
  SECTION_CLASS_TABLE          = 7, // New in ART 29
  SECTION_IMAGE_BITMAP         = 8,
};



} // Namespace ART_29


namespace ART_30 {

using ART_29::IMAGE_METHODS;
using ART_29::IMAGE_ROOTS;

enum IMAGE_SECTIONS {
  SECTION_OBJECTS              = 0,
  SECTION_ART_FIELDS           = 1,
  SECTION_ART_METHODS          = 2,
  SECTION_RUNTIME_METHODS      = 3,
  SECTION_IM_TABLES            = 4, // New in ART 30
  SECTION_IMT_CONFLICT_TABLES  = 5,
  SECTION_DEX_CACHE_ARRAYS     = 6,
  SECTION_INTERNED_STRINGS     = 7,
  SECTION_CLASS_TABLE          = 8,
  SECTION_IMAGE_BITMAP         = 9,
};

} // Namespace ART_30

namespace ART_44 {

using ART_30::IMAGE_SECTIONS;

enum IMAGE_METHODS {
  RESOLUTION_METHOD            = 0,
  IMT_CONFLICT_METHOD          = 1,
  IMT_UNIMPLEMENTED_METHOD     = 2,
  SAVE_ALL_CALLEE_SAVES_METHOD = 3, // New in ART 44
  SAVE_REFS_ONLY_METHOD        = 4, // New in ART 44
  SAVE_REFS_AND_ARGS_METHOD    = 5, // New in ART 44
  SAVE_EVERYTHING_METHOD       = 6, // New in ART 44
};


enum IMAGE_ROOTS {
  DEX_CACHES   = 0,
  CLASS_ROOTS  = 1,
  CLASS_LOADER = 2, // New in ART 44
};

} // Namespace ART_44


namespace ART_46 {

using ART_30::IMAGE_SECTIONS;
using ART_30::IMAGE_METHODS;
using ART_30::IMAGE_ROOTS;


} // Namespace ART_46

}
}
#endif
