/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_BASE_CONTAINER_ANNOTATIONS_H_
#define INCLUDE_PERFETTO_EXT_BASE_CONTAINER_ANNOTATIONS_H_

#include "perfetto/base/build_config.h"

// Windows ASAN doesn't currently support these annotations.
#if defined(ADDRESS_SANITIZER) && !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) && \
    !defined(ADDRESS_SANITIZER_WITHOUT_INSTRUMENTATION)

#define ANNOTATE_NEW_BUFFER(buffer, capacity, new_size)                      \
  if (buffer) {                                                              \
    __sanitizer_annotate_contiguous_container(buffer, (buffer) + (capacity), \
                                              (buffer) + (capacity),         \
                                              (buffer) + (new_size));        \
  }
#define ANNOTATE_DELETE_BUFFER(buffer, capacity, old_size)                   \
  if (buffer) {                                                              \
    __sanitizer_annotate_contiguous_container(buffer, (buffer) + (capacity), \
                                              (buffer) + (old_size),         \
                                              (buffer) + (capacity));        \
  }
#define ANNOTATE_CHANGE_SIZE(buffer, capacity, old_size, new_size)           \
  if (buffer) {                                                              \
    __sanitizer_annotate_contiguous_container(buffer, (buffer) + (capacity), \
                                              (buffer) + (old_size),         \
                                              (buffer) + (new_size));        \
  }
#define ANNOTATE_CHANGE_CAPACITY(buffer, old_capacity, buffer_size, \
                                 new_capacity)                      \
  ANNOTATE_DELETE_BUFFER(buffer, old_capacity, buffer_size);        \
  ANNOTATE_NEW_BUFFER(buffer, new_capacity, buffer_size);
// Annotations require buffers to begin on an 8-byte boundary.
#else  // defined(ADDRESS_SANITIZER)
#define ANNOTATE_NEW_BUFFER(buffer, capacity, new_size)
#define ANNOTATE_DELETE_BUFFER(buffer, capacity, old_size)
#define ANNOTATE_CHANGE_SIZE(buffer, capacity, old_size, new_size)
#define ANNOTATE_CHANGE_CAPACITY(buffer, old_capacity, buffer_size, \
                                 new_capacity)
#endif  // defined(ADDRESS_SANITIZER)

#endif  // INCLUDE_PERFETTO_EXT_BASE_CONTAINER_ANNOTATIONS_H_
