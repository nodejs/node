#if HAVE_OPENSSL && HAVE_QUIC
#include <gtest/gtest.h>
#include <quic/arena.h>
#include <cstdint>
#include <utility>
#include <vector>

namespace node::quic {
namespace {

// A simple test type with non-trivial constructor/destructor to verify
// that ArenaPool properly manages object lifecycles.
struct TestObj {
  int value;
  bool* destroyed;  // pointer to external flag set on destruction

  TestObj(int v, bool* d) : value(v), destroyed(d) {}
  ~TestObj() {
    if (destroyed) *destroyed = true;
  }
};

// A test type that accepts extra data (like Packet).
struct TestObjWithExtra {
  uint8_t* data;
  size_t capacity;
  int tag;

  TestObjWithExtra(uint8_t* d, size_t cap, int t)
      : data(d), capacity(cap), tag(t) {}
};

TEST(QuicArenaPool, BasicAcquireRelease) {
  ArenaPool<TestObj> pool(0, 4);
  EXPECT_EQ(pool.total_slots(), 0u);
  EXPECT_EQ(pool.in_use_count(), 0u);

  bool destroyed = false;
  {
    auto ptr = pool.Acquire(42, &destroyed);
    ASSERT_TRUE(ptr);
    EXPECT_EQ(ptr->value, 42);
    EXPECT_EQ(pool.total_slots(), 4);  // first block allocated
    EXPECT_EQ(pool.in_use_count(), 1);
    EXPECT_EQ(pool.block_count(), 1);
    EXPECT_FALSE(destroyed);
  }
  // Ptr went out of scope — destructor called, slot released
  EXPECT_TRUE(destroyed);
  EXPECT_EQ(pool.in_use_count(), 0);
}

TEST(QuicArenaPool, GrowsOnDemand) {
  ArenaPool<TestObj> pool(0, 2);  // 2 slots per block

  bool d1 = false, d2 = false, d3 = false;
  auto p1 = pool.Acquire(1, &d1);
  auto p2 = pool.Acquire(2, &d2);
  EXPECT_EQ(pool.block_count(), 1);
  EXPECT_EQ(pool.total_slots(), 2);

  // This should trigger a second block
  auto p3 = pool.Acquire(3, &d3);
  EXPECT_EQ(pool.block_count(), 2);
  EXPECT_EQ(pool.total_slots(), 4);
  EXPECT_EQ(pool.in_use_count(), 3);

  EXPECT_EQ(p1->value, 1);
  EXPECT_EQ(p2->value, 2);
  EXPECT_EQ(p3->value, 3);
}

TEST(QuicArenaPool, PtrMoveSemantics) {
  ArenaPool<TestObj> pool(0, 4);
  bool destroyed = false;

  auto p1 = pool.Acquire(10, &destroyed);
  ASSERT_TRUE(p1);

  // Move construction
  auto p2 = std::move(p1);
  EXPECT_FALSE(p1);  // NOLINT — testing moved-from state
  ASSERT_TRUE(p2);
  EXPECT_EQ(p2->value, 10);
  EXPECT_FALSE(destroyed);

  // Move assignment
  bool d2 = false;
  auto p3 = pool.Acquire(20, &d2);
  p3 = std::move(p2);
  EXPECT_TRUE(d2);   // p3's old object destroyed
  EXPECT_FALSE(p2);  // NOLINT
  ASSERT_TRUE(p3);
  EXPECT_EQ(p3->value, 10);
  EXPECT_FALSE(destroyed);

  p3.reset();
  EXPECT_TRUE(destroyed);
}

TEST(QuicArenaPool, PtrRelease) {
  ArenaPool<TestObj> pool(0, 4);
  bool destroyed = false;

  auto ptr = pool.Acquire(99, &destroyed);
  TestObj* raw = ptr.release();
  EXPECT_FALSE(ptr);        // Ptr is now empty
  EXPECT_FALSE(destroyed);  // Object not yet destroyed

  // Verify the raw pointer is valid
  EXPECT_EQ(raw->value, 99);
  EXPECT_EQ(pool.in_use_count(), 1);

  // Release via static method — this calls ~TestObj and returns slot
  ArenaPool<TestObj>::Release(raw);
  EXPECT_TRUE(destroyed);
  EXPECT_EQ(pool.in_use_count(), 0);
}

TEST(QuicArenaPool, MaybeGCFreesEmptyBlocks) {
  ArenaPool<TestObj> pool(0, 2);

  // Fill two blocks
  bool d[4] = {};
  auto p1 = pool.Acquire(1, &d[0]);
  auto p2 = pool.Acquire(2, &d[1]);
  auto p3 = pool.Acquire(3, &d[2]);
  auto p4 = pool.Acquire(4, &d[3]);
  EXPECT_EQ(pool.block_count(), 2u);
  EXPECT_EQ(pool.total_slots(), 4u);
  EXPECT_EQ(pool.in_use_count(), 4u);

  // Release all four. With in_use == 0 and 2 blocks, GC should
  // free one block (keeping at least one).
  p1.reset();
  p2.reset();
  p3.reset();
  p4.reset();
  EXPECT_EQ(pool.in_use_count(), 0u);
  // GC should have freed one block (triggered when in_use < total/2)
  EXPECT_EQ(pool.block_count(), 1u);
  EXPECT_EQ(pool.total_slots(), 2u);
}

TEST(QuicArenaPool, FlushKeepsAtLeastOneBlock) {
  ArenaPool<TestObj> pool(0, 2);

  bool d = false;
  auto p = pool.Acquire(1, &d);
  p.reset();
  // One block, all slots free.
  EXPECT_EQ(pool.block_count(), 1);

  pool.Flush();
  // Should keep at least one block even though it's fully free.
  EXPECT_EQ(pool.block_count(), 1);
}

TEST(QuicArenaPool, AcquireExtraWiresExtraData) {
  static constexpr size_t kExtraBytes = 256;
  ArenaPool<TestObjWithExtra> pool(kExtraBytes, 4);

  auto ptr = pool.AcquireExtra(42);
  ASSERT_TRUE(ptr);
  EXPECT_EQ(ptr->tag, 42);
  EXPECT_EQ(ptr->capacity, kExtraBytes);
  EXPECT_NE(ptr->data, nullptr);

  // The extra data pointer from Ptr should match what the constructor got.
  EXPECT_EQ(ptr.extra_data(), ptr->data);
  EXPECT_EQ(ptr.extra_bytes(), kExtraBytes);

  // Verify we can write to the extra data region.
  std::memset(ptr->data, 0xAB, kExtraBytes);
  EXPECT_EQ(ptr->data[0], 0xAB);
  EXPECT_EQ(ptr->data[kExtraBytes - 1], 0xAB);
}

TEST(QuicArenaPool, SlotReuse) {
  ArenaPool<TestObj> pool(0, 2);
  bool d1 = false, d2 = false;

  // Acquire and release
  {
    auto p = pool.Acquire(1, &d1);
    EXPECT_EQ(pool.total_slots(), 2);
  }
  EXPECT_TRUE(d1);

  // Acquire again — should reuse the slot without growing
  auto p2 = pool.Acquire(2, &d2);
  EXPECT_EQ(pool.total_slots(), 2);  // no growth
  EXPECT_EQ(pool.block_count(), 1);
  EXPECT_EQ(p2->value, 2);
}

TEST(QuicArenaPool, SlotSizeIncludesExtraBytes) {
  static constexpr size_t kExtra = 1200;
  ArenaPool<TestObjWithExtra> pool(kExtra, 4);

  // Slot size should accommodate header + TestObjWithExtra + 1200 extra bytes
  EXPECT_GE(pool.slot_size(), sizeof(TestObjWithExtra) + kExtra);
  EXPECT_EQ(pool.extra_bytes(), kExtra);
}

TEST(QuicArenaPool, MultipleAcquireReleaseChurn) {
  ArenaPool<TestObj> pool(0, 4);

  // Simulate a burst of acquires and releases
  for (int round = 0; round < 10; round++) {
    std::vector<ArenaPool<TestObj>::Ptr> ptrs;
    bool destroyed[8] = {};
    for (int i = 0; i < 8; i++) {
      ptrs.push_back(pool.Acquire(i, &destroyed[i]));
    }
    EXPECT_EQ(pool.in_use_count(), 8);

    // Release all
    ptrs.clear();
    EXPECT_EQ(pool.in_use_count(), 0);
    for (int i = 0; i < 8; i++) {
      EXPECT_TRUE(destroyed[i]);
    }
  }
  // Pool may have grown but should be stable
  EXPECT_EQ(pool.in_use_count(), 0);
}

}  // namespace
}  // namespace node::quic

#endif  // HAVE_OPENSSL && HAVE_QUIC
