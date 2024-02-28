// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/shared-object-conveyor-handles.h"

#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// TODO(v8:12547): Currently the shared isolate owns all the conveyors. Change
// the owner to the main isolate once the shared isolate is removed.
SharedObjectConveyorHandles::SharedObjectConveyorHandles(Isolate* isolate)
    : persistent_handles_(
          isolate->shared_space_isolate()->NewPersistentHandles()) {}

uint32_t SharedObjectConveyorHandles::Persist(
    Tagged<HeapObject> shared_object) {
  DCHECK(IsShared(shared_object));
  uint32_t id = static_cast<uint32_t>(shared_objects_.size());
  shared_objects_.push_back(persistent_handles_->NewHandle(shared_object));
  return id;
}

}  // namespace internal
}  // namespace v8
