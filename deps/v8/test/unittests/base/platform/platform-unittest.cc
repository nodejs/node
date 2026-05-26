// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"

#include <cstdio>
#include <cstring>

#include "include/v8-function.h"
#include "src/base/build_config.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef V8_TARGET_OS_LINUX
#include <sys/sysmacros.h>

#include "src/base/platform/platform-linux.h"
#include "src/base/virtual-address-space.h"
#endif

#ifdef V8_OS_WIN
#include <windows.h>
#endif

namespace v8 {
namespace base {

#ifdef V8_TARGET_OS_WIN
// Alignment is constrained on Windows.
constexpr size_t kMaxPageSize = 4096;
#elif V8_HOST_ARCH_PPC64
#if defined(_AIX)
// gcc might complain about overalignment (bug):
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89357
constexpr size_t kMaxPageSize = 4096;
#else
// Native PPC linux has large (64KB) physical pages.
constexpr size_t kMaxPageSize = 65536;
#endif
#else
constexpr size_t kMaxPageSize = 16384;
#endif

alignas(kMaxPageSize) const char kArray[kMaxPageSize] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua.";

TEST(OS, GetCurrentProcessId) {
#ifdef V8_OS_POSIX
  EXPECT_EQ(static_cast<int>(getpid()), OS::GetCurrentProcessId());
#endif

#ifdef V8_OS_WIN
  EXPECT_EQ(static_cast<int>(::GetCurrentProcessId()),
            OS::GetCurrentProcessId());
#endif
}

TEST(OS, RemapPages) {
  if constexpr (OS::IsRemapPageSupported()) {
    const size_t size = base::OS::AllocatePageSize();
    ASSERT_TRUE(size <= kMaxPageSize);
    const void* data = static_cast<const void*>(kArray);

    // Target mapping.
    void* remapped_data =
        OS::Allocate(nullptr, size, base::OS::AllocatePageSize(),
                     OS::MemoryPermission::kReadWrite);
    ASSERT_TRUE(remapped_data);

    EXPECT_TRUE(OS::RemapPages(data, size, remapped_data,
                               OS::MemoryPermission::kReadExecute));
    EXPECT_EQ(0, memcmp(remapped_data, data, size));

    OS::Free(remapped_data, size);
  }
}

#ifdef V8_TARGET_OS_LINUX
TEST(OS, GetSharedLibraryAddresses) {
  FILE* fp = tmpfile();
  ASSERT_TRUE(fp);
  const char* contents =
      R"EOF(12340000-12345000 r-xp 00026000 fe:01 12583839                   /lib/x86_64-linux-gnu/libc-2.33.so
12365000-12376000 rw-p 00000000 00:00 0    [heap]
12430000-12435000 r-xp 00062000 fe:01 12583839 /path/to/SomeApplication.apk
)EOF";
  size_t length = strlen(contents);
  ASSERT_EQ(fwrite(contents, 1, length, fp), length);
  rewind(fp);

  auto shared_library_addresses = GetSharedLibraryAddresses(fp);
  EXPECT_EQ(shared_library_addresses.size(), 2u);

  EXPECT_EQ(shared_library_addresses[0].library_path,
            "/lib/x86_64-linux-gnu/libc-2.33.so");
  EXPECT_EQ(shared_library_addresses[0].start, 0x12340000u - 0x26000);

