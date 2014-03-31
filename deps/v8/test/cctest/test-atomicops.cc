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

#include "v8.h"

#include "cctest.h"
#include "atomicops.h"

using namespace v8::internal;


#define CHECK_EQU(v1, v2) \
  CHECK_EQ(static_cast<int64_t>(v1), static_cast<int64_t>(v2))

#define NUM_BITS(T) (sizeof(T) * 8)


template <class AtomicType>
static void TestAtomicIncrement() {
  // For now, we just test the single-threaded execution.

  // Use a guard value to make sure that NoBarrier_AtomicIncrement doesn't
  // go outside the expected address bounds.  This is to test that the
  // 32-bit NoBarrier_AtomicIncrement doesn't do the wrong thing on 64-bit
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

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, 1), 1);
  CHECK_EQU(s.count, 1);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, 2), 3);
  CHECK_EQU(s.count, 3);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, 3), 6);
  CHECK_EQU(s.count, 6);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, -3), 3);
  CHECK_EQU(s.count, 3);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, -2), 1);
  CHECK_EQU(s.count, 1);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, -1), 0);
  CHECK_EQU(s.count, 0);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, -1), -1);
  CHECK_EQU(s.count, -1);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, -4), -5);
  CHECK_EQU(s.count, -5);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);

  CHECK_EQU(NoBarrier_AtomicIncrement(&s.count, 5), 0);
  CHECK_EQU(s.count, 0);
  CHECK_EQU(s.prev_word, prev_word_value);
  CHECK_EQU(s.next_word, next_word_value);
}


template <class AtomicType>
static void TestCompareAndSwap() {
  AtomicType value = 0;
  AtomicType prev = NoBarrier_CompareAndSwap(&value, 0, 1);
  CHECK_EQU(1, value);
  CHECK_EQU(0, prev);

  // Use a test value that has non-zero bits in both halves, for testing
  // the 64-bit implementation on 32-bit platforms.
  const AtomicType k_test_val =
      (static_cast<AtomicType>(1) << (NUM_BITS(AtomicType) - 2)) + 11;
  value = k_test_val;
  prev = NoBarrier_CompareAndSwap(&value, 0, 5);
  CHECK_EQU(k_test_val, value);
  CHECK_EQU(k_test_val, prev);

  value = k_test_val;
  prev = NoBarrier_CompareAndSwap(&value, k_test_val, 5);
  CHECK_EQU(5, value);
  CHECK_EQU(k_test_val, prev);
}


template <class AtomicType>
static void TestAtomicExchange() {
  AtomicType value = 0;
  AtomicType new_value = NoBarrier_AtomicExchange(&value, 1);
  CHECK_EQU(1, value);
  CHECK_EQU(0, new_value);

  // Use a test value that has non-zero bits in both halves, for testing
  // the 64-bit implementation on 32-bit platforms.
  const AtomicType k_test_val =
      (static_cast<AtomicType>(1) << (NUM_BITS(AtomicType) - 2)) + 11;
  value = k_test_val;
  new_value = NoBarrier_AtomicExchange(&value, k_test_val);
  CHECK_EQU(k_test_val, value);
  CHECK_EQU(k_test_val, new_value);

  value = k_test_val;
  new_value = NoBarrier_AtomicExchange(&value, 5);
  CHECK_EQU(5, value);
  CHECK_EQU(k_test_val, new_value);
}


template <class AtomicType>
static void TestAtomicIncrementBounds() {
  // Test at rollover boundary between int_max and int_min.
  AtomicType test_val =
      static_cast<AtomicType>(1) << (NUM_BITS(AtomicType) - 1);
  AtomicType value = -1 ^ test_val;
  AtomicType new_value = NoBarrier_AtomicIncrement(&value, 1);
  CHECK_EQU(test_val, value);
  CHECK_EQU(value, new_value);

  NoBarrier_AtomicIncrement(&value, -1);
  CHECK_EQU(-1 ^ test_val, value);

  // Test at 32-bit boundary for 64-bit atomic type.
  test_val = static_cast<AtomicType>(1) << (NUM_BITS(AtomicType) / 2);
  value = test_val - 1;
  new_value = NoBarrier_AtomicIncrement(&value, 1);
  CHECK_EQU(test_val, value);
  CHECK_EQU(value, new_value);

  NoBarrier_AtomicIncrement(&value, -1);
  CHECK_EQU(test_val - 1, value);
}


// Return an AtomicType with the value 0xa5a5a5..
template <class AtomicType>
static AtomicType TestFillValue() {
  AtomicType val = 0;
  memset(&val, 0xa5, sizeof(AtomicType));
  return val;
}


// This is a simple sanity check to ensure that values are correct.
// Not testing atomicity.
template <class AtomicType>
static void TestStore() {
  const AtomicType kVal1 = TestFillValue<AtomicType>();
  const AtomicType kVal2 = static_cast<AtomicType>(-1);

  AtomicType value;

  NoBarrier_Store(&value, kVal1);
  CHECK_EQU(kVal1, value);
  NoBarrier_Store(&value, kVal2);
  CHECK_EQU(kVal2, value);

  Acquire_Store(&value, kVal1);
  CHECK_EQU(kVal1, value);
  Acquire_Store(&value, kVal2);
  CHECK_EQU(kVal2, value);

  Release_Store(&value, kVal1);
  CHECK_EQU(kVal1, value);
  Release_Store(&value, kVal2);
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
  CHECK_EQU(kVal1, NoBarrier_Load(&value));
  value = kVal2;
  CHECK_EQU(kVal2, NoBarrier_Load(&value));

  value = kVal1;
  CHECK_EQU(kVal1, Acquire_Load(&value));
  value = kVal2;
  CHECK_EQU(kVal2, Acquire_Load(&value));

  value = kVal1;
  CHECK_EQU(kVal1, Release_Load(&value));
  value = kVal2;
  CHECK_EQU(kVal2, Release_Load(&value));
}


TEST(AtomicIncrement) {
  TestAtomicIncrement<Atomic32>();
  TestAtomicIncrement<AtomicWord>();
}


TEST(CompareAndSwap) {
  TestCompareAndSwap<Atomic32>();
  TestCompareAndSwap<AtomicWord>();
}


TEST(AtomicExchange) {
  TestAtomicExchange<Atomic32>();
  TestAtomicExchange<AtomicWord>();
}


TEST(AtomicIncrementBounds) {
  TestAtomicIncrementBounds<Atomic32>();
  TestAtomicIncrementBounds<AtomicWord>();
}


TEST(Store) {
  TestStore<Atomic32>();
  TestStore<AtomicWord>();
}


TEST(Load) {
  TestLoad<Atomic32>();
  TestLoad<AtomicWord>();
}
