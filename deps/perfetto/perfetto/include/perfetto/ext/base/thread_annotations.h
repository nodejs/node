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

#ifndef INCLUDE_PERFETTO_EXT_BASE_THREAD_ANNOTATIONS_H_
#define INCLUDE_PERFETTO_EXT_BASE_THREAD_ANNOTATIONS_H_

#include "perfetto/base/build_config.h"

// Windows TSAN doesn't currently support these annotations.
#if defined(THREAD_SANITIZER) && !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
extern "C" {
void AnnotateBenignRaceSized(const char* file,
                             int line,
                             unsigned long address,
                             unsigned long size,
                             const char* description);
}

#define PERFETTO_ANNOTATE_BENIGN_RACE_SIZED(pointer, size, description)   \
  AnnotateBenignRaceSized(__FILE__, __LINE__,                             \
                          reinterpret_cast<unsigned long>(pointer), size, \
                          description);
#else  // defined(ADDRESS_SANITIZER)
#define PERFETTO_ANNOTATE_BENIGN_RACE_SIZED(pointer, size, description)
#endif  // defined(ADDRESS_SANITIZER)

#endif  // INCLUDE_PERFETTO_EXT_BASE_THREAD_ANNOTATIONS_H_
