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

// Flags: --allow-natives-syntax

// Check that slicing array of holes keeps it as array of holes

(function() {
  var array = new Array(10);
  for (var i = 0; i < 7; i++) {
    var sliced = array.slice();
    assertEquals(array.length, sliced.length);
    assertFalse(0 in sliced);
  }
})();


// Check various variants of empty array's slicing.
(function() {
  for (var i = 0; i < 7; i++) {
    assertEquals([], [].slice(0, 0));
    assertEquals([], [].slice(1, 0));
    assertEquals([], [].slice(0, 1));
    assertEquals([], [].slice(-1, 0));
  }
})();


// Check various forms of arguments omission.
(function() {
  var array = new Array(7);

  for (var i = 0; i < 7; i++) {
    assertEquals(array, array.slice());
    assertEquals(array, array.slice(0));
    assertEquals(array, array.slice(undefined));
    assertEquals(array, array.slice("foobar"));
    assertEquals(array, array.slice(undefined, undefined));
  }
})();


// Check variants of negatives and positive indices.
(function() {
  var array = new Array(7);

  for (var i = 0; i < 7; i++) {
    assertEquals(7, array.slice(-100).length);
    assertEquals(3, array.slice(-3).length);
    assertEquals(3, array.slice(4).length);
    assertEquals(1, array.slice(6).length);
    assertEquals(0, array.slice(7).length);
    assertEquals(0, array.slice(8).length);
    assertEquals(0, array.slice(100).length);

    assertEquals(0, array.slice(0, -100).length);
    assertEquals(4, array.slice(0, -3).length);
    assertEquals(4, array.slice(0, 4).length);
    assertEquals(6, array.slice(0, 6).length);
    assertEquals(7, array.slice(0, 7).length);
    assertEquals(7, array.slice(0, 8).length);
    assertEquals(7, array.slice(0, 100).length);

    // Some exotic cases.

    obj = { toString: function() { throw 'Exception'; } };

    // More than 2 arguments:
    assertEquals(7, array.slice(0, 7, obj, null, undefined).length);

    // Custom conversion:
    assertEquals(1, array.slice({valueOf: function() { return 1; }},
                                {toString: function() { return 2; }}).length);

    // Throwing an exception in conversion:
    try {
      assertEquals(7, array.slice(0, obj).length);
      throw 'Should have thrown';
    } catch (e) {
      assertEquals('Exception', e);
    }
  }
})();


// Nasty: modify the array in ToInteger.
(function() {
  var array = [];
  var expected = []
  bad_guy = { valueOf: function() { array.push(array.length); return -1; } };

  for (var i = 0; i < 13; i++) {
    var sliced = array.slice(bad_guy);
    expected.push(i);
    assertEquals(expected, array);
    // According to the spec (15.4.4.10), length is calculated before
    // performing ToInteger on arguments.
    if (i == 0) {
      assertEquals([], sliced);  // Length was 0, nothing to get.
    } else {
      // Actually out of array [0..i] we get [i - 1] as length is i.
      assertEquals([i - 1], sliced);
    }
  }
})();


// Now check the case with array of holes and some elements on prototype.
// Note: that is important that this test runs before the next one
// as the next one tampers Array.prototype.
(function() {
  var len = 9;
  var array = new Array(len);

  var at3 = "@3";
  var at7 = "@7";

  for (var i = 0; i < 7; i++) {
    var array_proto = [];
    array_proto[3] = at3;
    array_proto[7] = at7;
    array.__proto__ = array_proto;

    assertEquals(len, array.length);
    for (var i = 0; i < array.length; i++) {
      assertEquals(array[i], array_proto[i]);
    }

    var sliced = array.slice();

    assertEquals(len, sliced.length);

    assertTrue(delete array_proto[3]);
    assertTrue(delete array_proto[7]);

    // Note that slice copies values from prototype into the array.
    assertEquals(array[3], undefined);
    assertFalse(array.hasOwnProperty(3));
    assertEquals(sliced[3], at3);
    assertTrue(sliced.hasOwnProperty(3));

    assertEquals(array[7], undefined);
    assertFalse(array.hasOwnProperty(7));
    assertEquals(sliced[7], at7);
    assertTrue(sliced.hasOwnProperty(7));

    // ... but keeps the rest as holes:
    array_proto[5] = "@5";
    assertEquals(array[5], array_proto[5]);
    assertFalse(array.hasOwnProperty(5));
  }
})();


