// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/base/atomicops.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

#define CHECK_EQU(v1, v2) \
  CHECK_EQ(static_cast<int64_t>(v1), static_cast<int64_t>(v2))

#define NUM_BITS(T) (sizeof(T) * 8)

template <class AtomicType>
static void TestAtomicIncrement() {
  // For now, we just test the single-threaded execution.

  // Use a guard value to make sure that Relaxed_AtomicIncrement doesn't
  // go outside the expected address bounds.  This is to test that the
  // 32-bit Relaxed_AtomicIncrement doesn't do the wrong thing on 64-bit
  // machines.
  struct {
    AtomicType prev_word;
    AtomicType count;
    AtomicType next_word;
  } s;

  AtomicType prev_word_value, next_word_value;
  memset(&prev_word_value, 0xFF, sizeof(AtomicType));
  memset(&next_word_value, 0xEE, sizeof(AtomicType));

  s.prev_word = prev_word_value;
  s.count = 0;
  s.next_word = next_word_value;

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, 1), 1);
  CHECK_EQU(s.count, 1);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, 2), 3);
  CHECK_EQU(s.count, 3);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, 3), 6);
  CHECK_EQU(s.count, 6);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, -3), 3);
  CHECK_EQU(s.count, 3);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, -2), 1);
  CHECK_EQU(s.count, 1);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, -1), 0);
  CHECK_EQU(s.count, 0);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, -1), -1);
  CHECK_EQU(s.count, -1);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, -4), -5);
  CHECK_EQU(s.count, -5);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(Relaxed_AtomicIncrement(&s.count, 5), 0);
  CHECK_EQU(s.count, 0);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);
}

template <class AtomicType>
static void TestCompareAndSwap() {
  AtomicType value = 0;
  AtomicType prev = Relaxed_CompareAndSwap(&value, 0, 1);
  CHECK_EQU(1, value);
  CHECK_EQU(0, prev);

  // Use a test value that has non-zero bits in both halves, for testing
  // the 64-bit implementation on 32-bit platforms.
  const AtomicType k_test_val =
      (static_cast<AtomicType>(1) << (NUM_BITS(AtomicType) - 2)) + 11;
  value = k_test_val;
  prev = Relaxed_CompareAndSwap(&value, 0, 5);
  CHECK_EQU(k_test_val, value);
  CHECK_EQU(k_test_val, prev);

  value = k_test_val;
  prev = Relaxed_CompareAndSwap(&value, k_test_val, 5);
  CHECK_EQU(5, value);
  CHECK_EQU(k_test_val, prev);
}

template <class AtomicType>
static void TestAtomicExchange() {
  AtomicType value = 0;
  AtomicType new_value = Relaxed_AtomicExchange(&value, 1);
  CHECK_EQU(1, value);
  CHECK_EQU(0, new_value);

  // Use a test value that has non-zero bits in both halves, for testing
  // the 64-bit implementation on 32-bit platforms.
  const AtomicType k_test_val =
      (static_cast<AtomicType>(1) << (NUM_BITS(AtomicType) - 2)) + 11;
  value = k_test_val;
  new_value = Relaxed_AtomicExchange(&value, k_test_val);
  CHECK_EQU(k_test_val, value);
  CHECK_EQU(k_test_val, new_value);

  value = k_test_val;
  new_value = Relaxed_AtomicExchange(&value, 5);
  CHECK_EQU(5, value);
  CHECK_EQU(k_test_val, new_value);
}

template <class AtomicType>
static void TestAtomicIncrementBounds() {
  // Test at 32-bit boundary for 64-bit atomic type.
  AtomicType test_val = static_cast<AtomicType>(1)
                        << (NUM_BITS(AtomicType) / 2);
  AtomicType value = test_val - 1;
  AtomicType new_value = Relaxed_AtomicIncrement(&value, 1);
  CHECK_EQU(test_val, value);
  CHECK_EQU(value, new_value);

  Relaxed_AtomicIncrement(&value, -1);
  CHECK_EQU(test_val - 1, value);
}

// Return an AtomicType with the value 0xA5A5A5..
template <class AtomicType>
static AtomicType TestFillValue() {
  AtomicType val = 0;
  memset(&val, 0xA5, sizeof(AtomicType));
  return val;
}

// This is a simple sanity check to ensure that values are correct.
// Not testing atomicity.
template <class AtomicType>
static void TestStore() {
  const AtomicType kVal1 = TestFillValue<AtomicType>();
  const AtomicType kVal2 = static_cast<AtomicType>(-1);

  AtomicType value;

  Relaxed_Store(&value, kVal1);
  CHECK_EQU(kVal1, value);
  Relaxed_Store(&value, kVal2);
  CHECK_EQU(kVal2, value);

  Release_Store(&value, kVal1);
  CHECK_EQU(kVal1, value);
  Release_Store(&value, kVal2);
  CHECK_EQU(kVal2, value);
}