  EXPECT_EQ(shared_library_addresses[1].library_path,
            "/path/to/SomeApplication.apk");
#if defined(V8_OS_ANDROID)
  EXPECT_EQ(shared_library_addresses[1].start, 0x12430000u);
#else
  EXPECT_EQ(shared_library_addresses[1].start, 0x12430000u - 0x62000);
#endif
}

TEST(OS, SignalSafeMapsParserForCurrentProcess) {
  SignalSafeMapsParser parser;
  ASSERT_TRUE(parser.IsValid());

  int stack_var = 42;
  auto heap_var = std::make_unique<int>(42);
  uintptr_t stack_addr = reinterpret_cast<uintptr_t>(&stack_var);
  uintptr_t heap_addr = reinterpret_cast<uintptr_t>(heap_var.get());

  bool found_stack = false;
  bool found_heap = false;
  bool found_entry = false;

  while (auto entry = parser.Next()) {
    found_entry = true;
    EXPECT_LT(entry->start, entry->end);

    if (stack_addr >= entry->start && stack_addr < entry->end) {
      found_stack = true;
      EXPECT_TRUE(entry->permissions == PagePermissions::kReadWrite);
    }
    if (heap_addr >= entry->start && heap_addr < entry->end) {
      found_heap = true;
      EXPECT_TRUE(entry->permissions == PagePermissions::kReadWrite);
    }
  }
  EXPECT_TRUE(found_entry);
  EXPECT_TRUE(found_stack);
  EXPECT_TRUE(found_heap);
}

#if V8_TARGET_ARCH_64_BIT
// This test assumes a 64-bit architecture due to the 64-bit addresses.
TEST(OS, SignalSafeMapsParserForCustomMapsFile) {
  FILE* fp = tmpfile();
  ASSERT_TRUE(fp);

  const char* valid_content =
      "123456000000-123456001000 r-xp 00026000 fe:01 12583839                  "
      " /lib/x86_64-linux-gnu/libc-2.33.so\n"
      "123456100000-123456200000 r--p 00000000 103:104 22                      "
      " /usr/local/bin/node\n"
      "123456300000-123456302000 rw-p 00000000 00:00 0                         "
      " [heap]\n"
      "123456400000-123458400000 rw-p 00000000 00:00 0\n"
      "123458500000-123458501000 -w-- 00000000 00:00 0\n"
      "123458600000-123458610000 --x- 00000000 00:00 0\n"
      "123458700000-123458703000 -wx- 00000000 00:00 0\n"
      "123458800000-123458900000 ---p 00000000 00:00 0\n";

  fprintf(fp, "%s", valid_content);
  // Add a truncated line at the end.
  fprintf(fp, "00000000-12345678 r--p");
  rewind(fp);

  SignalSafeMapsParser parser(fileno(fp), false);

  // 1. File backed (4KB)
  auto region = parser.Next();
  ASSERT_TRUE(region);
  EXPECT_EQ(region->start, 0x123456000000u);
  EXPECT_EQ(region->end, 0x123456001000u);
  EXPECT_EQ(std::string(region->raw_permissions), "r-xp");
  EXPECT_EQ(region->permissions, PagePermissions::kReadExecute);
  EXPECT_EQ(region->offset, 0x00026000u);
  EXPECT_EQ(region->dev, makedev(0xfe, 0x01));
  EXPECT_EQ(region->inode, 12583839u);
  EXPECT_STREQ(region->pathname, "/lib/x86_64-linux-gnu/libc-2.33.so");

  // 2. Large dev (1MB)
  region = parser.Next();
  ASSERT_TRUE(region);
  EXPECT_EQ(region->start, 0x123456100000u);
  EXPECT_EQ(region->end, 0x123456200000u);
  EXPECT_EQ(std::string(region->raw_permissions), "r--p");
  EXPECT_EQ(region->permissions, PagePermissions::kRead);
  EXPECT_EQ(region->dev, makedev(0x103, 0x104));
  EXPECT_EQ(region->inode, 22u);
  EXPECT_STREQ(region->pathname, "/usr/local/bin/node");

  // 3. Heap (8KB)
  region = parser.Next();
  ASSERT_TRUE(region);
  EXPECT_EQ(region->start, 0x123456300000u);
  EXPECT_EQ(region->end, 0x123456302000u);
  EXPECT_EQ(region->permissions, PagePermissions::kReadWrite);
  EXPECT_STREQ(region->pathname, "[heap]");

  // 4. Anonymous (32MB)
  region = parser.Next();
  ASSERT_TRUE(region);
  EXPECT_EQ(region->start, 0x123456400000u);
  EXPECT_EQ(region->end, 0x123458400000u);
  EXPECT_EQ(region->permissions, PagePermissions::kReadWrite);
  EXPECT_STREQ(region->pathname, "");

  // 5. Write only (4KB)
  region = parser.Next();
  ASSERT_TRUE(region);
  EXPECT_EQ(region->start, 0x123458500000u);
  EXPECT_EQ(region->end, 0x123458501000u);
  EXPECT_EQ(region->permissions, PagePermissions::kWrite);

  // 6. Execute only (64KB)
  region = parser.Next();
  ASSERT_TRUE(region);
  EXPECT_EQ(region->start, 0x123458600000u);
  EXPECT_EQ(region->end, 0x123458610000u);
  EXPECT_EQ(region->permissions, PagePermissions::kExecute);

  // 7. WriteExecute (12KB)
  region = parser.Next();
  ASSERT_TRUE(region);
  EXPECT_EQ(region->start, 0x123458700000u);
  EXPECT_EQ(region->end, 0x123458703000u);
  EXPECT_EQ(region->permissions, PagePermissions::kWriteExecute);

  // 8. No access (PROT_NONE) (1MB)
  region = parser.Next();
  ASSERT_TRUE(region);
  EXPECT_EQ(region->start, 0x123458800000u);
  EXPECT_EQ(region->end, 0x123458900000u);
  EXPECT_EQ(region->permissions, PagePermissions::kNoAccess);

  // 9. Truncated
  region = parser.Next();
  EXPECT_FALSE(region);  // Should fail to parse

  fclose(fp);
}
#endif  // V8_TARGET_ARCH_64_BIT

TEST(OS, SignalSafeMapsParserCustomName) {
  VirtualAddressSpace vas;
  // This test only works if virtual memory subspaces can be allocated.
  if (!vas.CanAllocateSubspaces()) return;

  const char* kName = "v8-test-name";
  auto subspace = vas.AllocateSubspace(
      0, vas.allocation_granularity(), vas.allocation_granularity(),
      PagePermissions::kReadWrite, std::nullopt, std::nullopt);

  // If we can't allocate a subspace, we can't run this test.
  if (!subspace) return;

  // If setting the name fails (e.g. due to missing kernel support for custom
  // names), we can't run this test.
  if (!subspace->SetName(kName)) return;

  SignalSafeMapsParser parser;
  ASSERT_TRUE(parser.IsValid());

  bool found = false;
  while (auto entry = parser.Next()) {
    if (strstr(entry->pathname, kName)) {
      found = true;
      EXPECT_EQ(entry->start, subspace->base());
      break;
    }
  }
  EXPECT_TRUE(found);
}
#endif  // V8_TARGET_OS_LINUX

namespace {

class ThreadLocalStorageTest : public Thread, public ::testing::Test {
 public:
  ThreadLocalStorageTest() : Thread(Options("ThreadLocalStorageTest")) {
    for (size_t i = 0; i < arraysize(keys_); ++i) {
      keys_[i] = Thread::CreateThreadLocalKey();
    }
  }
  ~ThreadLocalStorageTest() override {
    for (size_t i = 0; i < arraysize(keys_); ++i) {
      Thread::DeleteThreadLocalKey(keys_[i]);
    }
  }

