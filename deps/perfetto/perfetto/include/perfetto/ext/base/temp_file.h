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

#ifndef INCLUDE_PERFETTO_EXT_BASE_TEMP_FILE_H_
#define INCLUDE_PERFETTO_EXT_BASE_TEMP_FILE_H_

#include <string>

#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {
namespace base {

class TempFile {
 public:
  static TempFile CreateUnlinked();
  static TempFile Create();

  TempFile(TempFile&&) noexcept;
  TempFile& operator=(TempFile&&);
  ~TempFile();

  const std::string& path() const { return path_; }
  int fd() const { return *fd_; }
  int operator*() const { return *fd_; }

  // Unlinks the file from the filesystem but keeps the fd() open.
  // It is safe to call this multiple times.
  void Unlink();

  // Releases the underlying file descriptor. Will unlink the file from the
  // filesystem if it was created via CreateUnlinked().
  ScopedFile ReleaseFD();

 private:
  TempFile();
  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;

  ScopedFile fd_;
  std::string path_;
};

class TempDir {
 public:
  static TempDir Create();

  TempDir(TempDir&&) noexcept;
  TempDir& operator=(TempDir&&);
  ~TempDir();

  const std::string& path() const { return path_; }

 private:
  TempDir();
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  std::string path_;
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_TEMP_FILE_H_
