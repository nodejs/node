// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_MANUALLY_EXTERNALIZED_BUFFER_H_
#define V8_CCTEST_MANUALLY_EXTERNALIZED_BUFFER_H_

#include "src/api/api-inl.h"

namespace v8::internal::testing {

// Utility to free the allocated memory for a buffer that is manually
// externalized in a test.
struct ManuallyExternalizedBuffer {
  DirectHandle<JSArrayBuffer> buffer_;
  std::shared_ptr<v8::BackingStore> backing_store_;

  explicit ManuallyExternalizedBuffer(DirectHandle<JSArrayBuffer> buffer)
      : buffer_(buffer),
        backing_store_(v8::Utils::ToLocal(buffer_)->GetBackingStore()) {}
  ~ManuallyExternalizedBuffer() {
    // Nothing to be done. The reference to the backing store will be
    // dropped automatically.
  }
  void* backing_store() { return backing_store_->Data(); }
};

}  // namespace v8::internal::testing

#endif  // V8_CCTEST_MANUALLY_EXTERNALIZED_BUFFER_H_
