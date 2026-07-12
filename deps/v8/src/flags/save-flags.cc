// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/save-flags.h"

#include "src/flags/flags.h"

namespace v8::internal {

SaveFlags::SaveFlags() {
// For each flag, save the current flag value.
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) \
  SAVED_##nam = v8_flags.nam.value();
#include "src/flags/flag-definitions.h"
#undef FLAG_MODE_APPLY
}

SaveFlags::~SaveFlags() {
// For each flag, set back the old flag value if it changed (don't write the
// flag if it didn't change, to keep TSAN happy).
#define FLAG_MODE_APPLY(ftype, ctype, nam, def, cmt) \
  if (SAVED_##nam != v8_flags.nam.value()) {         \
    v8_flags.nam = SAVED_##nam;                      \
  }
#include "src/flags/flag-definitions.h"  // NOLINT
#undef FLAG_MODE_APPLY
}

}  // namespace v8::internal
