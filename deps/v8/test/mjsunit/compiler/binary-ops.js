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

// Values in distinct spans.
function or_test0(x, y) { return x | y; }
function and_test0(x, y) { return x & y; }
function add_test0(x, y) { return x + y; }

assertEquals(3, or_test0(1, 2));   // 1 | 2
assertEquals(2, and_test0(3, 6));  // 3 & 6
assertEquals(5, add_test0(2, 3));  // 2 + 3


// Values in the same span.
function or_test1(x, y) { return x | x; }
function and_test1(x, y) { return x & x; }
function add_test1(x, y) { return x + x; }

assertEquals(1, or_test1(1, 2));   // 1 | 1
assertEquals(3, and_test1(3, 6));  // 3 & 3
assertEquals(4, add_test1(2, 3));  // 2 + 2


// Values in distinct spans that alias.
function or_test2(x, y) { x = y; return x | y; }
function and_test2(x, y) { x = y; return x & y; }
function add_test2(x, y) { x = y; return x + y; }

assertEquals(2, or_test2(1, 2));   // 2 | 2
assertEquals(6, and_test2(3, 6));  // 6 & 6
assertEquals(6, add_test2(2, 3));  // 3 + 3
