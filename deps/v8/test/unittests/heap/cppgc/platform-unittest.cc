// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/platform.h"

#include "src/base/logging.h"
#include "src/base/page-allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

TEST(FatalOutOfMemoryHandlerDeathTest, DefaultHandlerCrashes) {
  FatalOutOfMemoryHandler handler;
  EXPECT_DEATH_IF_SUPPORTED(handler(), "");
}

namespace {

constexpr uintptr_t kHeapNeedle = 0x14;

[[noreturn]] void CustomHandler(const std::string&, const SourceLocation&,
                                HeapBase* heap) {
  if (heap == reinterpret_cast<HeapBase*>(kHeapNeedle)) {
    GRACEFUL_FATAL("cust0m h4ndl3r with matching heap");
  }
  GRACEFUL_FATAL("cust0m h4ndl3r");
}

}  // namespace

TEST(FatalOutOfMemoryHandlerDeathTest, CustomHandlerCrashes) {
  FatalOutOfMemoryHandler handler;
  handler.SetCustomHandler(&CustomHandler);
  EXPECT_DEATH_IF_SUPPORTED(handler(), "cust0m h4ndl3r");
}

TEST(FatalOutOfMemoryHandlerDeathTest, CustomHandlerWithHeapState) {
  FatalOutOfMemoryHandler handler(reinterpret_cast<HeapBase*>(kHeapNeedle));
  handler.SetCustomHandler(&CustomHandler);
  EXPECT_DEATH_IF_SUPPORTED(handler(), "cust0m h4ndl3r with matching heap");
}

}  // namespace internal
}  // namespace cppgc
