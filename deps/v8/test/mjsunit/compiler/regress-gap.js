// Copyright 2010 the V8 project authors. All rights reserved.
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

// Regression test that stresses the register allocator gap instruction.

function small_select(n, v1, v2) {
  for (var i = 0; i < n; ++i) {
    var tmp = v1;
    v1 = v2;
    v2 = tmp;
  }
  return v1;
}

function select(n, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) {
  for (var i = 0; i < n; ++i) {
    var tmp = v1;
    v1 = v2;
    v2 = v3;
    v3 = v4;
    v4 = v5;
    v5 = v6;
    v6 = v7;
    v7 = v8;
    v8 = v9;
    v9 = v10;
    v10 = tmp;
  }
  return v1;
}

function select_while(n, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) {
  var i = 0;
  while (i < n) {
    var tmp = v1;
    v1 = v2;
    v2 = v3;
    v3 = v4;
    v4 = v5;
    v5 = v6;
    v6 = v7;
    v7 = v8;
    v8 = v9;
    v9 = v10;
    v10 = tmp;
    i++;
  }
  return v1;
}

function two_cycles(n, v1, v2, v3, v4, v5, x1, x2, x3, x4, x5) {
  for (var i = 0; i < n; ++i) {
    var tmp = v1;
    v1 = v2;
    v2 = v3;
    v3 = v4;
    v4 = v5;
    v5 = tmp;
    tmp = x1;
    x1 = x2;
    x2 = x3;
    x3 = x4;
    x4 = x5;
    x5 = tmp;
  }
  return v1 + x1;
}

function two_cycles_while(n, v1, v2, v3, v4, v5, x1, x2, x3, x4, x5) {
  var i = 0;
  while (i < n) {
    var tmp = v1;
    v1 = v2;
    v2 = v3;
    v3 = v4;
    v4 = v5;
    v5 = tmp;
    tmp = x1;
    x1 = x2;
    x2 = x3;
    x3 = x4;
    x4 = x5;
    x5 = tmp;
    i++;
  }
  return v1 + x1;
}
assertEquals(1, small_select(0, 1, 2));
assertEquals(2, small_select(1, 1, 2));
assertEquals(1, small_select(10, 1, 2));

assertEquals(1, select(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(4, select(3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(10, select(9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));

assertEquals(1 + 6, two_cycles(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(4 + 9, two_cycles(3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(5 + 10, two_cycles(9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));

assertEquals(1, select_while(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(4, select_while(3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(10, select_while(9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));

assertEquals(1 + 6, two_cycles_while(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(4 + 9, two_cycles_while(3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
assertEquals(5 + 10, two_cycles_while(9, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