  void Run() final {
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK(!Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      Thread::SetThreadLocal(keys_[i], GetValue(i));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK(Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK_EQ(GetValue(i), Thread::GetThreadLocal(keys_[i]));
      CHECK_EQ(GetValue(i), Thread::GetExistingThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      Thread::SetThreadLocal(keys_[i], GetValue(arraysize(keys_) - i - 1));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK(Thread::HasThreadLocal(keys_[i]));
    }
    for (size_t i = 0; i < arraysize(keys_); i++) {
      CHECK_EQ(GetValue(arraysize(keys_) - i - 1),
               Thread::GetThreadLocal(keys_[i]));
      CHECK_EQ(GetValue(arraysize(keys_) - i - 1),
               Thread::GetExistingThreadLocal(keys_[i]));
    }
  }

 private:
  static void* GetValue(size_t x) { return reinterpret_cast<void*>(x + 1); }

  // Older versions of Android have fewer TLS slots (nominally 64, but the
  // system uses "about 5 of them" itself).
  Thread::LocalStorageKey keys_[32];
};

}  // namespace

TEST_F(ThreadLocalStorageTest, DoTest) {
  Run();
  CHECK(Start());
  Join();
}

TEST(StackTest, GetStackStart) { EXPECT_NE(nullptr, Stack::GetStackStart()); }

TEST(StackTest, GetCurrentStackPosition) {
  EXPECT_NE(nullptr, Stack::GetCurrentStackPosition());
}

#if !defined(V8_OS_FUCHSIA)
TEST(StackTest, StackVariableInBounds) {
  void* dummy;
  ASSERT_GT(static_cast<void*>(Stack::GetStackStart()),
            Stack::GetCurrentStackPosition());
  EXPECT_GT(static_cast<void*>(Stack::GetStackStart()),
            Stack::GetRealStackAddressForSlot(&dummy));
  EXPECT_LT(static_cast<void*>(Stack::GetCurrentStackPosition()),
            Stack::GetRealStackAddressForSlot(&dummy));
}
#endif  // !V8_OS_FUCHSIA

using SetDataReadOnlyTest = ::testing::Test;

TEST_F(SetDataReadOnlyTest, SetDataReadOnly) {
  static struct alignas(kMaxPageSize) TestData {
    int x;
    int y;
  } test_data;
  static_assert(alignof(TestData) == kMaxPageSize);
  static_assert(sizeof(TestData) == kMaxPageSize);

  test_data.x = 25;
  test_data.y = 41;

  OS::SetDataReadOnly(&test_data, sizeof(test_data));
  CHECK_EQ(25, test_data.x);
  CHECK_EQ(41, test_data.y);

  ASSERT_DEATH_IF_SUPPORTED(test_data.x = 1, "");
  ASSERT_DEATH_IF_SUPPORTED(test_data.y = 0, "");
}

}  // namespace base

namespace {

#ifdef V8_CC_GNU

static uintptr_t sp_addr = 0;

void GetStackPointerCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  GET_STACK_POINTER_TO(sp_addr);
  CHECK(i::ValidateCallbackInfo(info));
  info.GetReturnValue().Set(v8::Integer::NewFromUnsigned(
      info.GetIsolate(), static_cast<uint32_t>(sp_addr)));
}

using PlatformTest = v8::TestWithIsolate;
TEST_F(PlatformTest, StackAlignment) {
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate());
  global_template->Set(
      isolate(), "get_stack_pointer",
      FunctionTemplate::New(isolate(), GetStackPointerCallback));

  Local<Context> context = Context::New(isolate(), nullptr, global_template);
  Context::Scope context_scope(context);
  TryRunJS(
      "function foo() {"
      "  return get_stack_pointer();"
      "}");

  Local<Object> global_object = context->Global();
  Local<Function> foo = v8::Local<v8::Function>::Cast(
      global_object->Get(isolate()->GetCurrentContext(), NewString("foo"))
          .ToLocalChecked());

  Local<v8::Value> result =
      foo->Call(isolate()->GetCurrentContext(), global_object, 0, nullptr)
          .ToLocalChecked();
  CHECK_EQ(0u, result->Uint32Value(isolate()->GetCurrentContext()).FromJust() %
                   base::OS::ActivationFrameAlignment());
}
#endif  // V8_CC_GNU

}  // namespace

}  // namespace v8
