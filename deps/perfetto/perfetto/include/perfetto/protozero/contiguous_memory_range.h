/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_PROTOZERO_CONTIGUOUS_MEMORY_RANGE_H_
#define INCLUDE_PERFETTO_PROTOZERO_CONTIGUOUS_MEMORY_RANGE_H_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

namespace protozero {

// Keep this struct trivially constructible (no ctors, no default initializers).
struct ContiguousMemoryRange {
  uint8_t* begin;
  uint8_t* end;  // STL style: one byte past the end of the buffer.

  inline bool is_valid() const { return begin != nullptr; }
  inline void reset() { begin = nullptr; }
  inline size_t size() const { return static_cast<size_t>(end - begin); }
};

}  // namespace protozero

#endif  // INCLUDE_PERFETTO_PROTOZERO_CONTIGUOUS_MEMORY_RANGE_H_
