// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_PLATFORM_LINUX_H_
#define V8_BASE_PLATFORM_PLATFORM_LINUX_H_

#include <sys/types.h>

#include <cstdint>
#include <optional>
#include <string>

#include "src/base/base-export.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

// Represents a memory region, as parsed from /proc/PID/maps.
// Visible for testing.
struct V8_BASE_EXPORT MemoryRegion {
  uintptr_t start;
  uintptr_t end;
  char raw_permissions[5];
  PagePermissions permissions;
  uintptr_t offset;
  dev_t dev;
  ino_t inode;
  static constexpr size_t kMaxPathnameSize = 128;
  char pathname[kMaxPathnameSize];
};

// A /proc/self/maps parser that is safe to use in signal handlers.
// It uses only async-signal-safe functions (open, read, close).
class V8_BASE_EXPORT SignalSafeMapsParser {
 public:
  SignalSafeMapsParser(int fd = -1, bool should_close_fd = true);
  ~SignalSafeMapsParser();

  // Returns true if the file was successfully opened.
  bool IsValid() const { return fd_ >= 0; }

  // Reads the next line and extracts the MemoryRegion from it.
  // Returns a MemoryRegion on success, or std::nullopt on EOF or error.
  std::optional<MemoryRegion> Next();

 private:
  bool ReadChar(char* out);
  bool ReadHex(uintptr_t* out_val, char* out_delim);
  bool ReadDecimal(uintptr_t* out_val, char* out_delim);

  int fd_;
  bool should_close_fd_;
  static constexpr size_t kBufferSize = 1024;
  char buffer_[kBufferSize];
  ssize_t buffer_pos_;
  ssize_t buffer_end_;
};

// The |fp| parameter is for testing, to pass a fake /proc/self/maps file.
V8_BASE_EXPORT std::vector<OS::SharedLibraryAddress> GetSharedLibraryAddresses(
    FILE* fp);

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_PLATFORM_LINUX_H_
