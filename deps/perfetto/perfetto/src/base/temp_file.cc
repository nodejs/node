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

#include "perfetto/base/build_config.h"
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#include "perfetto/ext/base/temp_file.h"

#include <stdlib.h>
#include <unistd.h>

namespace perfetto {
namespace base {

namespace {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
constexpr char kSysTmpPath[] = "/data/local/tmp";
#else
constexpr char kSysTmpPath[] = "/tmp";
#endif
}  // namespace

// static
TempFile TempFile::Create() {
  TempFile temp_file;
  const char* tmpdir = getenv("TMPDIR");
  if (tmpdir) {
    temp_file.path_.assign(tmpdir);
  } else {
    temp_file.path_.assign(kSysTmpPath);
  }
  temp_file.path_.append("/perfetto-XXXXXXXX");
  temp_file.fd_.reset(mkstemp(&temp_file.path_[0]));
  if (PERFETTO_UNLIKELY(!temp_file.fd_)) {
    PERFETTO_FATAL("Could not create temp file %s", temp_file.path_.c_str());
  }
  return temp_file;
}

// static
TempFile TempFile::CreateUnlinked() {
  TempFile temp_file = TempFile::Create();
  temp_file.Unlink();
  return temp_file;
}

TempFile::TempFile() = default;

TempFile::~TempFile() {
  Unlink();
}

ScopedFile TempFile::ReleaseFD() {
  Unlink();
  return std::move(fd_);
}

void TempFile::Unlink() {
  if (path_.empty())
    return;
  PERFETTO_CHECK(unlink(path_.c_str()) == 0);
  path_.clear();
}

TempFile::TempFile(TempFile&&) noexcept = default;
TempFile& TempFile::operator=(TempFile&&) = default;

// static
TempDir TempDir::Create() {
  TempDir temp_dir;
  temp_dir.path_.assign(kSysTmpPath);
  temp_dir.path_.append("/perfetto-XXXXXXXX");
  PERFETTO_CHECK(mkdtemp(&temp_dir.path_[0]));
  return temp_dir;
}

TempDir::TempDir() = default;

TempDir::~TempDir() {
  PERFETTO_CHECK(rmdir(path_.c_str()) == 0);
}

}  // namespace base
}  // namespace perfetto


#endif  // !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
