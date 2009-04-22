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

// This file contains a number of tests of array functions and their
// interaction with properties in the prototype chain.
//
// The behavior of SpiderMonkey is slightly different for arrays (see
// below).  Our behavior is consistent and matches the bahavior of
// KJS.

var proto = { length:3, 0:'zero', 1:'one', 2:'two' }
function constructor() {};
constructor.prototype = proto;

// Set elements on the array prototype.
Array.prototype[0] = 'zero';
Array.prototype[1] = 'one';
Array.prototype[2] = 'two';

// ----------------------------------------------------------------------
// Helper functions.
// ----------------------------------------------------------------------
function assertHasOwnProperties(object, limit) {
  for (var i = 0; i < limit; i++) {
    assertTrue(object.hasOwnProperty(i));
  }
}


// ----------------------------------------------------------------------
// shift.
// ----------------------------------------------------------------------

function runTest() {
  var nonArray = new constructor();
  var array = ['zero', , 'two'];
  // Shift away the zero.
  assertEquals('zero', array.shift());
  assertEquals('zero', Array.prototype.shift.call(nonArray));
  // Check that the local object has properties 0 and 1 with the right
  // values.
  assertEquals(2, array.length);
  assertEquals(2, nonArray.length);
  assertHasOwnProperties(array, 2);
  assertHasOwnProperties(nonArray, 2);
  // Note: Spidermonkey is inconsistent here.  It treats arrays
  // differently from non-arrays.  It only consults the prototype for
  // non-arrays.  Therefore, array[0] is undefined in Spidermonkey and
  // 'one' in V8 and KJS.
  assertEquals('one', array[0]);
  assertEquals('one', nonArray[0]);
  assertEquals('two', array[1]);
  assertEquals('two', nonArray[1]);
  // Get index 2 from the prototype.
  assertEquals('two', array[2]);
  assertEquals('two', nonArray[2]);
}

runTest();

// ----------------------------------------------------------------------
// unshift.
// ----------------------------------------------------------------------

runTest = function() {
  var nonArray = new constructor();
  var array = ['zero', , 'two'];
  // Unshift a new 'zero'.
  assertEquals(4, array.unshift('zero'));
  assertEquals(4, Array.prototype.unshift.call(nonArray, 'zero'));
  // Check that the local object has properties 0 through 3 with the
  // right values.
  assertEquals(4, array.length);
  assertEquals(4, nonArray.length);
  assertHasOwnProperties(array, 4);
  assertHasOwnProperties(nonArray, 4);
  assertEquals('zero', array[0]);
  assertEquals('zero', nonArray[0]);
  assertEquals('zero', array[1]);
  assertEquals('zero', nonArray[1]);
  // Again Spidermonkey is inconsistent.  array[2] is undefined
  // instead of 'one'.
  assertEquals('one', array[2]);
  assertEquals('one', nonArray[2]);
  assertEquals('two', array[3]);
  assertEquals('two', nonArray[3]);
}

runTest();


// ----------------------------------------------------------------------
// splice
// ----------------------------------------------------------------------

runTest = function() {
  var nonArray = new constructor();
  var array = ['zero', , 'two'];
  // Delete the first element by splicing in nothing.
  assertArrayEquals(['zero'], array.splice(0, 1));
  assertArrayEquals(['zero'], Array.prototype.splice.call(nonArray, 0, 1));
  // Check that the local object has properties 0 and 1 with the right
  // values.
  assertEquals(2, array.length);
  assertEquals(2, nonArray.length);
  assertHasOwnProperties(array, 2);
  assertHasOwnProperties(nonArray, 2);
  // Again Spidermonkey is inconsistent.  array[0] is undefined
  // instead of 'one'.
  assertEquals('one', array[0]);
  assertEquals('one', nonArray[0]);
  assertEquals('two', array[1]);
  assertEquals('two', nonArray[1]);
  // Get index 2 from the prototype.
  assertEquals('two', array[2]);
  assertEquals('two', nonArray[2]);
};

runTest();


// ----------------------------------------------------------------------
// slice
// ----------------------------------------------------------------------

runTest = function() {
  var nonArray = new constructor();
  var array = ['zero', , 'two'];
  // Again Spidermonkey is inconsistent.  (array.slice(0, 3))[1] is
  // undefined instead of 'one'.
  assertArrayEquals(['zero', 'one', 'two'], array.slice(0, 3));
  assertArrayEquals(['zero', 'one', 'two'], Array.prototype.slice.call(nonArray, 0, 3));
};

runTest();
