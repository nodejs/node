#include "gtest/gtest.h"

#ifdef HAVE_FFI_FASTCALL

#include <set>
#include <vector>

#if defined(__POSIX__)
#include <unistd.h>
#endif

#include "ffi/fastcall/jit_memory.h"
#include "node_test_fixture.h"

namespace {
size_t SysPageSize() {
#if defined(__POSIX__)
  return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#else
  return 4096;
#endif
}
}  // namespace

using node::ffi::fastcall::JitMemory;

class JitMemoryTest : public NodeTestFixture {
 protected:
  void TearDown() override {
    JitMemory::Get(isolate_)->ResetForTesting();
    NodeTestFixture::TearDown();
  }
};

TEST_F(JitMemoryTest, AllocateZeroFails) {
  auto* jit = JitMemory::Get(isolate_);
  size_t alloc_size = 0;
  EXPECT_EQ(nullptr, jit->Allocate(0, &alloc_size));
}

TEST_F(JitMemoryTest, AllocateSmallReturnsAligned) {
  auto* jit = JitMemory::Get(isolate_);
  size_t alloc_size = 0;
  void* p = jit->Allocate(32, &alloc_size);
  ASSERT_NE(nullptr, p);
  EXPECT_GE(alloc_size, 32u);
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(p) % 64);
  jit->Free(p, alloc_size);
}

TEST_F(JitMemoryTest, FreeIsNoOp) {
  auto* jit = JitMemory::Get(isolate_);
  size_t a = 0, b = 0;
  void* p1 = jit->Allocate(48, &a);
  ASSERT_NE(nullptr, p1);
  size_t live_before = jit->TotalLiveBytes();
  jit->Free(p1, a);
  EXPECT_LT(jit->TotalLiveBytes(), live_before);
  void* p2 = jit->Allocate(48, &b);
  ASSERT_NE(nullptr, p2);
  EXPECT_NE(p1, p2);
  jit->Free(p2, b);
}

TEST_F(JitMemoryTest, ManySmallAllocsFit) {
  auto* jit = JitMemory::Get(isolate_);
  std::vector<std::pair<void*, size_t>> ptrs;
  for (int i = 0; i < 200; ++i) {
    size_t a = 0;
    void* p = jit->Allocate(40, &a);
    ASSERT_NE(nullptr, p) << "alloc " << i;
    ptrs.push_back({p, a});
  }
  for (auto& [p, a] : ptrs) jit->Free(p, a);
}

#if defined(__x86_64__) || defined(_M_X64) || \
    defined(__aarch64__) || defined(_M_ARM64)
TEST_F(JitMemoryTest, ExecutableStubReturnsValue) {
  auto* jit = JitMemory::Get(isolate_);
  size_t alloc_size = 0;
  void* p = jit->Allocate(64, &alloc_size);
  ASSERT_NE(nullptr, p);

  uint8_t* code = static_cast<uint8_t*>(p);
#if defined(__x86_64__) || defined(_M_X64)
  // mov eax, 42; ret
  code[0] = 0xb8; code[1] = 0x2a; code[2] = 0x00;
  code[3] = 0x00; code[4] = 0x00;
  code[5] = 0xc3;
#elif defined(__aarch64__) || defined(_M_ARM64)
  uint32_t* w = reinterpret_cast<uint32_t*>(code);
  w[0] = 0x52800540;  // mov w0, #42
  w[1] = 0xd65f03c0;  // ret
#endif

  ASSERT_TRUE(jit->MakeExecutable(p, alloc_size));
  using FnPtr = int (*)();
  EXPECT_EQ(42, reinterpret_cast<FnPtr>(p)());
  jit->Free(p, alloc_size);
}
#endif  // x86_64 || aarch64

TEST_F(JitMemoryTest, SelfTestPasses) {
  EXPECT_TRUE(JitMemory::Get(isolate_)->SelfTest(isolate_));
}

TEST_F(JitMemoryTest, MultiplePagesAllocate) {
  auto* jit = JitMemory::Get(isolate_);
  // Force at least 2 pages by allocating many large chunks.
  // 1000 × 1024 bytes = ~1 MB, guaranteed to span pages on any platform.
  std::vector<std::pair<void*, size_t>> ptrs;
  for (int i = 0; i < 1000; ++i) {
    size_t a = 0;
    void* p = jit->Allocate(1024, &a);
    ASSERT_NE(nullptr, p) << "alloc " << i;
    ptrs.push_back({p, a});
  }
  // Sanity: at least two distinct page bases observed.
  const size_t page_mask = ~(SysPageSize() - 1);
  std::set<uintptr_t> page_bases;
  for (auto& [p, ignored] : ptrs) {
    page_bases.insert(reinterpret_cast<uintptr_t>(p) & page_mask);
  }
  EXPECT_GE(page_bases.size(), 2u);
  for (auto& [p, a] : ptrs) jit->Free(p, a);
}

TEST_F(JitMemoryTest, MakeExecutableNullArgsSafe) {
  auto* jit = JitMemory::Get(isolate_);
  // Verify graceful handling of nullptr / zero size. The page_size_==0
  // uninitialized branch is unreachable here because Get(isolate_) already
  // initialized the singleton; that branch is defense-in-depth only.
  EXPECT_FALSE(jit->MakeExecutable(nullptr, 64));
  EXPECT_FALSE(jit->MakeExecutable(reinterpret_cast<void*>(0x1), 0));
}

TEST_F(JitMemoryTest, AllocateAfterMakeExecutableSkipsSealedPage) {
  auto* jit = JitMemory::Get(isolate_);
  size_t a1 = 0;
  void* p1 = jit->Allocate(64, &a1);
  ASSERT_NE(nullptr, p1);
  ASSERT_TRUE(jit->MakeExecutable(p1, a1));

  // After sealing, a new alloc should NOT be in the same page.
  size_t a2 = 0;
  void* p2 = jit->Allocate(64, &a2);
  ASSERT_NE(nullptr, p2);
  const uintptr_t page_mask = ~(SysPageSize() - 1);
  uintptr_t page1 = reinterpret_cast<uintptr_t>(p1) & page_mask;
  uintptr_t page2 = reinterpret_cast<uintptr_t>(p2) & page_mask;
  EXPECT_NE(page1, page2);

  jit->Free(p2, a2);
  // p1 was already made executable; freeing is fine (counter only).
  jit->Free(p1, a1);
}

#endif  // HAVE_FFI_FASTCALL