// Merge this test with TestStore as soon as we have Atomic8 acquire
// and release stores.
static void TestStoreAtomic8() {
  const Atomic8 kVal1 = TestFillValue<Atomic8>();
  const Atomic8 kVal2 = static_cast<Atomic8>(-1);

  Atomic8 value;

  Relaxed_Store(&value, kVal1);
  CHECK_EQU(kVal1, value);
  Relaxed_Store(&value, kVal2);
  CHECK_EQU(kVal2, value);
}

// This is a simple sanity check to ensure that values are correct.
// Not testing atomicity.
template <class AtomicType>
static void TestLoad() {
  const AtomicType kVal1 = TestFillValue<AtomicType>();
  const AtomicType kVal2 = static_cast<AtomicType>(-1);

  AtomicType value;

  value = kVal1;
  CHECK_EQU(kVal1, Relaxed_Load(&value));
  value = kVal2;
  CHECK_EQU(kVal2, Relaxed_Load(&value));

  value = kVal1;
  CHECK_EQU(kVal1, Acquire_Load(&value));
  value = kVal2;
  CHECK_EQU(kVal2, Acquire_Load(&value));
}

// Merge this test with TestLoad as soon as we have Atomic8 acquire
// and release loads.
static void TestLoadAtomic8() {
  const Atomic8 kVal1 = TestFillValue<Atomic8>();
  const Atomic8 kVal2 = static_cast<Atomic8>(-1);

  Atomic8 value;

  value = kVal1;
  CHECK_EQU(kVal1, Relaxed_Load(&value));
  value = kVal2;
  CHECK_EQU(kVal2, Relaxed_Load(&value));
}

TEST(Atomicops, AtomicIncrement) {
  TestAtomicIncrement<Atomic32>();
  TestAtomicIncrement<AtomicWord>();
}

TEST(Atomicops, CompareAndSwap) {
  TestCompareAndSwap<Atomic32>();
  TestCompareAndSwap<AtomicWord>();
}

TEST(Atomicops, AtomicExchange) {
  TestAtomicExchange<Atomic32>();
  TestAtomicExchange<AtomicWord>();
}

TEST(Atomicops, AtomicIncrementBounds) {
  TestAtomicIncrementBounds<Atomic32>();
  TestAtomicIncrementBounds<AtomicWord>();
}

TEST(Atomicops, Store) {
  TestStoreAtomic8();
  TestStore<Atomic32>();
  TestStore<AtomicWord>();
}

TEST(Atomicops, Load) {
  TestLoadAtomic8();
  TestLoad<Atomic32>();
  TestLoad<AtomicWord>();
}

TEST(Atomicops, Relaxed_Memmove) {
  constexpr size_t kLen = 6;
  Atomic8 arr[kLen];
  {
    for (size_t i = 0; i < kLen; ++i) arr[i] = i;
    Relaxed_Memmove(arr + 2, arr + 3, 2);
    uint8_t expected[]{0, 1, 3, 4, 4, 5};
    for (size_t i = 0; i < kLen; ++i) CHECK_EQ(arr[i], expected[i]);
  }
  {
    for (size_t i = 0; i < kLen; ++i) arr[i] = i;
    Relaxed_Memmove(arr + 3, arr + 2, 2);
    uint8_t expected[]{0, 1, 2, 2, 3, 5};
    for (size_t i = 0; i < kLen; ++i) CHECK_EQ(arr[i], expected[i]);
  }
}

TEST(Atomicops, Relaxed_Memcmp) {
  constexpr size_t kLen = 50;
  Atomic8 arr1[kLen];
  Atomic8 arr1_same[kLen];
  Atomic8 arr2[kLen];
  for (size_t i = 0; i < kLen; ++i) {
    arr1[i] = arr1_same[i] = i;
    arr2[i] = i + 1;
  }

  for (size_t offset = 0; offset < kLen; offset++) {
    const Atomic8* arr1p = arr1 + offset;
    const Atomic8* arr1_samep = arr1_same + offset;
    const Atomic8* arr2p = arr2 + offset;
    const size_t len = kLen - offset;
    CHECK_EQ(0, Relaxed_Memcmp(arr1p, arr1p, len));
    CHECK_EQ(0, Relaxed_Memcmp(arr1p, arr1_samep, len));
    CHECK_LT(Relaxed_Memcmp(arr1p, arr2p, len), 0);
    CHECK_GT(Relaxed_Memcmp(arr2p, arr1p, len), 0);
  }
}

}  // namespace base
}  // namespace v8
