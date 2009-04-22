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

// Simple splice tests based on webkit layout tests.
var arr = ['a','b','c','d'];
assertArrayEquals(['a','b','c','d'], arr);
assertArrayEquals(['c','d'], arr.splice(2));
assertArrayEquals(['a','b'], arr);
assertArrayEquals(['a','b'], arr.splice(0));
assertArrayEquals([], arr)

arr = ['a','b','c','d'];
assertEquals(undefined, arr.splice())
assertArrayEquals(['a','b','c','d'], arr);
assertArrayEquals(['a','b','c','d'], arr.splice(undefined))
assertArrayEquals([], arr);

arr = ['a','b','c','d'];
assertArrayEquals(['a','b','c','d'], arr.splice(null))
assertArrayEquals([], arr);

arr = ['a','b','c','d'];
assertArrayEquals([], arr.splice(100))
assertArrayEquals(['a','b','c','d'], arr);
assertArrayEquals(['d'], arr.splice(-1))
assertArrayEquals(['a','b','c'], arr);

assertArrayEquals([], arr.splice(2, undefined))
assertArrayEquals([], arr.splice(2, null))
assertArrayEquals([], arr.splice(2, -1))
assertArrayEquals([], arr.splice(2, 0))
assertArrayEquals(['a','b','c'], arr);
assertArrayEquals(['c'], arr.splice(2, 100))
assertArrayEquals(['a','b'], arr);