// Now check the case with array of holes and some elements on prototype.
(function() {
  var len = 9;
  var array = new Array(len);

  var at3 = "@3";
  var at7 = "@7";

  for (var i = 0; i < 7; i++) {
    Array.prototype[3] = at3;
    Array.prototype[7] = at7;

    assertEquals(len, array.length);
    for (var i = 0; i < array.length; i++) {
      assertEquals(array[i], Array.prototype[i]);
    }

    var sliced = array.slice();

    assertEquals(len, sliced.length);

    assertTrue(delete Array.prototype[3]);
    assertTrue(delete Array.prototype[7]);

    // Note that slice copies values from prototype into the array.
    assertEquals(array[3], undefined);
    assertFalse(array.hasOwnProperty(3));
    assertEquals(sliced[3], at3);
    assertTrue(sliced.hasOwnProperty(3));

    assertEquals(array[7], undefined);
    assertFalse(array.hasOwnProperty(7));
    assertEquals(sliced[7], at7);
    assertTrue(sliced.hasOwnProperty(7));

    // ... but keeps the rest as holes:
    Array.prototype[5] = "@5";
    assertEquals(array[5], Array.prototype[5]);
    assertFalse(array.hasOwnProperty(5));
    assertEquals(sliced[5], Array.prototype[5]);
    assertFalse(sliced.hasOwnProperty(5));

    assertTrue(delete Array.prototype[5]);
  }
})();

// Check slicing on arguments object.
(function() {
  function func(expected, a0, a1, a2) {
    let result =  Array.prototype.slice.call(arguments, 1);
    %HeapObjectVerify(result);
    %HeapObjectVerify(arguments);
    assertEquals(expected, result);
  }

  func([]);
  func(['a'], 'a');
  func(['a', 1], 'a', 1);
  func(['a', 1, 2, 3, 4, 5], 'a', 1, 2, 3, 4, 5);
  func(['a', 1, undefined], 'a', 1, undefined);
  func(['a', 1, undefined, void(0)], 'a', 1, undefined, void(0));
})();

// Check slicing on arguments object when missing arguments get assigined.
(function() {
  function func(x, y) {
    assertEquals(1, arguments.length);
    assertEquals(undefined, y);
    y = 239;
    assertEquals(1, arguments.length);  // arguments length is the same.
    let result = Array.prototype.slice.call(arguments, 0);
    %HeapObjectVerify(result);
    %HeapObjectVerify(arguments);
    assertEquals([x], result);
  }

  func('a');
})();

// Check slicing on arguments object when length property has been set.
(function() {
  function func(x, y) {
    assertEquals(1, arguments.length);
    arguments.length = 7;
    let result = Array.prototype.slice.call(arguments, 0);
    assertEquals([x,,,,,,,], result);
    %HeapObjectVerify(result);
    %HeapObjectVerify(arguments);
  }

  func('a');
})();

// Check slicing on arguments object when length property has been set to
// some strange value.
(function() {
  function func(x, y) {
    assertEquals(1, arguments.length);
    arguments.length = 'foobar';
    let result = Array.prototype.slice.call(arguments, 0);
    assertEquals([], result);
    %HeapObjectVerify(result);
    %HeapObjectVerify(arguments);
  }

  func('a');
})();

// Check slicing on arguments object when extra argument has been added
// via indexed assignment.
(function() {
  function func(x, y) {
    assertEquals(1, arguments.length);
    arguments[3] = 239;
    let result = Array.prototype.slice.call(arguments, 0);
    assertEquals([x], result);
    %HeapObjectVerify(result);
    %HeapObjectVerify(arguments);
  }

  func('a');
})();

// Check slicing on arguments object when argument has been deleted by index.
(function() {
  function func(x, y, z) {
    assertEquals(3, arguments.length);
    delete arguments[1];
    let result = Array.prototype.slice.call(arguments, 0);
    assertEquals([x,,z], result);
    %HeapObjectVerify(result);
    %HeapObjectVerify(arguments);
  }

  func('a', 'b', 'c');
})();

// Check slicing of holey objects with elements in the prototype
(function() {
  function f() {
    delete arguments[1];
    arguments.__proto__[1] = 5;
    var result = Array.prototype.slice.call(arguments);
    delete arguments.__proto__[1];
    assertEquals([1,5,3], result);
    %HeapObjectVerify(result);
    %HeapObjectVerify(arguments);
  }
  f(1,2,3);
})();
