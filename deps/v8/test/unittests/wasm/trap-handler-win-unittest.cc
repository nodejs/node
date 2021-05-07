// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "include/v8.h"
#include "src/base/page-allocator.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/allocation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if V8_TRAP_HANDLER_SUPPORTED

bool g_handler_got_executed = false;
// The start address of the virtual memory we use to cause an exception.
i::Address g_start_address;

// When using V8::EnableWebAssemblyTrapHandler, we save the old one to fall back
// on if V8 doesn't handle the exception. This allows tools like ASan to
// register a handler early on during the process startup and still generate
// stack traces on failures.
class ExceptionHandlerFallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Register this handler as the last handler.
    registered_handler_ = AddVectoredExceptionHandler(/*first=*/0, TestHandler);
    CHECK_NOT_NULL(registered_handler_);

    v8::PageAllocator* page_allocator = i::GetPlatformPageAllocator();
    // We only need a single page.
    size_t size = page_allocator->AllocatePageSize();
    void* hint = page_allocator->GetRandomMmapAddr();
    i::VirtualMemory mem(page_allocator, size, hint, size);
    g_start_address = mem.address();
    // Set the permissions of the memory to no-access.
    CHECK(mem.SetPermissions(g_start_address, size,
                             v8::PageAllocator::kNoAccess));
    mem_ = std::move(mem);
  }

  void WriteToTestMemory(int value) {
    *reinterpret_cast<volatile int*>(g_start_address) = value;
  }

  int ReadFromTestMemory() {
    return *reinterpret_cast<volatile int*>(g_start_address);
  }

  void TearDown() override {
    // be a good citizen and remove the exception handler.
    ULONG result = RemoveVectoredExceptionHandler(registered_handler_);
    EXPECT_TRUE(result);
  }

 private:
  static LONG WINAPI TestHandler(EXCEPTION_POINTERS* exception) {
    g_handler_got_executed = true;
    v8::PageAllocator* page_allocator = i::GetPlatformPageAllocator();
    // Make the allocated memory accessible so that from now on memory accesses
    // do not cause an exception anymore.
    EXPECT_TRUE(i::SetPermissions(page_allocator, g_start_address,
                                  page_allocator->AllocatePageSize(),
                                  v8::PageAllocator::kReadWrite));
    // The memory access should work now, we can continue execution.
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  i::VirtualMemory mem_;
  void* registered_handler_;
};

TEST_F(ExceptionHandlerFallbackTest, DoTest) {
  constexpr bool use_default_handler = true;
  EXPECT_TRUE(v8::V8::EnableWebAssemblyTrapHandler(use_default_handler));
  // In the original test setup the test memory is protected against any kind of
  // access. Therefore the access here causes an access violation exception,
  // which should be caught by the exception handler we install above. In the
  // exception handler we change the permission of the test memory to make it
  // accessible, and then return from the exception handler to execute the
  // memory access again. This time we expect the memory access to work.
  constexpr int test_value = 42;
  WriteToTestMemory(test_value);
  EXPECT_EQ(test_value, ReadFromTestMemory());
  EXPECT_TRUE(g_handler_got_executed);
  v8::internal::trap_handler::RemoveTrapHandler();
}

#endif

}  //  namespace
