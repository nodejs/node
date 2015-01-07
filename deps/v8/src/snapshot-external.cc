// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building with external snapshots.

#include "src/snapshot.h"

#include "src/serialize.h"
#include "src/snapshot-source-sink.h"
#include "src/v8.h"  // for V8::Initialize


#ifndef V8_USE_EXTERNAL_STARTUP_DATA
#error snapshot-external.cc is used only for the external snapshot build.
#endif  // V8_USE_EXTERNAL_STARTUP_DATA


namespace v8 {
namespace internal {

static v8::StartupData external_startup_blob = {NULL, 0};

void SetSnapshotFromFile(StartupData* snapshot_blob) {
  DCHECK(snapshot_blob);
  DCHECK(snapshot_blob->data);
  DCHECK(snapshot_blob->raw_size > 0);
  DCHECK(!external_startup_blob.data);
  // Validate snapshot blob.
  DCHECK(!Snapshot::ExtractStartupData(snapshot_blob).is_empty());
  DCHECK(!Snapshot::ExtractContextData(snapshot_blob).is_empty());
  external_startup_blob = *snapshot_blob;
}


const v8::StartupData Snapshot::SnapshotBlob() { return external_startup_blob; }
} }  // namespace v8::internal
