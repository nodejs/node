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

// Check that unshifting array of holes keeps the original array
// as array of holes
(function() {
  var array = new Array(10);
  assertEquals(13, array.unshift('1st', '2ns', '3rd'));
  assertTrue(0 in array);
  assertTrue(1 in array);
  assertTrue(2 in array);
  assertFalse(3 in array);
})();


// Check that unshift with no args has no side-effects.
(function() {
  var len = 3;
  var array = new Array(len);

  var at0 = '@0';
  var at2 = '@2';

  Array.prototype[0] = at0;
  Array.prototype[2] = at2;

  // array owns nothing...
  assertFalse(array.hasOwnProperty(0));
  assertFalse(array.hasOwnProperty(1));
  assertFalse(array.hasOwnProperty(2));

  // ... but sees values from Array.prototype
  assertEquals(array[0], at0);
  assertEquals(array[1], undefined);
  assertEquals(array[2], at2);

  assertEquals(len, array.unshift());

  assertTrue(delete Array.prototype[0]);
  assertTrue(delete Array.prototype[2]);

  // array still owns nothing...
  assertFalse(array.hasOwnProperty(0));
  assertFalse(array.hasOwnProperty(1));
  assertFalse(array.hasOwnProperty(2));

  // ... so they are not affected be delete.
  assertEquals(array[0], undefined);
  assertEquals(array[1], undefined);
  assertEquals(array[2], undefined);
})();


// Now check the case with array of holes and some elements on prototype.
(function() {
  var len = 9;
  var array = new Array(len);
  Array.prototype[3] = "@3";
  Array.prototype[7] = "@7";

  assertEquals(len, array.length);
  for (var i = 0; i < array.length; i++) {
    assertEquals(array[i], Array.prototype[i]);
  }

  assertEquals(len + 1, array.unshift('head'));

  assertEquals(len + 1, array.length);
  // Note that unshift copies values from prototype into the array.
  assertEquals(array[4], Array.prototype[3]);
  assertTrue(array.hasOwnProperty(4));

  assertEquals(array[8], Array.prototype[7]);
  assertTrue(array.hasOwnProperty(8));

  // ... but keeps the rest as holes:
  Array.prototype[5] = "@5";
  assertEquals(array[5], Array.prototype[5]);
  assertFalse(array.hasOwnProperty(5));

  assertEquals(array[3], Array.prototype[3]);
  assertFalse(array.hasOwnProperty(3));

  assertEquals(array[7], Array.prototype[7]);
  assertFalse(array.hasOwnProperty(7));

  assertTrue(delete Array.prototype[3]);
  assertTrue(delete Array.prototype[5]);
  assertTrue(delete Array.prototype[7]);
})();

// Check that unshift with no args has no side-effects.
(function() {
  var len = 3;
  var array = new Array(len);

  var at0 = '@0';
  var at2 = '@2';

  var array_proto = [];
  array_proto[0] = at0;
  array_proto[2] = at2;
  array.__proto__ = array_proto;

  // array owns nothing...
  assertFalse(array.hasOwnProperty(0));
  assertFalse(array.hasOwnProperty(1));
  assertFalse(array.hasOwnProperty(2));

  // ... but sees values from array_proto.
  assertEquals(array[0], at0);
  assertEquals(array[1], undefined);
  assertEquals(array[2], at2);

  assertEquals(len, array.unshift());

  // array still owns nothing.
  assertFalse(array.hasOwnProperty(0));
  assertFalse(array.hasOwnProperty(1));
  assertFalse(array.hasOwnProperty(2));

  // ... but still sees values from array_proto.
  assertEquals(array[0], at0);
  assertEquals(array[1], undefined);
  assertEquals(array[2], at2);
})();


// Now check the case with array of holes and some elements on prototype.
(function() {
  var len = 9;
  var array = new Array(len);
  var array_proto = []
  array_proto[3] = "@3";
  array_proto[7] = "@7";
  array.__proto__ = array_proto;

  assertEquals(len, array.length);
  for (var i = 0; i < array.length; i++) {
    assertEquals(array[i], array_proto[i]);
  }

  assertEquals(len + 1, array.unshift('head'));

  assertEquals(len + 1, array.length);
  // Note that unshift copies values from prototype into the array.
  assertEquals(array[4], array_proto[3]);
  assertTrue(array.hasOwnProperty(4));

  assertEquals(array[8], array_proto[7]);
  assertTrue(array.hasOwnProperty(8));

  // ... but keeps the rest as holes:
  array_proto[5] = "@5";
  assertEquals(array[5], array_proto[5]);
  assertFalse(array.hasOwnProperty(5));

  assertEquals(array[3], array_proto[3]);
  assertFalse(array.hasOwnProperty(3));

  assertEquals(array[7], array_proto[7]);
  assertFalse(array.hasOwnProperty(7));
})();

// Check the behaviour when approaching maximal values for length.
(function() {
  for (var i = 0; i < 7; i++) {
    try {
      let obj = { length: 2 ** 53 - 3};
      Array.prototype.unshift.call(obj, 1, 2, 3, 4, 5);
      throw 'Should have thrown TypeError';
    } catch (e) {
      assertTrue(e instanceof TypeError);
    }
  }
})();

(function() {
  for (var i = 0; i < 7; i++) {
    var a = [6, 7, 8, 9];
    a.unshift(1, 2, 3, 4, 5);
    assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], a);
  }
})();

// Check that non-enumerable elements are treated appropriately
(function() {
  var array = [2, 3];
  Object.defineProperty(array, '1', {enumerable: false});
  array.unshift(1)
  assertEquals([1, 2, 3], array);

  array = [2];
  array.length = 2;
  array.__proto__[1] = 3;
  Object.defineProperty(array.__proto__, '1', {enumerable: false});
  array.unshift(1);
  assertEquals([1, 2, 3], array);
})();
