// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FLAGS_SAVE_FLAGS_H_
#define V8_FLAGS_SAVE_FLAGS_H_

#include <vector>

#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8::internal {

struct Flag;

// TODO(clemensb): Clean up usages of {SaveFlags} that only set flags for an
// entire test (since tests run in isolated processes).
class V8_NODISCARD SaveFlags {
 public:
  V8_EXPORT_PRIVATE SaveFlags();
  V8_EXPORT_PRIVATE ~SaveFlags();
  SaveFlags(const SaveFlags&) = delete;
  SaveFlags& operator=(const SaveFlags&) = delete;

 private:
  std::vector<Flag> saved_flags_;
#define FLAG_MODE_APPLY(ctype, nam, primary, cmt) ctype SAVED_##nam;
#include "src/flags/flag-definitions.h"
#undef FLAG_MODE_APPLY
};

}  // namespace v8::internal

#endif  // V8_FLAGS_SAVE_FLAGS_H_
