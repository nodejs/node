// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/atomic-utils.h"

#include <limits.h>

#include "src/base/memcopy.h"
#include "src/base/platform/platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {
namespace {

enum TestFlag : base::AtomicWord { kA, kB, kC };

}  // namespace


TEST(AtomicValue, Initial) {
  AtomicValue<TestFlag> a(kA);
  EXPECT_EQ(TestFlag::kA, a.Value());
}

TEST(AtomicValue, SetValue) {
  AtomicValue<TestFlag> a(kB);
  a.SetValue(kC);
  EXPECT_EQ(TestFlag::kC, a.Value());
}


TEST(AtomicValue, WithVoidStar) {
  AtomicValue<void*> a(nullptr);
  AtomicValue<void*> dummy(nullptr);
  EXPECT_EQ(nullptr, a.Value());
  a.SetValue(&a);
  EXPECT_EQ(&a, a.Value());
}

TEST(AsAtomic8, CompareAndSwap_Sequential) {
  uint8_t bytes[8];
  for (int i = 0; i < 8; i++) {
    bytes[i] = 0xF0 + i;
  }
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(0xF0 + i,
              AsAtomic8::Release_CompareAndSwap(&bytes[i], i, 0xF7 + i));
  }
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(0xF0 + i,
              AsAtomic8::Release_CompareAndSwap(&bytes[i], 0xF0 + i, 0xF7 + i));
  }
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(0xF7 + i, bytes[i]);
  }
}

namespace {

class ByteIncrementingThread final : public Thread {
 public:
  ByteIncrementingThread()
      : Thread(Options("ByteIncrementingThread")),
        byte_addr_(nullptr),
        increments_(0) {}

  void Initialize(uint8_t* byte_addr, int increments) {
    byte_addr_ = byte_addr;
    increments_ = increments;
  }

  void Run() override {
    for (int i = 0; i < increments_; i++) {
      Increment();
    }
  }

  void Increment() {
    uint8_t byte;
    do {
      byte = AsAtomic8::Relaxed_Load(byte_addr_);
    } while (AsAtomic8::Release_CompareAndSwap(byte_addr_, byte, byte + 1) !=
             byte);
  }

 private:
  uint8_t* byte_addr_;
  int increments_;
};

}  // namespace

TEST(AsAtomic8, CompareAndSwap_Concurrent) {
  const int kIncrements = 10;
  const int kByteCount = 8;
  uint8_t bytes[kByteCount];
  const int kThreadsPerByte = 4;
  const int kThreadCount = kByteCount * kThreadsPerByte;
  ByteIncrementingThread threads[kThreadCount];

  for (int i = 0; i < kByteCount; i++) {
    AsAtomic8::Relaxed_Store(&bytes[i], i);
    for (int j = 0; j < kThreadsPerByte; j++) {
      threads[i * kThreadsPerByte + j].Initialize(&bytes[i], kIncrements);
    }
  }
  for (int i = 0; i < kThreadCount; i++) {
    CHECK(threads[i].Start());
  }

  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Join();
  }

  for (int i = 0; i < kByteCount; i++) {
    EXPECT_EQ(i + kIncrements * kThreadsPerByte,
              AsAtomic8::Relaxed_Load(&bytes[i]));
  }
}

TEST(AsAtomicWord, Relaxed_SetBits_Sequential) {
  uintptr_t word = 0;
  // Fill the word with a repeated 0xF0 pattern.
  for (unsigned i = 0; i < sizeof(word); i++) {
    word = (word << 8) | 0xF0;
  }
  // Check the pattern.
  for (unsigned i = 0; i < sizeof(word); i++) {
    EXPECT_EQ(0xF0u, (word >> (i * 8) & 0xFFu));
  }
  // Set the i-th byte value to i.
  uintptr_t mask = 0xFF;
  for (unsigned i = 0; i < sizeof(word); i++) {
    uintptr_t byte = static_cast<uintptr_t>(i) << (i * 8);
    AsAtomicWord::Relaxed_SetBits(&word, byte, mask);
    mask <<= 8;
  }
  for (unsigned i = 0; i < sizeof(word); i++) {
    EXPECT_EQ(i, (word >> (i * 8) & 0xFFu));
  }
}

TEST(AsAtomicWord, Relaxed_SetBitsByMask_Sequential) {
  uintptr_t word = 0;
  // Fill the word with a repeated 0xF0 pattern.
  for (unsigned i = 0; i < sizeof(word); i++) {
    word = (word << 8) | 0xF0;
  }
  // Check the pattern.
  for (unsigned i = 0; i < sizeof(word); i++) {
    EXPECT_EQ(0xF0u, (word >> (i * 8) & 0xFFu));
  }
  // Set the i-th byte value to 0XF1.
  uintptr_t mask = 0x01;
  for (unsigned i = 0; i < sizeof(word); i++) {
    AsAtomicWord::Relaxed_SetBits(&word, mask);
    mask <<= 8;
  }
  for (unsigned i = 0; i < sizeof(word); i++) {
    EXPECT_EQ(0xF1u, (word >> (i * 8) & 0xFFu));
  }
}

