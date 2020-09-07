// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_OFF_THREAD_TRANSFER_HANDLE_STORAGE_H_
#define V8_HANDLES_OFF_THREAD_TRANSFER_HANDLE_STORAGE_H_

#include "src/common/assert-scope.h"
#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class OffThreadTransferHandleStorage {
 public:
  enum State { kOffThreadHandle, kRawObject, kHandle };

  inline explicit OffThreadTransferHandleStorage(
      Address* off_thread_handle_location,
      std::unique_ptr<OffThreadTransferHandleStorage> next);

  inline void ConvertFromOffThreadHandleOnFinish();

  inline void ConvertToHandleOnPublish(Isolate* isolate,
                                       DisallowHeapAllocation* no_gc);

  inline Address* handle_location() const;

  OffThreadTransferHandleStorage* next() { return next_.get(); }

  State state() const { return state_; }

 private:
  inline void CheckValid() const;

  union {
    Address* handle_location_;
    Address raw_obj_ptr_;
  };
  std::unique_ptr<OffThreadTransferHandleStorage> next_;
  State state_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_OFF_THREAD_TRANSFER_HANDLE_STORAGE_H_
