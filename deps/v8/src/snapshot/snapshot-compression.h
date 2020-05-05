// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SNAPSHOT_COMPRESSION_H_
#define V8_SNAPSHOT_SNAPSHOT_COMPRESSION_H_

#include "src/snapshot/serializer-common.h"
#include "src/snapshot/serializer.h"
#include "src/snapshot/snapshot.h"
#include "src/utils/vector.h"

namespace v8 {
namespace internal {

class SnapshotCompression : public AllStatic {
 public:
  V8_EXPORT_PRIVATE static SnapshotData Compress(
      const SnapshotData* uncompressed_data);
  V8_EXPORT_PRIVATE static SnapshotData Decompress(
      Vector<const byte> compressed_data);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SNAPSHOT_COMPRESSION_H_
