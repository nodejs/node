// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bailout-reason.h"
#include "src/base/logging.h"

namespace v8 {
namespace internal {

const char* GetBailoutReason(BailoutReason reason) {
  DCHECK(reason < kLastErrorMessage);
#define ERROR_MESSAGES_TEXTS(C, T) T,
  static const char* error_messages_[] = {
      ERROR_MESSAGES_LIST(ERROR_MESSAGES_TEXTS)};
#undef ERROR_MESSAGES_TEXTS
  return error_messages_[reason];
}
}
}  // namespace v8::internal
