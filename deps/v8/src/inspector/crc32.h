// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_CRC32_H_
#define V8_INSPECTOR_CRC32_H_

#include "src/inspector/string-16.h"

namespace v8_inspector {

int32_t computeCrc32(const String16&);

}

#endif  // V8_INSPECTOR_CRC32_H_
