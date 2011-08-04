// Copyright 2011 the V8 project authors. All rights reserved.
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
#include <string.h>
#include "v8.h"
#include "cctest.h"

using namespace v8::internal;

// Use a testing allocator that clears memory before deletion.
class ZeroingAllocationPolicy {
 public:
  static void* New(size_t size) {
    // Stash the size in the first word to use for Delete.
    size_t true_size = size + sizeof(size_t);
    size_t* result = reinterpret_cast<size_t*>(malloc(true_size));
    if (result == NULL) return result;
    *result = true_size;
    return result + 1;
  }

  static void Delete(void* ptr) {
    size_t* true_ptr = reinterpret_cast<size_t*>(ptr) - 1;
    memset(true_ptr, 0, *true_ptr);
    free(true_ptr);
  }
};

// Check that we can add (a reference to) an element of the list
// itself.
TEST(ListAdd) {
  // Add elements to the list to grow it to its capacity.
  List<int, ZeroingAllocationPolicy> list(4);
  list.Add(1);
  list.Add(2);
  list.Add(3);
  list.Add(4);

  // Add an existing element, the backing store should have to grow.
  list.Add(list[0]);
  CHECK_EQ(1, list[4]);
}

// Test that we can add all elements from a list to another list.
TEST(ListAddAll) {
  List<int, ZeroingAllocationPolicy> list(4);
  list.Add(0);
  list.Add(1);
  list.Add(2);

  CHECK_EQ(3, list.length());
  for (int i = 0; i < 3; i++) {
    CHECK_EQ(i, list[i]);
  }

  List<int, ZeroingAllocationPolicy> other_list(4);

  // Add no elements to list since other_list is empty.
  list.AddAll(other_list);
  CHECK_EQ(3, list.length());
  for (int i = 0; i < 3; i++) {
    CHECK_EQ(i, list[i]);
  }

  // Add three elements to other_list.
  other_list.Add(0);
  other_list.Add(1);
  other_list.Add(2);

  // Copy the three elements from other_list to list.
  list.AddAll(other_list);
  CHECK_EQ(6, list.length());
  for (int i = 0; i < 6; i++) {
    CHECK_EQ(i % 3, list[i]);
  }
}


TEST(RemoveLast) {
  List<int> list(4);
  CHECK_EQ(0, list.length());
  list.Add(1);
  CHECK_EQ(1, list.length());
  CHECK_EQ(1, list.last());
  list.RemoveLast();
  CHECK_EQ(0, list.length());
  list.Add(2);
  list.Add(3);
  CHECK_EQ(2, list.length());
  CHECK_EQ(3, list.last());
  list.RemoveLast();
  CHECK_EQ(1, list.length());
  CHECK_EQ(2, list.last());
  list.RemoveLast();
  CHECK_EQ(0, list.length());

  const int kElements = 100;
  for (int i = 0; i < kElements; i++) list.Add(i);
  for (int j = kElements - 1; j >= 0; j--) {
    CHECK_EQ(j + 1, list.length());
    CHECK_EQ(j, list.last());
    list.RemoveLast();
    CHECK_EQ(j, list.length());
  }
}


TEST(Clear) {
  List<int> list(4);
  CHECK_EQ(0, list.length());
  for (int i = 0; i < 4; ++i) list.Add(i);
  CHECK_EQ(4, list.length());
  list.Clear();
  CHECK_EQ(0, list.length());
}


TEST(DeleteEmpty) {
  {
    List<int>* list = new List<int>(0);
    delete list;
  }
  {
    List<int> list(0);
  }
}
