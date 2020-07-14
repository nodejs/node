// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/off-thread-isolate.h"

#include "src/execution/isolate.h"
#include "src/execution/thread-id.h"
#include "src/handles/handles-inl.h"
#include "src/logging/off-thread-logger.h"

namespace v8 {
namespace internal {

class OffThreadTransferHandleStorage {
 public:
  enum State { kOffThreadHandle, kRawObject, kHandle };

  explicit OffThreadTransferHandleStorage(
      Address* off_thread_handle_location,
      std::unique_ptr<OffThreadTransferHandleStorage> next)
      : handle_location_(off_thread_handle_location),
        next_(std::move(next)),
        state_(kOffThreadHandle) {
    CheckValid();
  }

  void ConvertFromOffThreadHandleOnFinish() {
    CheckValid();
    DCHECK_EQ(state_, kOffThreadHandle);
    raw_obj_ptr_ = *handle_location_;
    state_ = kRawObject;
    CheckValid();
  }

  void ConvertToHandleOnPublish(Isolate* isolate) {
    CheckValid();
    DCHECK_EQ(state_, kRawObject);
    handle_location_ = handle(Object(raw_obj_ptr_), isolate).location();
    state_ = kHandle;
    CheckValid();
  }

  Address* handle_location() const {
    DCHECK_EQ(state_, kHandle);
    DCHECK(
        Object(*handle_location_).IsSmi() ||
        !Heap::InOffThreadSpace(HeapObject::cast(Object(*handle_location_))));
    return handle_location_;
  }

  OffThreadTransferHandleStorage* next() { return next_.get(); }

  State state() const { return state_; }

 private:
  void CheckValid() {
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

    // The object that is not yet in a main-thread handle should be in
    // off-thread space. Main-thread handles can still point to off-thread space
    // during Publish, so that invariant is taken care of on main-thread handle
    // access.
    DCHECK_IMPLIES(state_ != kHandle,
                   Heap::InOffThreadSpace(HeapObject::cast(obj)));
#endif
  }

  union {
    Address* handle_location_;
    Address raw_obj_ptr_;
  };
  std::unique_ptr<OffThreadTransferHandleStorage> next_;
  State state_;
};

Address* OffThreadTransferHandleBase::ToHandleLocation() const {
  return storage_ == nullptr ? nullptr : storage_->handle_location();
}

OffThreadIsolate::OffThreadIsolate(Isolate* isolate, Zone* zone)
    : HiddenOffThreadFactory(isolate),
      heap_(isolate->heap()),
      isolate_(isolate),
      logger_(new OffThreadLogger()),
      handle_zone_(zone),
      off_thread_transfer_handles_head_(nullptr) {}

OffThreadIsolate::~OffThreadIsolate() = default;

void OffThreadIsolate::FinishOffThread() {
  heap()->FinishOffThread();

  OffThreadTransferHandleStorage* storage =
      off_thread_transfer_handles_head_.get();
  while (storage != nullptr) {
    storage->ConvertFromOffThreadHandleOnFinish();
    storage = storage->next();
  }

  handle_zone_ = nullptr;
}

void OffThreadIsolate::Publish(Isolate* isolate) {
  OffThreadTransferHandleStorage* storage =
      off_thread_transfer_handles_head_.get();
  while (storage != nullptr) {
    storage->ConvertToHandleOnPublish(isolate);
    storage = storage->next();
  }

  heap()->Publish(isolate->heap());
}

int OffThreadIsolate::GetNextScriptId() { return isolate_->GetNextScriptId(); }

#if V8_SFI_HAS_UNIQUE_ID
int OffThreadIsolate::GetNextUniqueSharedFunctionInfoId() {
  return isolate_->GetNextUniqueSharedFunctionInfoId();
}
#endif  // V8_SFI_HAS_UNIQUE_ID

bool OffThreadIsolate::is_collecting_type_profile() {
  // TODO(leszeks): Figure out if it makes sense to check this asynchronously.
  return isolate_->is_collecting_type_profile();
}

void OffThreadIsolate::PinToCurrentThread() {
  DCHECK(!thread_id_.IsValid());
  thread_id_ = ThreadId::Current();
}

OffThreadTransferHandleStorage* OffThreadIsolate::AddTransferHandleStorage(
    HandleBase handle) {
  DCHECK_IMPLIES(off_thread_transfer_handles_head_ != nullptr,
                 off_thread_transfer_handles_head_->state() ==
                     OffThreadTransferHandleStorage::kOffThreadHandle);
  off_thread_transfer_handles_head_ =
      std::make_unique<OffThreadTransferHandleStorage>(
          handle.location(), std::move(off_thread_transfer_handles_head_));
  return off_thread_transfer_handles_head_.get();
}

}  // namespace internal
}  // namespace v8
