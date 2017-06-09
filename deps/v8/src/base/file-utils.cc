// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/file-utils.h"

#include <stdlib.h>
#include <string.h>

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

char* RelativePath(char** buffer, const char* exec_path, const char* name) {
  DCHECK(exec_path);
  int path_separator = static_cast<int>(strlen(exec_path)) - 1;
  while (path_separator >= 0 &&
         !OS::isDirectorySeparator(exec_path[path_separator])) {
    path_separator--;
  }
  if (path_separator >= 0) {
    int name_length = static_cast<int>(strlen(name));
    *buffer =
        reinterpret_cast<char*>(calloc(path_separator + name_length + 2, 1));
    *buffer[0] = '\0';
    strncat(*buffer, exec_path, path_separator + 1);
    strncat(*buffer, name, name_length);
  } else {
    *buffer = strdup(name);
  }
  return *buffer;
}

}  // namespace base
}  // namespace v8
