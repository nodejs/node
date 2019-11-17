// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_MANUALLY_EXTERNALIZED_BUFFER_H_
#define V8_CCTEST_MANUALLY_EXTERNALIZED_BUFFER_H_

#include "src/api/api-inl.h"

namespace v8 {
namespace internal {
namespace testing {

// Utility to free the allocated memory for a buffer that is manually
// externalized in a test.
struct ManuallyExternalizedBuffer {
  Handle<JSArrayBuffer> buffer_;
  v8::ArrayBuffer::Contents contents_;

  explicit ManuallyExternalizedBuffer(Handle<JSArrayBuffer> buffer)
      : buffer_(buffer),
        contents_(v8::Utils::ToLocal(buffer_)->Externalize()) {}
  ~ManuallyExternalizedBuffer() {
    contents_.Deleter()(contents_.Data(), contents_.ByteLength(),
                        contents_.DeleterData());
  }
  void* backing_store() { return contents_.Data(); }
};

}  // namespace testing
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_MANUALLY_EXTERNALIZED_BUFFER_H_
