// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-error.h"

namespace v8 {
namespace internal {
namespace regexp {

const char* const kErrorStrings[] = {
#define TEMPLATE(NAME, STRING) STRING,
    REGEXP_ERROR_MESSAGES(TEMPLATE)
#undef TEMPLATE
};

const char* ErrorString(Error error) {
  DCHECK_LT(error, Error::NumErrors);
  return kErrorStrings[static_cast<int>(error)];
}

}  // namespace regexp
}  // namespace internal
}  // namespace v8
