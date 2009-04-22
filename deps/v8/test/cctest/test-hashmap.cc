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

#include "v8.h"
#include "hashmap.h"
#include "cctest.h"

using namespace v8::internal;

static bool DefaultMatchFun(void* a, void* b) {
  return a == b;
}


class IntSet {
 public:
  IntSet() : map_(DefaultMatchFun)  {}

  void Insert(int x) {
    ASSERT(x != 0);  // 0 corresponds to (void*)NULL - illegal key value
    HashMap::Entry* p = map_.Lookup(reinterpret_cast<void*>(x), Hash(x), true);
    CHECK(p != NULL);  // insert is set!
    CHECK_EQ(reinterpret_cast<void*>(x), p->key);
    // we don't care about p->value
  }

  bool Present(int x) {
    HashMap::Entry* p = map_.Lookup(reinterpret_cast<void*>(x), Hash(x), false);
    if (p != NULL) {
      CHECK_EQ(reinterpret_cast<void*>(x), p->key);
    }
    return p != NULL;
  }

  void Clear() {
    map_.Clear();
  }

  uint32_t occupancy() const {
    uint32_t count = 0;
    for (HashMap::Entry* p = map_.Start(); p != NULL; p = map_.Next(p)) {
      count++;
    }
    CHECK_EQ(map_.occupancy(), static_cast<double>(count));
    return count;
  }

 private:
  HashMap map_;
  static uint32_t Hash(uint32_t key)  { return key * 23; }
};


TEST(Set) {
  IntSet set;
  CHECK_EQ(0, set.occupancy());

  set.Insert(1);
  set.Insert(2);
  set.Insert(3);
  CHECK_EQ(3, set.occupancy());

  set.Insert(2);
  set.Insert(3);
  CHECK_EQ(3, set.occupancy());

  CHECK(set.Present(1));
  CHECK(set.Present(2));
  CHECK(set.Present(3));
  CHECK(!set.Present(4));
  CHECK_EQ(3, set.occupancy());

  set.Clear();
  CHECK_EQ(0, set.occupancy());

  // Insert a long series of values.
  const int start = 453;
  const int factor = 13;
  const int offset = 7;
  const uint32_t n = 1000;

  int x = start;
  for (uint32_t i = 0; i < n; i++) {
    CHECK_EQ(i, static_cast<double>(set.occupancy()));
    set.Insert(x);
    x = x*factor + offset;
  }

  // Verify the same sequence of values.
  x = start;
  for (uint32_t i = 0; i < n; i++) {
    CHECK(set.Present(x));
    x = x*factor + offset;
  }

  CHECK_EQ(n, static_cast<double>(set.occupancy()));
}
