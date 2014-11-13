// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building with external snapshots.

#include "src/snapshot.h"

#include "src/serialize.h"
#include "src/snapshot-source-sink.h"
#include "src/v8.h"  // for V8::Initialize

namespace v8 {
namespace internal {


struct SnapshotImpl {
 public:
  const byte* data;
  int size;
  int new_space_used;
  int pointer_space_used;
  int data_space_used;
  int code_space_used;
  int map_space_used;
  int cell_space_used;
  int property_cell_space_used;
  int lo_space_used;

  const byte* context_data;
  int context_size;
  int context_new_space_used;
  int context_pointer_space_used;
  int context_data_space_used;
  int context_code_space_used;
  int context_map_space_used;
  int context_cell_space_used;
  int context_property_cell_space_used;
  int context_lo_space_used;
};


static SnapshotImpl* snapshot_impl_ = NULL;


bool Snapshot::HaveASnapshotToStartFrom() {
  return snapshot_impl_ != NULL;
}


bool Snapshot::Initialize(Isolate* isolate) {
  if (!HaveASnapshotToStartFrom())
    return false;

  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization) {
    timer.Start();
  }
  SnapshotByteSource source(snapshot_impl_->data, snapshot_impl_->size);
  Deserializer deserializer(&source);
  deserializer.AddReservation(NEW_SPACE, snapshot_impl_->new_space_used);
  deserializer.AddReservation(OLD_POINTER_SPACE,
                              snapshot_impl_->pointer_space_used);
  deserializer.AddReservation(OLD_DATA_SPACE, snapshot_impl_->data_space_used);
  deserializer.AddReservation(CODE_SPACE, snapshot_impl_->code_space_used);
  deserializer.AddReservation(MAP_SPACE, snapshot_impl_->map_space_used);
  deserializer.AddReservation(CELL_SPACE, snapshot_impl_->cell_space_used);
  deserializer.AddReservation(PROPERTY_CELL_SPACE,
                              snapshot_impl_->property_cell_space_used);
  deserializer.AddReservation(LO_SPACE, snapshot_impl_->lo_space_used);
  bool success = isolate->Init(&deserializer);
  if (FLAG_profile_deserialization) {
    double ms = timer.Elapsed().InMillisecondsF();
    PrintF("[Snapshot loading and deserialization took %0.3f ms]\n", ms);
  }
  return success;
}


Handle<Context> Snapshot::NewContextFromSnapshot(Isolate* isolate) {
  if (!HaveASnapshotToStartFrom())
    return Handle<Context>();

  SnapshotByteSource source(snapshot_impl_->context_data,
                            snapshot_impl_->context_size);
  Deserializer deserializer(&source);
  deserializer.AddReservation(NEW_SPACE,
                              snapshot_impl_->context_new_space_used);
  deserializer.AddReservation(OLD_POINTER_SPACE,
                              snapshot_impl_->context_pointer_space_used);
  deserializer.AddReservation(OLD_DATA_SPACE,
                              snapshot_impl_->context_data_space_used);
  deserializer.AddReservation(CODE_SPACE,
                              snapshot_impl_->context_code_space_used);
  deserializer.AddReservation(MAP_SPACE,
                              snapshot_impl_->context_map_space_used);
  deserializer.AddReservation(CELL_SPACE,
                              snapshot_impl_->context_cell_space_used);
  deserializer.AddReservation(PROPERTY_CELL_SPACE,
                              snapshot_impl_->context_property_cell_space_used);
  deserializer.AddReservation(LO_SPACE, snapshot_impl_->context_lo_space_used);
  Object* root;
  deserializer.DeserializePartial(isolate, &root);
  CHECK(root->IsContext());
  return Handle<Context>(Context::cast(root));
}


void SetSnapshotFromFile(StartupData* snapshot_blob) {
  DCHECK(snapshot_blob);
  DCHECK(snapshot_blob->data);
  DCHECK(snapshot_blob->raw_size > 0);
  DCHECK(!snapshot_impl_);

  snapshot_impl_ = new SnapshotImpl;
  SnapshotByteSource source(reinterpret_cast<const byte*>(snapshot_blob->data),
                            snapshot_blob->raw_size);

  bool success = source.GetBlob(&snapshot_impl_->data,
                                &snapshot_impl_->size);
  snapshot_impl_->new_space_used = source.GetInt();
  snapshot_impl_->pointer_space_used = source.GetInt();
  snapshot_impl_->data_space_used = source.GetInt();
  snapshot_impl_->code_space_used = source.GetInt();
  snapshot_impl_->map_space_used = source.GetInt();
  snapshot_impl_->cell_space_used = source.GetInt();
  snapshot_impl_->property_cell_space_used = source.GetInt();
  snapshot_impl_->lo_space_used = source.GetInt();

  success &= source.GetBlob(&snapshot_impl_->context_data,
                            &snapshot_impl_->context_size);
  snapshot_impl_->context_new_space_used = source.GetInt();
  snapshot_impl_->context_pointer_space_used = source.GetInt();
  snapshot_impl_->context_data_space_used = source.GetInt();
  snapshot_impl_->context_code_space_used = source.GetInt();
  snapshot_impl_->context_map_space_used = source.GetInt();
  snapshot_impl_->context_cell_space_used = source.GetInt();
  snapshot_impl_->context_property_cell_space_used = source.GetInt();
  snapshot_impl_->context_lo_space_used = source.GetInt();

  DCHECK(success);
}

} }  // namespace v8::internal
