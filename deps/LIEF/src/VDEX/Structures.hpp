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
#ifndef LIEF_VDEX_STRUCTURES_H
#define LIEF_VDEX_STRUCTURES_H

#include <cstring>
#include "LIEF/types.hpp"

namespace LIEF {
/// Namespace related to the LIEF's VDEX module
namespace VDEX {
namespace details {
using vdex_version_t = uint32_t;

static constexpr uint8_t magic[] = { 'v', 'd', 'e', 'x' };
static constexpr vdex_version_t vdex_version = 0;

using checksum_t = uint32_t;

struct header {
  uint8_t magic[4];
  uint8_t version[4];

  uint32_t number_of_dex_files;
  uint32_t dex_size;
  uint32_t verifier_deps_size;
  uint32_t quickening_info_size;

};

// =======================
// VDEX Version 6
// ========================
namespace VDEX_6 {
using header = details::header;
static constexpr vdex_version_t vdex_version = 6;
}

// =======================
// VDEX Version 10
// ========================
namespace VDEX_10 {
using header = details::header;
static constexpr vdex_version_t vdex_version = 10;
}

// =======================
// VDEX Version 11
// ========================
namespace VDEX_11 {
using header = details::header;
static constexpr vdex_version_t vdex_version = 11;
}

class VDEX6 {
  public:
  using vdex_header                            = VDEX_6::header;
  static constexpr vdex_version_t vdex_version = VDEX_6::vdex_version;
};

class VDEX10 {
  public:
  using vdex_header                            = VDEX_10::header;
  static constexpr vdex_version_t vdex_version = VDEX_10::vdex_version;
};

class VDEX11 {
  public:
  using vdex_header                            = VDEX_11::header;
  static constexpr vdex_version_t vdex_version = VDEX_11::vdex_version;
};

} /* end namespace details */
} /* end namespace VDEX */
} /* end namespace LIEF */
#endif

