// Copyright 2008 the V8 project authors. All rights reserved.
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

#include <stdlib.h>

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/hashmap.h"

namespace v8 {
namespace internal {
namespace test_hashmap {

typedef uint32_t (*IntKeyHash)(uint32_t key);

class IntSet {
 public:
  explicit IntSet(IntKeyHash hash) : hash_(hash) {}

  void Insert(int x) {
    CHECK_NE(0, x);  // 0 corresponds to (void*)nullptr - illegal key value
    v8::base::HashMap::Entry* p =
        map_.LookupOrInsert(reinterpret_cast<void*>(x), hash_(x));
    CHECK_NOT_NULL(p);  // insert is set!
    CHECK_EQ(reinterpret_cast<void*>(x), p->key);
    // we don't care about p->value
  }

  void Remove(int x) {
    CHECK_NE(0, x);  // 0 corresponds to (void*)nullptr - illegal key value
    map_.Remove(reinterpret_cast<void*>(x), hash_(x));
  }

  bool Present(int x) {
    v8::base::HashMap::Entry* p =
        map_.Lookup(reinterpret_cast<void*>(x), hash_(x));
    if (p != nullptr) {
      CHECK_EQ(reinterpret_cast<void*>(x), p->key);
    }
    return p != nullptr;
  }

  void Clear() {
    map_.Clear();
  }

  uint32_t occupancy() const {
    uint32_t count = 0;
    for (v8::base::HashMap::Entry* p = map_.Start(); p != nullptr;
         p = map_.Next(p)) {
      count++;
    }
    CHECK_EQ(map_.occupancy(), static_cast<double>(count));
    return count;
  }

 private:
  IntKeyHash hash_;
  v8::base::HashMap map_;
};


static uint32_t Hash(uint32_t key)  { return 23; }
static uint32_t CollisionHash(uint32_t key)  { return key & 0x3; }


void TestSet(IntKeyHash hash, int size) {
  IntSet set(hash);
  CHECK_EQ(0u, set.occupancy());

  set.Insert(1);
  set.Insert(2);
  set.Insert(3);
  CHECK_EQ(3u, set.occupancy());

  set.Insert(2);
  set.Insert(3);
  CHECK_EQ(3u, set.occupancy());

  CHECK(set.Present(1));
  CHECK(set.Present(2));
  CHECK(set.Present(3));
  CHECK(!set.Present(4));
  CHECK_EQ(3u, set.occupancy());

  set.Remove(1);
  CHECK(!set.Present(1));
  CHECK(set.Present(2));
  CHECK(set.Present(3));
  CHECK_EQ(2u, set.occupancy());

  set.Remove(3);
  CHECK(!set.Present(1));
  CHECK(set.Present(2));
  CHECK(!set.Present(3));
  CHECK_EQ(1u, set.occupancy());

  set.Clear();
  CHECK_EQ(0u, set.occupancy());

  // Insert a long series of values.
  const int start = 453;
  const int factor = 13;
  const int offset = 7;
  const uint32_t n = size;

  int x = start;
  for (uint32_t i = 0; i < n; i++) {
    CHECK_EQ(i, static_cast<double>(set.occupancy()));
    set.Insert(x);
    x = x * factor + offset;
  }
  CHECK_EQ(n, static_cast<double>(set.occupancy()));

  // Verify the same sequence of values.
  x = start;
  for (uint32_t i = 0; i < n; i++) {
    CHECK(set.Present(x));
    x = x * factor + offset;
  }
  CHECK_EQ(n, static_cast<double>(set.occupancy()));

  // Remove all these values.
  x = start;
  for (uint32_t i = 0; i < n; i++) {
    CHECK_EQ(n - i, static_cast<double>(set.occupancy()));
    CHECK(set.Present(x));
    set.Remove(x);
    CHECK(!set.Present(x));
    x = x * factor + offset;

    // Verify the the expected values are still there.
    int y = start;
    for (uint32_t j = 0; j < n; j++) {
      if (j <= i) {
        CHECK(!set.Present(y));
      } else {
        CHECK(set.Present(y));
      }
      y = y * factor + offset;
    }
  }
  CHECK_EQ(0u, set.occupancy());
}


TEST(HashSet) {
  TestSet(Hash, 100);
  TestSet(CollisionHash, 50);
}

}  // namespace test_hashmap
}  // namespace internal
}  // namespace v8
