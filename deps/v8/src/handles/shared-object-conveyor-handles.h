// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_SHARED_OBJECT_CONVEYOR_HANDLES_H_
#define V8_HANDLES_SHARED_OBJECT_CONVEYOR_HANDLES_H_

#include <memory>
#include <vector>

#include "src/handles/persistent-handles.h"

namespace v8 {
namespace internal {

class PersistentHandles;

// Wrapper around PersistentHandles that is used to convey shared objects
// (i.e. keep them alive) from a ValueSerializer to a ValueDeserializer for APIs
// like postMessage.
//
// The conveyor must be allocated in an isolate that remains alive until the
// ValueDeserializer in the receiving isolate finishes processing the message.
//
// Each shared object gets an id that is stable across GCs.
//
// The embedder owns the lifetime of instances of this class. See
// v8::SharedValueConveyor.
class SharedObjectConveyorHandles {
 public:
  explicit SharedObjectConveyorHandles(Isolate* isolate);

  SharedObjectConveyorHandles(const SharedObjectConveyorHandles&) = delete;
  SharedObjectConveyorHandles& operator=(const SharedObjectConveyorHandles&) =
      delete;

  uint32_t Persist(HeapObject shared_object);

  bool HasPersisted(uint32_t object_id) const {
    return object_id < shared_objects_.size();
  }

  HeapObject GetPersisted(uint32_t object_id) const {
    DCHECK(HasPersisted(object_id));
    return *shared_objects_[object_id];
  }

 private:
  std::unique_ptr<PersistentHandles> persistent_handles_;
  std::vector<Handle<HeapObject>> shared_objects_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_SHARED_OBJECT_CONVEYOR_HANDLES_H_
