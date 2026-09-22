// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-array-buffer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using ArrayBufferTest = TestWithContext;

TEST_F(ArrayBufferTest, TransferWithDetachKey) {
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate(), 1);
  Local<Value> key = Symbol::New(isolate());
  ab->SetDetachKey(key);
  Local<Object> global = context()->Global();
  Local<String> property_name =
      String::NewFromUtf8Literal(isolate(), "test_ab");
  global->Set(context(), property_name, ab).ToChecked();

  {
    TryCatch try_catch(isolate());
    CHECK(TryRunJS("globalThis.test_ab.transfer()").IsEmpty());
  }

  // Didnot transfer.
  EXPECT_EQ(ab->ByteLength(), 1u);

  ab->SetDetachKey(Undefined(isolate()));
  RunJS("globalThis.test_ab.transfer()");

  // Transferred.
  EXPECT_EQ(ab->ByteLength(), 0u);
}

TEST_F(ArrayBufferTest, BackingStoreByteSpan) {
  constexpr size_t kByteLength = 8;

  std::unique_ptr<BackingStore> backing_store =
      ArrayBuffer::NewBackingStore(isolate(), kByteLength);

  std::span<uint8_t> span = backing_store->ByteSpan();

  EXPECT_EQ(span.data(), static_cast<uint8_t*>(backing_store->Data()));
  EXPECT_EQ(span.size(), backing_store->ByteLength());

  std::fill(span.begin(), span.end(), uint8_t{0xAB});
  const auto* data = static_cast<const uint8_t*>(backing_store->Data());

  for (size_t i = 0; i < kByteLength; ++i) {
    EXPECT_EQ(uint8_t{0xAB}, data[i]);
  }
}

}  // namespace
}  // namespace v8
