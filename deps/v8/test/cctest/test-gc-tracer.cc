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

#include <stdlib.h>
#include <utility>

#include "src/v8.h"

#include "test/cctest/cctest.h"

using namespace v8::internal;

TEST(RingBufferPartialFill) {
  const int max_size = 6;
  typedef RingBuffer<int, max_size>::const_iterator Iter;
  RingBuffer<int, max_size> ring_buffer;
  CHECK(ring_buffer.empty());
  CHECK_EQ(static_cast<int>(ring_buffer.size()), 0);
  CHECK(ring_buffer.begin() == ring_buffer.end());

  // Fill ring_buffer partially: [0, 1, 2]
  for (int i = 0; i < max_size / 2; i++) ring_buffer.push_back(i);

  CHECK(!ring_buffer.empty());
  CHECK(static_cast<int>(ring_buffer.size()) == max_size / 2);
  CHECK(ring_buffer.begin() != ring_buffer.end());

  // Test forward itartion
  int i = 0;
  for (Iter iter = ring_buffer.begin(); iter != ring_buffer.end(); ++iter) {
    CHECK(*iter == i);
    ++i;
  }
  CHECK_EQ(i, 3);  // one past last element.

  // Test backward iteration
  i = 2;
  Iter iter = ring_buffer.back();
  while (true) {
    CHECK(*iter == i);
    if (iter == ring_buffer.begin()) break;
    --iter;
    --i;
  }
  CHECK_EQ(i, 0);
}


TEST(RingBufferWrapAround) {
  const int max_size = 6;
  typedef RingBuffer<int, max_size>::const_iterator Iter;
  RingBuffer<int, max_size> ring_buffer;

  // Fill ring_buffer (wrap around): [9, 10, 11, 12, 13, 14]
  for (int i = 0; i < 2 * max_size + 3; i++) ring_buffer.push_back(i);

  CHECK(!ring_buffer.empty());
  CHECK(static_cast<int>(ring_buffer.size()) == max_size);
  CHECK(ring_buffer.begin() != ring_buffer.end());

  // Test forward iteration
  int i = 9;
  for (Iter iter = ring_buffer.begin(); iter != ring_buffer.end(); ++iter) {
    CHECK(*iter == i);
    ++i;
  }
  CHECK_EQ(i, 15);  // one past last element.

  // Test backward iteration
  i = 14;
  Iter iter = ring_buffer.back();
  while (true) {
    CHECK(*iter == i);
    if (iter == ring_buffer.begin()) break;
    --iter;
    --i;
  }
  CHECK_EQ(i, 9);
}


TEST(RingBufferPushFront) {
  const int max_size = 6;
  typedef RingBuffer<int, max_size>::const_iterator Iter;
  RingBuffer<int, max_size> ring_buffer;

  // Fill ring_buffer (wrap around): [14, 13, 12, 11, 10, 9]
  for (int i = 0; i < 2 * max_size + 3; i++) ring_buffer.push_front(i);

  CHECK(!ring_buffer.empty());
  CHECK(static_cast<int>(ring_buffer.size()) == max_size);
  CHECK(ring_buffer.begin() != ring_buffer.end());

  // Test forward iteration
  int i = 14;
  for (Iter iter = ring_buffer.begin(); iter != ring_buffer.end(); ++iter) {
    CHECK(*iter == i);
    --i;
  }
  CHECK_EQ(i, 8);  // one past last element.
}
