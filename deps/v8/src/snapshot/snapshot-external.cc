// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building with external snapshots.

#include "src/snapshot/snapshot.h"

#include "src/base/platform/mutex.h"
#include "src/init/v8.h"  // for V8::Initialize
#include "src/snapshot/snapshot-source-sink.h"

#ifndef V8_USE_EXTERNAL_STARTUP_DATA
#error snapshot-external.cc is used only for the external snapshot build.
#endif  // V8_USE_EXTERNAL_STARTUP_DATA


namespace v8 {
namespace internal {

static base::LazyMutex external_startup_data_mutex = LAZY_MUTEX_INITIALIZER;
static v8::StartupData external_startup_blob = {nullptr, 0};

void SetSnapshotFromFile(StartupData* snapshot_blob) {
  base::MutexGuard lock_guard(external_startup_data_mutex.Pointer());
  DCHECK(snapshot_blob);
  DCHECK(snapshot_blob->data);
  DCHECK_GT(snapshot_blob->raw_size, 0);
  DCHECK(!external_startup_blob.data);
  DCHECK(Snapshot::SnapshotIsValid(snapshot_blob));
  external_startup_blob = *snapshot_blob;
}


const v8::StartupData* Snapshot::DefaultSnapshotBlob() {
  base::MutexGuard lock_guard(external_startup_data_mutex.Pointer());
  return &external_startup_blob;
}
}  // namespace internal
}  // namespace v8
