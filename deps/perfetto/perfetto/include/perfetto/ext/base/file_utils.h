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

#ifndef INCLUDE_PERFETTO_EXT_BASE_FILE_UTILS_H_
#define INCLUDE_PERFETTO_EXT_BASE_FILE_UTILS_H_

#include <stddef.h>

#include <string>

#include "perfetto/ext/base/utils.h"

namespace perfetto {
namespace base {

bool ReadFileDescriptor(int fd, std::string* out);
bool ReadFileStream(FILE* f, std::string* out);
bool ReadFile(const std::string& path, std::string* out);

// Call write until all data is written or an error is detected.
//
// man 2 write:
//   If a write() is interrupted by a signal handler before any bytes are
//   written, then the call fails with the error EINTR; if it is
//   interrupted after at least one byte has been written, the call
//   succeeds, and returns the number of bytes written.
ssize_t WriteAll(int fd, const void* buf, size_t count);

bool FlushFile(int fd);

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_FILE_UTILS_H_
