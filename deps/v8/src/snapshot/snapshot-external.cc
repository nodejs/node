// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building with external snapshots.

#include "src/base/platform/mutex.h"
#include "src/flags/flags.h"
#include "src/init/v8.h"  // for V8::Initialize
#include "src/snapshot/snapshot-source-sink.h"
#include "src/snapshot/snapshot.h"

#ifndef V8_USE_EXTERNAL_STARTUP_DATA
#error snapshot-external.cc is used only for the external snapshot build.
#endif  // V8_USE_EXTERNAL_STARTUP_DATA


namespace v8 {
namespace internal {

static base::LazyMutex external_startup_data_mutex = LAZY_MUTEX_INITIALIZER;
static v8::StartupData external_startup_blob = {nullptr, 0};
#ifdef V8_TARGET_OS_ANDROID
static bool external_startup_checksum_verified = false;
#endif

void SetSnapshotFromFile(StartupData* snapshot_blob) {
  base::MutexGuard lock_guard(external_startup_data_mutex.Pointer());
  DCHECK(snapshot_blob);
  DCHECK(snapshot_blob->data);
  DCHECK_GT(snapshot_blob->raw_size, 0);
  DCHECK(!external_startup_blob.data);
  DCHECK(Snapshot::SnapshotIsValid(snapshot_blob));
  external_startup_blob = *snapshot_blob;
#ifdef V8_TARGET_OS_ANDROID
  external_startup_checksum_verified = false;
#endif
}

bool Snapshot::ShouldVerifyChecksum(const v8::StartupData* data) {
#ifdef V8_TARGET_OS_ANDROID
  base::MutexGuard lock_guard(external_startup_data_mutex.Pointer());
  if (data != &external_startup_blob) {
    return v8_flags.verify_snapshot_checksum;
  }
  // Verify the external snapshot maximally once per process due to the
  // additional overhead.
  if (external_startup_checksum_verified) return false;
  external_startup_checksum_verified = true;
  return true;
#else
  return v8_flags.verify_snapshot_checksum;
#endif
}

const v8::StartupData* Snapshot::DefaultSnapshotBlob() {
  base::MutexGuard lock_guard(external_startup_data_mutex.Pointer());
  return &external_startup_blob;
}
}  // namespace internal
}  // namespace v8
