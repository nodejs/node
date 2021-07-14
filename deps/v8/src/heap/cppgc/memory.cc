// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/memory.h"

#include <cstddef>

#include "src/heap/cppgc/globals.h"

namespace cppgc {
namespace internal {

void NoSanitizeMemset(void* address, char c, size_t bytes) {
  volatile Address base = reinterpret_cast<Address>(address);
  for (size_t i = 0; i < bytes; ++i) {
    base[i] = c;
  }
}

}  // namespace internal
}  // namespace cppgc
