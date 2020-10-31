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

#include <sys/stat.h>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/utils.h"

#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN) || \
    PERFETTO_BUILDFLAG(PERFETTO_COMPILER_GCC)
#include <unistd.h>
#else
#include <corecrt_io.h>
#include <io.h>
#endif

namespace perfetto {
namespace base {
namespace {
constexpr size_t kBufSize = 2048;
}

bool ReadFileDescriptor(int fd, std::string* out) {
  // Do not override existing data in string.
  size_t i = out->size();

  struct stat buf {};
  if (fstat(fd, &buf) != -1) {
    if (buf.st_size > 0)
      out->resize(i + static_cast<size_t>(buf.st_size));
  }

  ssize_t bytes_read;
  for (;;) {
    if (out->size() < i + kBufSize)
      out->resize(out->size() + kBufSize);

    bytes_read = PERFETTO_EINTR(read(fd, &((*out)[i]), kBufSize));
    if (bytes_read > 0) {
      i += static_cast<size_t>(bytes_read);
    } else {
      out->resize(i);
      return bytes_read == 0;
    }
  }
}

bool ReadFileStream(FILE* f, std::string* out) {
  return ReadFileDescriptor(fileno(f), out);
}

bool ReadFile(const std::string& path, std::string* out) {
  base::ScopedFile fd = base::OpenFile(path, O_RDONLY);
  if (!fd)
    return false;

  return ReadFileDescriptor(*fd, out);
}

ssize_t WriteAll(int fd, const void* buf, size_t count) {
  size_t written = 0;
  while (written < count) {
    ssize_t wr = PERFETTO_EINTR(
        write(fd, static_cast<const char*>(buf) + written, count - written));
    if (wr == 0)
      break;
    if (wr < 0)
      return wr;
    written += static_cast<size_t>(wr);
  }
  return static_cast<ssize_t>(written);
}

bool FlushFile(int fd) {
  PERFETTO_DCHECK(fd != 0);
#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  return !PERFETTO_EINTR(fdatasync(fd));
#elif PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  return !PERFETTO_EINTR(_commit(fd));
#else
  return !PERFETTO_EINTR(fsync(fd));
#endif
}

}  // namespace base
}  // namespace perfetto
