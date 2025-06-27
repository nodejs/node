// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SNAPSHOT_COMPRESSION_H_
#define V8_SNAPSHOT_SNAPSHOT_COMPRESSION_H_

#include "src/base/vector.h"
#include "src/snapshot/snapshot-data.h"

namespace v8 {
namespace internal {

class SnapshotCompression : public AllStatic {
 public:
  V8_EXPORT_PRIVATE static SnapshotData Compress(
      const SnapshotData* uncompressed_data);
  V8_EXPORT_PRIVATE static SnapshotData Decompress(
      base::Vector<const uint8_t> compressed_data);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SNAPSHOT_COMPRESSION_H_