TEST(AsAtomicWord, Release_SetBits_Sequential) {
  uintptr_t word = 0;
  // Fill the word with a repeated 0xF0 pattern.
  for (unsigned i = 0; i < sizeof(word); i++) {
    word = (word << 8) | 0xF0;
  }
  // Check the pattern.
  for (unsigned i = 0; i < sizeof(word); i++) {
    EXPECT_EQ(0xF0u, (word >> (i * 8) & 0xFFu));
  }
  // Set the i-th byte value to i.
  uintptr_t mask = 0xFF;
  for (unsigned i = 0; i < sizeof(word); i++) {
    uintptr_t byte = static_cast<uintptr_t>(i) << (i * 8);
    AsAtomicWord::Release_SetBits(&word, byte, mask);
    mask <<= 8;
  }
  for (unsigned i = 0; i < sizeof(word); i++) {
    EXPECT_EQ(i, (word >> (i * 8) & 0xFFu));
  }
}

namespace {

class BitSettingThread final : public Thread {
 public:
  BitSettingThread()
      : Thread(Options("BitSettingThread")),
        word_addr_(nullptr),
        bit_index_(0) {}

  void Initialize(uintptr_t* word_addr, int bit_index) {
    word_addr_ = word_addr;
    bit_index_ = bit_index;
  }

  void Run() override {
    uintptr_t bit = 1;
    bit = bit << bit_index_;
    AsAtomicWord::Relaxed_SetBits(word_addr_, bit, bit);
  }

 private:
  uintptr_t* word_addr_;
  int bit_index_;
};

}  // namespace.

TEST(AsAtomicWord, SetBits_Concurrent) {
  const int kBitCount = sizeof(uintptr_t) * 8;
  const int kThreadCount = kBitCount / 2;
  BitSettingThread threads[kThreadCount];

  uintptr_t word;
  AsAtomicWord::Relaxed_Store(&word, 0);
  for (int i = 0; i < kThreadCount; i++) {
    // Thread i sets bit number i * 2.
    threads[i].Initialize(&word, i * 2);
  }
  for (int i = 0; i < kThreadCount; i++) {
    CHECK(threads[i].Start());
  }
  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Join();
  }
  uintptr_t actual_word = AsAtomicWord::Relaxed_Load(&word);
  for (int i = 0; i < kBitCount; i++) {
    // Every second bit must be set.
    uintptr_t expected = (i % 2 == 0);
    EXPECT_EQ(expected, actual_word & 1u);
    actual_word >>= 1;
  }
}

template <typename T>
void TestRelaxedMemsetHelper(T value, T sentinel) {
  constexpr int kBufferSize = 64;
  for (int offset = 0; offset < 8; ++offset) {
    for (int count = 0; count < 32; ++count) {
      if (offset + count > kBufferSize) continue;
      alignas(8) T buffer[kBufferSize];
      for (int i = 0; i < kBufferSize; ++i) {
        buffer[i] = sentinel;
      }
      Relaxed_Memset(&buffer[offset], value, count);
      for (int i = 0; i < kBufferSize; ++i) {
        if (i >= offset && i < offset + count) {
          EXPECT_EQ(value, buffer[i])
              << "offset=" << offset << " count=" << count << " i=" << i;
        } else {
          EXPECT_EQ(sentinel, buffer[i])
              << "offset=" << offset << " count=" << count << " i=" << i;
        }
      }
    }
  }
}

TEST(RelaxedMemset, VariousTypesAndAlignments) {
  TestRelaxedMemsetHelper<uint8_t>(0xAB, 0x12);
  TestRelaxedMemsetHelper<uint16_t>(0xABCD, 0x1234);
  TestRelaxedMemsetHelper<uint32_t>(0xABCDEF01u, 0x12345678u);
  TestRelaxedMemsetHelper<uint64_t>(0xABCDEF0123456789ULL,
                                    0x1111222233334444ULL);
  TestRelaxedMemsetHelper<int8_t>(-1, 0);
  TestRelaxedMemsetHelper<int32_t>(-123456, 789);
}

TEST(RelaxedMemset, ConstexprReplicatedWord) {
  static_assert(ReplicateValueWord<uint8_t>(0xAB) ==
#if V8_HOST_ARCH_64_BIT
                0xABABABABABABABABULL
#else
                0xABABABABu
#endif
  );
  static_assert(ReplicateValueWord<uint16_t>(0x1234) ==
#if V8_HOST_ARCH_64_BIT
                0x1234123412341234ULL
#else
                0x12341234u
#endif
  );

  constexpr int kBufferSize = 32;
  alignas(8) uint8_t buffer[kBufferSize];
  for (int i = 0; i < kBufferSize; ++i) {
    buffer[i] = 0xFF;
  }
  Relaxed_Memset<uint8_t>(&buffer[1], 0x42, 15);
  EXPECT_EQ(0xFF, buffer[0]);
  for (int i = 1; i < 16; ++i) {
    EXPECT_EQ(0x42, buffer[i]);
  }
  for (int i = 16; i < kBufferSize; ++i) {
    EXPECT_EQ(0xFF, buffer[i]);
  }
}

}  // namespace base
}  // namespace v8
