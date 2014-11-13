// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The common functionality when building with or without snapshots.

#include "src/v8.h"

#include "src/api.h"
#include "src/base/platform/platform.h"
#include "src/serialize.h"
#include "src/snapshot.h"

namespace v8 {
namespace internal {

void Snapshot::ReserveSpaceForLinkedInSnapshot(Deserializer* deserializer) {
  deserializer->AddReservation(NEW_SPACE, new_space_used_);
  deserializer->AddReservation(OLD_POINTER_SPACE, pointer_space_used_);
  deserializer->AddReservation(OLD_DATA_SPACE, data_space_used_);
  deserializer->AddReservation(CODE_SPACE, code_space_used_);
  deserializer->AddReservation(MAP_SPACE, map_space_used_);
  deserializer->AddReservation(CELL_SPACE, cell_space_used_);
  deserializer->AddReservation(PROPERTY_CELL_SPACE, property_cell_space_used_);
  deserializer->AddReservation(LO_SPACE, lo_space_used_);
}


bool Snapshot::Initialize(Isolate* isolate) {
  if (size_ > 0) {
    base::ElapsedTimer timer;
    if (FLAG_profile_deserialization) {
      timer.Start();
    }
    SnapshotByteSource source(raw_data_, raw_size_);
    Deserializer deserializer(&source);
    ReserveSpaceForLinkedInSnapshot(&deserializer);
    bool success = isolate->Init(&deserializer);
    if (FLAG_profile_deserialization) {
      double ms = timer.Elapsed().InMillisecondsF();
      PrintF("[Snapshot loading and deserialization took %0.3f ms]\n", ms);
    }
    return success;
  }
  return false;
}


bool Snapshot::HaveASnapshotToStartFrom() {
  return size_ != 0;
}


Handle<Context> Snapshot::NewContextFromSnapshot(Isolate* isolate) {
  if (context_size_ == 0) {
    return Handle<Context>();
  }
  SnapshotByteSource source(context_raw_data_,
                            context_raw_size_);
  Deserializer deserializer(&source);
  Object* root;
  deserializer.AddReservation(NEW_SPACE, context_new_space_used_);
  deserializer.AddReservation(OLD_POINTER_SPACE, context_pointer_space_used_);
  deserializer.AddReservation(OLD_DATA_SPACE, context_data_space_used_);
  deserializer.AddReservation(CODE_SPACE, context_code_space_used_);
  deserializer.AddReservation(MAP_SPACE, context_map_space_used_);
  deserializer.AddReservation(CELL_SPACE, context_cell_space_used_);
  deserializer.AddReservation(PROPERTY_CELL_SPACE,
                              context_property_cell_space_used_);
  deserializer.AddReservation(LO_SPACE, context_lo_space_used_);
  deserializer.DeserializePartial(isolate, &root);
  CHECK(root->IsContext());
  return Handle<Context>(Context::cast(root));
}


#ifdef V8_USE_EXTERNAL_STARTUP_DATA
// Dummy implementations of Set*FromFile(..) APIs.
//
// These are meant for use with snapshot-external.cc. Should this file
// be compiled with those options we just supply these dummy implementations
// below. This happens when compiling the mksnapshot utility.
void SetNativesFromFile(StartupData* data) { CHECK(false); }
void SetSnapshotFromFile(StartupData* data) { CHECK(false); }
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

} }  // namespace v8::internal
