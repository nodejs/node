// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_OFF_THREAD_TRANSFER_HANDLE_STORAGE_INL_H_
#define V8_HANDLES_OFF_THREAD_TRANSFER_HANDLE_STORAGE_INL_H_

#include "src/handles/handles-inl.h"
#include "src/handles/off-thread-transfer-handle-storage.h"

namespace v8 {
namespace internal {

OffThreadTransferHandleStorage::OffThreadTransferHandleStorage(
    Address* off_thread_handle_location,
    std::unique_ptr<OffThreadTransferHandleStorage> next)
    : handle_location_(off_thread_handle_location),
      next_(std::move(next)),
      state_(kOffThreadHandle) {
  CheckValid();
}

void OffThreadTransferHandleStorage::ConvertFromOffThreadHandleOnFinish() {
  CheckValid();
  DCHECK_EQ(state_, kOffThreadHandle);
  raw_obj_ptr_ = *handle_location_;
  state_ = kRawObject;
  CheckValid();
}

void OffThreadTransferHandleStorage::ConvertToHandleOnPublish(
    Isolate* isolate, DisallowHeapAllocation* no_gc) {
  CheckValid();
  DCHECK_EQ(state_, kRawObject);
  handle_location_ = handle(Object(raw_obj_ptr_), isolate).location();
  state_ = kHandle;
  CheckValid();
}

Address* OffThreadTransferHandleStorage::handle_location() const {
  CheckValid();
  DCHECK_EQ(state_, kHandle);
  return handle_location_;
}

void OffThreadTransferHandleStorage::CheckValid() const {
#ifdef DEBUG
  Object obj;

  switch (state_) {
    case kHandle:
    case kOffThreadHandle:
      DCHECK_NOT_NULL(handle_location_);
      obj = Object(*handle_location_);
      break;
    case kRawObject:
      obj = Object(raw_obj_ptr_);
      break;
  }

  // Smis are always fine.
  if (obj.IsSmi()) return;

  // The main-thread handle should not be in off-thread space, and vice verse.
  // Raw object pointers can point to the main-thread heap during Publish, so
  // we don't check that.
  DCHECK_IMPLIES(state_ == kOffThreadHandle,
                 Heap::InOffThreadSpace(HeapObject::cast(obj)));
  DCHECK_IMPLIES(state_ == kHandle,
                 !Heap::InOffThreadSpace(HeapObject::cast(obj)));
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_OFF_THREAD_TRANSFER_HANDLE_STORAGE_INL_H_
