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

#ifndef INCLUDE_PERFETTO_PROFILING_NORMALIZE_H_
#define INCLUDE_PERFETTO_PROFILING_NORMALIZE_H_

// Header only code that gets used in other projects.
// This is currently used in
// * ART
// * Bionic
// * Heapprofd
//
// DO NOT USE THE STL HERE. This gets used in parts of Bionic that do not
// use the STL.

#include <string.h>

namespace perfetto {
namespace profiling {

// Normalize cmdline in place. Stores new beginning of string in *cmdline_ptr.
// Returns new size of string (from new beginning).
// Modifies string in *cmdline_ptr.
static ssize_t NormalizeCmdLine(char** cmdline_ptr, size_t size) {
  char* cmdline = *cmdline_ptr;
  char* first_arg = static_cast<char*>(memchr(cmdline, '\0', size));
  if (first_arg == nullptr) {
    errno = EOVERFLOW;
    return -1;
  }
  // For consistency with what we do with Java app cmdlines, trim everything
  // after the @ sign of the first arg.
  char* first_at = static_cast<char*>(memchr(cmdline, '@', size));
  if (first_at != nullptr && first_at < first_arg) {
    *first_at = '\0';
    first_arg = first_at;
  }
  char* start = static_cast<char*>(
      memrchr(cmdline, '/', static_cast<size_t>(first_arg - cmdline)));
  if (start == nullptr) {
    start = cmdline;
  } else {
    // Skip the /.
    start++;
  }
  *cmdline_ptr = start;
  return first_arg - start;
}

}  // namespace profiling
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_PROFILING_NORMALIZE_H_
