// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building without snapshots.

#include "src/v8.h"

#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
// Dummy implementations of Set*FromFile(..) APIs.
//
// These are meant for use with snapshot-external.cc. Should this file
// be compiled with those options we just supply these dummy implementations
// below. This happens when compiling the mksnapshot utility.
void SetNativesFromFile(StartupData* data) { CHECK(false); }
void SetSnapshotFromFile(StartupData* data) { CHECK(false); }
void ReadNatives() {}
void DisposeNatives() {}
#endif  // V8_USE_EXTERNAL_STARTUP_DATA


const v8::StartupData* Snapshot::DefaultSnapshotBlob() { return NULL; }
} }  // namespace v8::internal
