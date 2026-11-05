// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags/save-flags.h"

#include "src/flags/flags-impl.h"
#include "src/flags/flags.h"

namespace v8::internal {

SaveFlags::SaveFlags() {
  base::Vector<Flag> all_flags = Flags();
  saved_flags_.assign(all_flags.begin(), all_flags.end());
  for (Flag& flag : all_flags) {
    // Prevent {Flag} from freeing pre-existing owned strings during the scope;
    // ownership is restored in {~SaveFlags}.
    flag.owns_ptr_ = false;
  }
// For each flag, save the current flag value.
#define FLAG_MODE_APPLY(ctype, nam, primary, cmt) \
  SAVED_##nam = v8_flags.nam.value();
#include "src/flags/flag-definitions.h"
#undef FLAG_MODE_APPLY
}

SaveFlags::~SaveFlags() {
  base::Vector<Flag> all_flags = Flags();
  DCHECK_EQ(saved_flags_.size(), all_flags.size());
  for (size_t i = 0; i < all_flags.size(); ++i) {
    all_flags[i].ReleaseDynamicAllocations();
    all_flags[i] = saved_flags_[i];
  }
// For each flag, set back the old flag value if it changed (don't write the
// flag if it didn't change, to keep TSAN happy).
#define FLAG_MODE_APPLY(ctype, nam, primary, cmt) \
  if (SAVED_##nam != v8_flags.nam.value()) {      \
    v8_flags.nam.Overwrite(SAVED_##nam);          \
  }
#include "src/flags/flag-definitions.h"  // NOLINT
#undef FLAG_MODE_APPLY
}

}  // namespace v8::internal
