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

// Tests for non-standard array iteration functions.
//
// See
//
// <http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array>
//
// for an explanation of each of the functions.

//
// Array.prototype.filter
//
(function() {
  // Simple use.
  var a = [0,1];
  assertArrayEquals([0], a.filter(function(n) { return n == 0; }));
  assertArrayEquals(a, a);

  // Use specified object as this object when calling the function.
  var o = { value: 42 }
  a = [1,42,3,42,4];
  assertArrayEquals([42,42], a.filter(function(n) { return this.value == n }, o))

  // Modify original array.
  a = [1,42,3,42,4];
  assertArrayEquals([42,42], a.filter(function(n, index, array) { array[index] = 43; return 42 == n; }));
  assertArrayEquals([43,43,43,43,43], a);

  // Only loop through initial part of array eventhough elements are
  // added.
  a = [1,1];
  assertArrayEquals([], a.filter(function(n, index, array) { array.push(n+1); return n == 2; }));
  assertArrayEquals([1,1,2,2], a);

  // Respect holes.
  a = new Array(20);
  var count = 0;
  a[2] = 2;
  a[15] = 2;
  a[17] = 4;
  var a = a.filter(function(n) { count++; return n == 2; });
  assertEquals(3, count);
  for (var i in a) assertEquals(2, a[i]);

})();


//
// Array.prototype.forEach
//
(function() {
  // Simple use.
  var a = [0,1];
  var count = 0;
  a.forEach(function(n) { count++; });
  assertEquals(2, count);

  // Use specified object as this object when calling the function.
  var o = { value: 42 }
  var result = [];
  a.forEach(function(n) { result.push(this.value); }, o);
  assertArrayEquals([42,42], result);

  // Modify original array.
  a = [0,1];
  count = 0;
  a.forEach(function(n, index, array) { array[index] = n + 1; count++; });
  assertEquals(2, count);
  assertArrayEquals([1,2], a);

  // Only loop through initial part of array eventhough elements are
  // added.
  a = [1,1];
  count = 0;
  a.forEach(function(n, index, array) { array.push(n+1); count++; });
  assertEquals(2, count);
  assertArrayEquals([1,1,2,2], a);

  // Respect holes.
  a = new Array(20);
  count = 0;
  a[15] = 2;
  a.forEach(function(n) { count++; });
  assertEquals(1, count);

})();


//
// Array.prototype.every
//
(function() {
  // Simple use.
  var a = [0,1];
  assertFalse(a.every(function(n) { return n == 0 }));
  a = [0,0];
  assertTrue(a.every(function(n) { return n == 0 }));
  assertTrue([].every(function(n) { return n == 0}));

  // Use specified object as this object when calling the function.
  var o = { value: 42 }
  a = [0];
  assertFalse(a.every(function(n) { return this.value == n; }, o));
  a = [42];
  assertTrue(a.every(function(n) { return this.value == n; }, o));

  // Modify original array.
  a = [0,1];
  assertFalse(a.every(function(n, index, array) { array[index] = n + 1; return n == 1;}));
  assertArrayEquals([1,1], a);

  // Only loop through initial part of array eventhough elements are
  // added.
  a = [1,1];
  assertTrue(a.every(function(n, index, array) { array.push(n + 1); return n == 1;}));
  assertArrayEquals([1,1,2,2], a);

  // Respect holes.
  a = new Array(20);
  var count = 0;
  a[2] = 2;
  a[15] = 2;
  assertTrue(a.every(function(n) { count++; return n == 2; }));
  assertEquals(2, count);

})();

//
// Array.prototype.map
//
(function() {
  var a = [0,1,2,3,4];

  // Simple use.
  var result = [1,2,3,4,5];
  assertArrayEquals(result, a.map(function(n) { return n + 1; }));
  assertEquals(a, a);

  // Use specified object as this object when calling the function.
  var o = { delta: 42 }
  result = [42,43,44,45,46];
  assertArrayEquals(result, a.map(function(n) { return this.delta + n; }, o));

  // Modify original array.
  a = [0,1,2,3,4];
  result = [1,2,3,4,5];
  assertArrayEquals(result, a.map(function(n, index, array) { array[index] = n + 1; return n + 1;}));
  assertArrayEquals(result, a);

  // Only loop through initial part of array eventhough elements are
  // added.
  a = [0,1,2,3,4];
  result = [1,2,3,4,5];
  assertArrayEquals(result, a.map(function(n, index, array) { array.push(n); return n + 1;}));
  assertArrayEquals([0,1,2,3,4,0,1,2,3,4], a);

  // Respect holes.
  a = new Array(20);
  a[15] = 2;
  a = a.map(function(n) { return 2*n; });
  for (var i in a) assertEquals(4, a[i]);

})();

//
// Array.prototype.some
//
(function() {
  var a = [0,1,2,3,4];

  // Simple use.
  assertTrue(a.some(function(n) { return n == 3}));
  assertFalse(a.some(function(n) { return n == 5}));

  // Use specified object as this object when calling the function.
  var o = { element: 42 };
  a = [1,42,3];
  assertTrue(a.some(function(n) { return this.element == n; }, o));
  a = [1];
  assertFalse(a.some(function(n) { return this.element == n; }, o));

  // Modify original array.
  a = [0,1,2,3];
  assertTrue(a.some(function(n, index, array) { array[index] = n + 1; return n == 2; }));
  assertArrayEquals([1,2,3,3], a);

  // Only loop through initial part when elements are added.
  a = [0,1,2];
  assertFalse(a.some(function(n, index, array) { array.push(42); return n == 42; }));
  assertArrayEquals([0,1,2,42,42,42], a);

  // Respect holes.
  a = new Array(20);
  var count = 0;
  a[2] = 42;
  a[10] = 2;
  a[15] = 42;
  assertTrue(a.some(function(n) { count++; return n == 2; }));
  assertEquals(2, count);

})();

