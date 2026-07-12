// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DIAGNOSTICS_UNWINDER_H_
#define V8_DIAGNOSTICS_UNWINDER_H_

#include "include/v8-internal.h"

namespace v8 {

internal::Address Load(internal::Address address);

}  // namespace v8

#endif  // V8_DIAGNOSTICS_UNWINDER_H_
