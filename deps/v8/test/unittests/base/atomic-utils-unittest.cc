// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include "src/base/atomic-utils.h"
#include "src/base/platform/platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(AtomicNumber, Constructor) {
  // Test some common types.
  AtomicNumber<int> zero_int;
  AtomicNumber<size_t> zero_size_t;
  AtomicNumber<intptr_t> zero_intptr_t;
  EXPECT_EQ(0, zero_int.Value());
  EXPECT_EQ(0u, zero_size_t.Value());
  EXPECT_EQ(0, zero_intptr_t.Value());
}


TEST(AtomicNumber, Value) {
  AtomicNumber<int> a(1);
  EXPECT_EQ(1, a.Value());
  AtomicNumber<int> b(-1);
  EXPECT_EQ(-1, b.Value());
  AtomicNumber<size_t> c(1);
  EXPECT_EQ(1u, c.Value());
  AtomicNumber<size_t> d(static_cast<size_t>(-1));
  EXPECT_EQ(std::numeric_limits<size_t>::max(), d.Value());
}


TEST(AtomicNumber, SetValue) {
  AtomicNumber<int> a(1);
  a.SetValue(-1);
  EXPECT_EQ(-1, a.Value());
}


TEST(AtomicNumber, Increment) {
  AtomicNumber<int> a(std::numeric_limits<int>::max());
  a.Increment(1);
  EXPECT_EQ(std::numeric_limits<int>::min(), a.Value());
  // Check that potential signed-ness of the underlying storage has no impact
  // on unsigned types.
  AtomicNumber<size_t> b(std::numeric_limits<intptr_t>::max());
  b.Increment(1);
  EXPECT_EQ(static_cast<size_t>(std::numeric_limits<intptr_t>::max()) + 1,
            b.Value());
  // Should work as decrement as well.
  AtomicNumber<size_t> c(1);
  c.Increment(-1);
  EXPECT_EQ(0u, c.Value());
  c.Increment(-1);
  EXPECT_EQ(std::numeric_limits<size_t>::max(), c.Value());
}

TEST(AtomicNumber, Decrement) {
  AtomicNumber<size_t> a(std::numeric_limits<size_t>::max());
  a.Increment(1);
  EXPECT_EQ(0u, a.Value());
  a.Decrement(1);
  EXPECT_EQ(std::numeric_limits<size_t>::max(), a.Value());
}

TEST(AtomicNumber, OperatorAdditionAssignment) {
  AtomicNumber<size_t> a(0u);
  AtomicNumber<size_t> b(std::numeric_limits<size_t>::max());
  a += b.Value();
  EXPECT_EQ(a.Value(), b.Value());
  EXPECT_EQ(b.Value(), std::numeric_limits<size_t>::max());
}

TEST(AtomicNumber, OperatorSubtractionAssignment) {
  AtomicNumber<size_t> a(std::numeric_limits<size_t>::max());
  AtomicNumber<size_t> b(std::numeric_limits<size_t>::max());
  a -= b.Value();
  EXPECT_EQ(a.Value(), 0u);
  EXPECT_EQ(b.Value(), std::numeric_limits<size_t>::max());
}

namespace {

enum TestFlag : base::AtomicWord {
  kA,
  kB,
  kC,
};

}  // namespace


TEST(AtomicValue, Initial) {
  AtomicValue<TestFlag> a(kA);
  EXPECT_EQ(TestFlag::kA, a.Value());
}


TEST(AtomicValue, TrySetValue) {
  AtomicValue<TestFlag> a(kA);
  EXPECT_FALSE(a.TrySetValue(kB, kC));
  EXPECT_TRUE(a.TrySetValue(kA, kC));
  EXPECT_EQ(TestFlag::kC, a.Value());
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
  EXPECT_FALSE(a.TrySetValue(nullptr, &dummy));
  EXPECT_TRUE(a.TrySetValue(&a, &dummy));
  EXPECT_EQ(&dummy, a.Value());
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
    threads[i].Start();
  }

  for (int i = 0; i < kThreadCount; i++) {
    threads[i].Join();
  }

  for (int i = 0; i < kByteCount; i++) {
    EXPECT_EQ(i + kIncrements * kThreadsPerByte,
              AsAtomic8::Relaxed_Load(&bytes[i]));
  }
}

TEST(AsAtomicWord, SetBits_Sequential) {
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
    AsAtomicWord::SetBits(&word, byte, mask);
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
    AsAtomicWord::SetBits(word_addr_, bit, bit);
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
    threads[i].Start();
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

}  // namespace base
}  // namespace v8
