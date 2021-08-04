// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/file-utils.h"

#include <stdlib.h>
#include <string.h>

#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"

namespace v8 {
namespace base {

std::unique_ptr<char[]> RelativePath(const char* exec_path, const char* name) {
  DCHECK(exec_path);
  size_t basename_start = strlen(exec_path);
  while (basename_start > 0 &&
         !OS::isDirectorySeparator(exec_path[basename_start - 1])) {
    --basename_start;
  }
  size_t name_length = strlen(name);
  auto buffer = std::make_unique<char[]>(basename_start + name_length + 1);
  if (basename_start > 0) base::Memcpy(buffer.get(), exec_path, basename_start);
  base::Memcpy(buffer.get() + basename_start, name, name_length);
  return buffer;
}

}  // namespace base
}  // namespace v8
