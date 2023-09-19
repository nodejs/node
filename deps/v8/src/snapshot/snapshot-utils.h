// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SNAPSHOT_UTILS_H_
#define V8_SNAPSHOT_SNAPSHOT_UTILS_H_

#include "src/base/vector.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

V8_EXPORT_PRIVATE uint32_t Checksum(base::Vector<const byte> payload);

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SNAPSHOT_UTILS_H_
