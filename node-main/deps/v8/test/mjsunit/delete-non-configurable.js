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

// Delete elements of a String object.
var TIPLI = "tipli"
var so = new String(TIPLI);
var length = so.length;

for (var i = 0; i < length; i++) {
  assertFalse(delete so[i]);
  assertThrows("'use strict'; delete so[i];", TypeError);
  assertFalse(delete so[i.toString()]);
  assertThrows("'use strict'; delete so[i.toString()];", TypeError);
}

assertEquals(length, so.length);
assertEquals(new String(TIPLI), so);

// Delete elements of an Array.
var arr = new Array(length);

for (var i = 0; i < length; i++) {
  arr[i] = i;
  Object.defineProperty(arr, i, { configurable: false });
}

for (var i = 0; i < length; i++) {
  assertFalse(delete arr[i]);
  assertThrows("'use strict'; delete arr[i];", TypeError);
  assertFalse(delete arr[i.toString()]);
  assertThrows("'use strict'; delete arr[i.toString()];", TypeError);
  assertEquals(i, arr[i]);
}

assertEquals(length, arr.length);
assertTrue(delete arr[length]);

// Delete an element of an Object.
var INDEX = 28;
var obj = new Object();

obj[INDEX] = TIPLI;
Object.defineProperty(obj, INDEX, { configurable: false });

assertFalse(delete obj[INDEX]);
assertThrows("'use strict'; delete obj[INDEX];", TypeError);
assertFalse(delete obj[INDEX.toString()]);
assertThrows("'use strict'; delete obj[INDEX.toString()];", TypeError);
assertEquals(TIPLI, obj[INDEX]);
assertTrue(delete arr[INDEX+1]);
