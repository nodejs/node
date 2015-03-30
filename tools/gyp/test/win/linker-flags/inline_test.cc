// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inline_test.h"

#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

bool IsFunctionInlined(void* caller_return_address) {
  return _ReturnAddress() == caller_return_address;
}
