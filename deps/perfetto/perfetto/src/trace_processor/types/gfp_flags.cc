/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/types/gfp_flags.h"

#include <array>

namespace perfetto {
namespace trace_processor {

namespace {

struct Flag {
  uint64_t mask;
  const char* flag_name;
};

using FlagArray = std::array<Flag, 37>;

constexpr FlagArray v3_4 = {
    {{(((0x10u) | (0x40u) | (0x80u) | (0x20000u) | (0x02u) | (0x08u)) |
       (0x4000u) | (0x10000u) | (0x1000u) | (0x200u) | (0x400000u)),
      "GFP_TRANSHUGE"},
     {((0x10u) | (0x40u) | (0x80u) | (0x20000u) | (0x02u) | (0x08u)),
      "GFP_HIGHUSER_MOVABLE"},
     {((0x10u) | (0x40u) | (0x80u) | (0x20000u) | (0x02u)), "GFP_HIGHUSER"},
     {((0x10u) | (0x40u) | (0x80u) | (0x20000u)), "GFP_USER"},
     {((0x10u) | (0x40u) | (0x80u) | (0x80000u)), "GFP_TEMPORARY"},
     {((0x10u) | (0x40u) | (0x80u)), "GFP_KERNEL"},
     {((0x10u) | (0x40u)), "GFP_NOFS"},
     {((0x20u)), "GFP_ATOMIC"},
     {((0x10u)), "GFP_NOIO"},
     {(0x20u), "GFP_HIGH"},
     {(0x10u), "GFP_WAIT"},
     {(0x40u), "GFP_IO"},
     {(0x100u), "GFP_COLD"},
     {(0x200u), "GFP_NOWARN"},
     {(0x400u), "GFP_REPEAT"},
     {(0x800u), "GFP_NOFAIL"},
     {(0x1000u), "GFP_NORETRY"},
     {(0x4000u), "GFP_COMP"},
     {(0x8000u), "GFP_ZERO"},
     {(0x10000u), "GFP_NOMEMALLOC"},
     {(0x20000u), "GFP_HARDWALL"},
     {(0x40000u), "GFP_THISNODE"},
     {(0x80000u), "GFP_RECLAIMABLE"},
     {(0x08u), "GFP_MOVABLE"},
     {(0), "GFP_NOTRACK"},
     {(0x400000u), "GFP_NO_KSWAPD"},
     {(0x800000u), "GFP_OTHER_NODE"},
     {0, nullptr}}};

constexpr FlagArray v3_10 = {
    {{(((0x10u) | (0x40u) | (0x80u) | (0x20000u) | (0x02u) | (0x08u)) |
       (0x4000u) | (0x10000u) | (0x1000u) | (0x200u) | (0x400000u)),
      "GFP_TRANSHUGE"},
     {((0x10u) | (0x40u) | (0x80u) | (0x20000u) | (0x02u) | (0x08u)),
      "GFP_HIGHUSER_MOVABLE"},
     {((0x10u) | (0x40u) | (0x80u) | (0x20000u) | (0x02u)), "GFP_HIGHUSER"},
     {((0x10u) | (0x40u) | (0x80u) | (0x20000u)), "GFP_USER"},
     {((0x10u) | (0x40u) | (0x80u) | (0x80000u)), "GFP_TEMPORARY"},
     {((0x10u) | (0x40u) | (0x80u)), "GFP_KERNEL"},
     {((0x10u) | (0x40u)), "GFP_NOFS"},
     {((0x20u)), "GFP_ATOMIC"},
     {((0x10u)), "GFP_NOIO"},
     {(0x20u), "GFP_HIGH"},
     {(0x10u), "GFP_WAIT"},
     {(0x40u), "GFP_IO"},
     {(0x100u), "GFP_COLD"},
     {(0x200u), "GFP_NOWARN"},
     {(0x400u), "GFP_REPEAT"},
     {(0x800u), "GFP_NOFAIL"},
     {(0x1000u), "GFP_NORETRY"},
     {(0x4000u), "GFP_COMP"},
     {(0x8000u), "GFP_ZERO"},
     {(0x10000u), "GFP_NOMEMALLOC"},
     {(0x2000u), "GFP_MEMALLOC"},
     {(0x20000u), "GFP_HARDWALL"},
     {(0x40000u), "GFP_THISNODE"},
     {(0x80000u), "GFP_RECLAIMABLE"},
     {(0x100000u), "GFP_KMEMCG"},
     {(0x08u), "GFP_MOVABLE"},
     {(0x200000u), "GFP_NOTRACK"},
     {(0x400000u), "GFP_NO_KSWAPD"},
     {(0x800000u), "GFP_OTHER_NODE"},
     {0, nullptr}}};

constexpr FlagArray v4_4 = {
    {{(((((((0x400000u | 0x2000000u)) | (0x40u) | (0x80u) | (0x20000u)) |
          (0x02u)) |
         (0x08u)) |
        (0x4000u) | (0x10000u) | (0x1000u) | (0x200u)) &
       ~(0x2000000u)),
      "GFP_TRANSHUGE"},
     {(((((0x400000u | 0x2000000u)) | (0x40u) | (0x80u) | (0x20000u)) |
        (0x02u)) |
       (0x08u)),
      "GFP_HIGHUSER_MOVABLE"},
     {((((0x400000u | 0x2000000u)) | (0x40u) | (0x80u) | (0x20000u)) | (0x02u)),
      "GFP_HIGHUSER"},
     {(((0x400000u | 0x2000000u)) | (0x40u) | (0x80u) | (0x20000u)),
      "GFP_USER"},
     {(((0x400000u | 0x2000000u)) | (0x40u) | (0x80u) | (0x10u)),
      "GFP_TEMPORARY"},
     {(((0x400000u | 0x2000000u)) | (0x40u) | (0x80u)), "GFP_KERNEL"},
     {(((0x400000u | 0x2000000u)) | (0x40u)), "GFP_NOFS"},
     {((0x20u) | (0x80000u) | (0x2000000u)), "GFP_ATOMIC"},
     {(((0x400000u | 0x2000000u))), "GFP_NOIO"},
     {(0x20u), "GFP_HIGH"},
     {(0x80000u), "GFP_ATOMIC"},
     {(0x40u), "GFP_IO"},
     {(0x100u), "GFP_COLD"},
     {(0x200u), "GFP_NOWARN"},
     {(0x400u), "GFP_REPEAT"},
     {(0x800u), "GFP_NOFAIL"},
     {(0x1000u), "GFP_NORETRY"},
     {(0x4000u), "GFP_COMP"},
     {(0x8000u), "GFP_ZERO"},
     {(0x10000u), "GFP_NOMEMALLOC"},
     {(0x2000u), "GFP_MEMALLOC"},
     {(0x20000u), "GFP_HARDWALL"},
     {(0x40000u), "GFP_THISNODE"},
     {(0x10u), "GFP_RECLAIMABLE"},
     {(0x08u), "GFP_MOVABLE"},
     {(0x200000u), "GFP_NOTRACK"},
     {(0x400000u), "GFP_DIRECT_RECLAIM"},
     {(0x2000000u), "GFP_KSWAPD_RECLAIM"},
     {(0x800000u), "GFP_OTHER_NODE"},
     {0, nullptr}}};

constexpr FlagArray v4_14 = {
    {{((((((((0x400000u | 0x1000000u)) | (0x40u) | (0x80u) | (0x20000u)) |
           (0x02u)) |
          (0x08u)) |
         (0x4000u) | (0x10000u) | (0x200u)) &
        ~((0x400000u | 0x1000000u))) |
       (0x400000u)),
      "GFP_TRANSHUGE"},
     {(((((((0x400000u | 0x1000000u)) | (0x40u) | (0x80u) | (0x20000u)) |
          (0x02u)) |
         (0x08u)) |
        (0x4000u) | (0x10000u) | (0x200u)) &
       ~((0x400000u | 0x1000000u))),
      "GFP_TRANSHUGE_LIGHT"},
     {(((((0x400000u | 0x1000000u)) | (0x40u) | (0x80u) | (0x20000u)) |
        (0x02u)) |
       (0x08u)),
      "GFP_HIGHUSER_MOVABLE"},
     {((((0x400000u | 0x1000000u)) | (0x40u) | (0x80u) | (0x20000u)) | (0x02u)),
      "GFP_HIGHUSER"},
     {(((0x400000u | 0x1000000u)) | (0x40u) | (0x80u) | (0x20000u)),
      "GFP_USER"},
     {((((0x400000u | 0x1000000u)) | (0x40u) | (0x80u)) | (0x100000u)),
      "GFP_KERNEL_ACCOUNT"},
     {(((0x400000u | 0x1000000u)) | (0x40u) | (0x80u)), "GFP_KERNEL"},
     {(((0x400000u | 0x1000000u)) | (0x40u)), "GFP_NOFS"},
     {((0x20u) | (0x80000u) | (0x1000000u)), "GFP_ATOMIC"},
     {(((0x400000u | 0x1000000u))), "GFP_NOIO"},
     {((0x1000000u)), "GFP_NOWAIT"},
     {(0x01u), "GFP_DMA"},
     {(0x02u), "__GFP_HIGHMEM"},
     {(0x04u), "GFP_DMA32"},
     {(0x20u), "__GFP_HIGH"},
     {(0x80000u), "__GFP_ATOMIC"},
     {(0x40u), "__GFP_IO"},
     {(0x80u), "__GFP_FS"},
     {(0x100u), "__GFP_COLD"},
     {(0x200u), "__GFP_NOWARN"},
     {(0x400u), "__GFP_RETRY_MAYFAIL"},
     {(0x800u), "__GFP_NOFAIL"},
     {(0x1000u), "__GFP_NORETRY"},
     {(0x4000u), "__GFP_COMP"},
     {(0x8000u), "__GFP_ZERO"},
     {(0x10000u), "__GFP_NOMEMALLOC"},
     {(0x2000u), "__GFP_MEMALLOC"},
     {(0x20000u), "__GFP_HARDWALL"},
     {(0x40000u), "__GFP_THISNODE"},
     {(0x10u), "__GFP_RECLAIMABLE"},
     {(0x08u), "__GFP_MOVABLE"},
     {(0x100000u), "__GFP_ACCOUNT"},
     {(0x800000u), "__GFP_WRITE"},
     {((0x400000u | 0x1000000u)), "__GFP_RECLAIM"},
     {(0x400000u), "__GFP_DIRECT_RECLAIM"},
     {(0x1000000u), "__GFP_KSWAPD_RECLAIM"},
     {0, nullptr}}};

// Get the bitmask closest to the kernel version. For versions less than 3.4
// and greater than 4.14 this may end up being inaccurate.
const FlagArray* GetBitmaskVersion(std::tuple<int32_t, int32_t> version) {
  if (version < std::make_tuple(3, 10)) {
    return &v3_4;
  } else if (version >= std::make_tuple(3, 10) &&
             version < std::make_tuple(4, 4)) {
    return &v3_10;
  } else if (version >= std::make_tuple(4, 4) &&
             version < std::make_tuple(4, 14)) {
    return &v4_4;
  } else {  // version >= 4.14
    // TODO(taylori): Add newer kernel versions once we have access to them.
    return &v4_14;
  }
}
}  // namespace

void WriteGfpFlag(uint64_t value,
                  std::tuple<uint32_t, uint32_t> version,
                  base::StringWriter* writer) {
  // On all kernel versions if this flag is not set, return GFP_NOWAIT.
  if (value == 0)
    writer->AppendString("GFP_NOWAIT");

  std::string result;
  const FlagArray* bitmasks = GetBitmaskVersion(version);

  // Based on trace_print_flags_seq() in the kernel.
  size_t i = 0;
  while (bitmasks->at(i).flag_name != nullptr) {
    size_t current = i++;
    uint64_t mask = bitmasks->at(current).mask;
    const char* str = bitmasks->at(current).flag_name;

    if ((value & mask) != mask)
      continue;
    value &= ~mask;

    result += str;
    result += "|";
  }

  // Add any leftover flags.
  if (value) {
    writer->AppendString(result.c_str(), result.size());
    writer->AppendString("0x", 2);
    writer->AppendHexInt(value);
  } else {
    writer->AppendString(result.c_str(), result.size() - 1);
  }
}

}  // namespace trace_processor
}  // namespace perfetto
