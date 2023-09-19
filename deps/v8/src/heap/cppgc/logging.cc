// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/logging.h"
#include "include/cppgc/source-location.h"

#include "src/base/logging.h"

namespace cppgc {
namespace internal {

void DCheckImpl(const char* message, const SourceLocation& loc) {
  V8_Dcheck(loc.FileName(), static_cast<int>(loc.Line()), message);
}

void FatalImpl(const char* message, const SourceLocation& loc) {
#if DEBUG
  V8_Fatal(loc.FileName(), static_cast<int>(loc.Line()), "Check failed: %s.",
           message);
#elif !defined(OFFICIAL_BUILD)
  V8_Fatal("Check failed: %s.", message);
#else
  V8_Fatal("ignored");
#endif
}

}  // namespace internal
}  // namespace cppgc
