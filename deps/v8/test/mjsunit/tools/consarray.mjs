// Copyright 2009 the V8 project authors. All rights reserved.
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

import { ConsArray } from "../../../tools/consarray.mjs";

var arr1 = new ConsArray();
assertTrue(arr1.atEnd());

arr1.concat([]);
assertTrue(arr1.atEnd());

arr1.concat([1]);
assertFalse(arr1.atEnd());
assertEquals(1, arr1.next());
assertTrue(arr1.atEnd());

arr1.concat([2, 3, 4]);
arr1.concat([5]);
arr1.concat([]);
arr1.concat([6, 7]);

assertFalse(arr1.atEnd());
assertEquals(2, arr1.next());
assertFalse(arr1.atEnd());
assertEquals(3, arr1.next());
assertFalse(arr1.atEnd());
assertEquals(4, arr1.next());
assertFalse(arr1.atEnd());
assertEquals(5, arr1.next());
assertFalse(arr1.atEnd());
assertEquals(6, arr1.next());
assertFalse(arr1.atEnd());
assertEquals(7, arr1.next());
assertTrue(arr1.atEnd());
