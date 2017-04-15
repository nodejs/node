// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "src/bit-vector.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

TEST(BitVector) {
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  {
    BitVector v(15, &zone);
    v.Add(1);
    CHECK(v.Contains(1));
    v.Remove(0);
    CHECK(!v.Contains(0));
    v.Add(0);
    v.Add(1);
    BitVector w(15, &zone);
    w.Add(1);
    v.Intersect(w);
    CHECK(!v.Contains(0));
    CHECK(v.Contains(1));
  }

  {
    BitVector v(64, &zone);
    v.Add(27);
    v.Add(30);
    v.Add(31);
    v.Add(33);
    BitVector::Iterator iter(&v);
    CHECK_EQ(27, iter.Current());
    iter.Advance();
    CHECK_EQ(30, iter.Current());
    iter.Advance();
    CHECK_EQ(31, iter.Current());
    iter.Advance();
    CHECK_EQ(33, iter.Current());
    iter.Advance();
    CHECK(iter.Done());
  }

  {
    BitVector v(15, &zone);
    v.Add(0);
    BitVector w(15, &zone);
    w.Add(1);
    v.Union(w);
    CHECK(v.Contains(0));
    CHECK(v.Contains(1));
  }

  {
    BitVector v(15, &zone);
    v.Add(0);
    BitVector w(15, &zone);
    w.CopyFrom(v);
    CHECK(w.Contains(0));
    w.Add(1);
    BitVector u(w, &zone);
    CHECK(u.Contains(0));
    CHECK(u.Contains(1));
    v.Union(w);
    CHECK(v.Contains(0));
    CHECK(v.Contains(1));
  }

  {
    BitVector v(35, &zone);
    v.Add(0);
    BitVector w(35, &zone);
    w.Add(33);
    v.Union(w);
    CHECK(v.Contains(0));
    CHECK(v.Contains(33));
  }

  {
    BitVector v(35, &zone);
    v.Add(32);
    v.Add(33);
    BitVector w(35, &zone);
    w.Add(33);
    v.Intersect(w);
    CHECK(!v.Contains(32));
    CHECK(v.Contains(33));
    BitVector r(35, &zone);
    r.CopyFrom(v);
    CHECK(!r.Contains(32));
    CHECK(r.Contains(33));
  }
}
