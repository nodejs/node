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

// Check that splicing array of holes keeps it as array of holes
(function() {
  for (var i = 0; i < 7; i++) {
    var array = new Array(10);
    var spliced = array.splice(1, 1, 'one', 'two');
    assertEquals(1, spliced.length);
    assertFalse(0 in spliced, "0 in spliced");

    assertEquals(11, array.length);
    assertFalse(0 in array, "0 in array");
    assertTrue(1 in array);
    assertTrue(2 in array);
    assertFalse(3 in array, "3 in array");
  }
})();


// Check various variants of empty array's splicing.
(function() {
  for (var i = 0; i < 7; i++) {
    assertEquals([], [].splice(0, 0));
    assertEquals([], [].splice(1, 0));
    assertEquals([], [].splice(0, 1));
    assertEquals([], [].splice(-1, 0));
  }
})();


// Check that even if result array is empty, receiver gets sliced.
(function() {
  for (var i = 0; i < 7; i++) {
    var a = [1, 2, 3];
    assertEquals([], a.splice(1, 0, 'a', 'b', 'c'));
    assertEquals([1, 'a', 'b', 'c', 2, 3], a);
  }
})();


// Check various forms of arguments omission.
(function() {
  var array;
  for (var i = 0; i < 7; i++) {
    // SpiderMonkey and JSC return undefined in the case where no
    // arguments are given instead of using the implicit undefined
    // arguments.  This does not follow ECMA-262, but we do the same for
    // compatibility.
    // TraceMonkey follows ECMA-262 though.
    array = [1, 2, 3]
    assertEquals(undefined, array.splice());
    assertEquals([1, 2, 3], array);

    // SpiderMonkey, TraceMonkey and JSC treat the case where no delete count is
    // given differently from when an undefined delete count is given.
    // This does not follow ECMA-262, but we do the same for
    // compatibility.
    array = [1, 2, 3]
    assertEquals([1, 2, 3], array.splice(0));
    assertEquals([], array);

    array = [1, 2, 3]
    assertEquals([1, 2, 3], array.splice(undefined));
    assertEquals([], array);

    array = [1, 2, 3]
    assertEquals([1, 2, 3], array.splice("foobar"));
    assertEquals([], array);

    array = [1, 2, 3]
    assertEquals([], array.splice(undefined, undefined));
    assertEquals([1, 2, 3], array);

    array = [1, 2, 3]
    assertEquals([], array.splice("foobar", undefined));
    assertEquals([1, 2, 3], array);

    array = [1, 2, 3]
    assertEquals([], array.splice(undefined, "foobar"));
    assertEquals([1, 2, 3], array);

    array = [1, 2, 3]
    assertEquals([], array.splice("foobar", "foobar"));
    assertEquals([1, 2, 3], array);
  }
})();


// Check variants of negatives and positive indices.
(function() {
  var array, spliced;
  for (var i = 0; i < 7; i++) {
    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(-100);
    assertEquals([], array);
    assertEquals([1, 2, 3, 4, 5, 6, 7], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(-3);
    assertEquals([1, 2, 3, 4], array);
    assertEquals([5, 6, 7], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(4);
    assertEquals([1, 2, 3, 4], array);
    assertEquals([5, 6, 7], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(6);
    assertEquals([1, 2, 3, 4, 5, 6], array);
    assertEquals([7], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(7);
    assertEquals([1, 2, 3, 4, 5, 6, 7], array);
    assertEquals([], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(8);
    assertEquals([1, 2, 3, 4, 5, 6, 7], array);
    assertEquals([], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(100);
    assertEquals([1, 2, 3, 4, 5, 6, 7], array);
    assertEquals([], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(0, -100);
    assertEquals([1, 2, 3, 4, 5, 6, 7], array);
    assertEquals([], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(0, -3);
    assertEquals([1, 2, 3, 4, 5, 6, 7], array);
    assertEquals([], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(0, 4);
    assertEquals([5, 6, 7], array);
    assertEquals([1, 2, 3, 4], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(0, 6);
    assertEquals([7], array);
    assertEquals([1, 2, 3, 4, 5, 6], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(0, 7);
    assertEquals([], array);
    assertEquals([1, 2, 3, 4, 5, 6, 7], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(0, 8);
    assertEquals([], array);
    assertEquals([1, 2, 3, 4, 5, 6, 7], spliced);

    array = [1, 2, 3, 4, 5, 6, 7];
    spliced = array.splice(0, 100);
    assertEquals([], array);
    assertEquals([1, 2, 3, 4, 5, 6, 7], spliced);

    // Some exotic cases.
    obj = { toString: function() { throw 'Exception'; } };

    // Throwing an exception in conversion:
    try {
      [1, 2, 3].splice(obj, 3);
      throw 'Should have thrown';
    } catch (e) {
      assertEquals('Exception', e);
    }

    try {
      [1, 2, 3].splice(0, obj, 3);
      throw 'Should have thrown';
    } catch (e) {
      assertEquals('Exception', e);
    }

    array = [1, 2, 3];
    array.splice(0, 3, obj);
    assertEquals(1, array.length);

    // Custom conversion:
    array = [1, 2, 3];
    spliced = array.splice({valueOf: function() { return 1; }},
                           {toString: function() { return 2; }},
                           'one', 'two');
    assertEquals([2, 3], spliced);
    assertEquals([1, 'one', 'two'], array);
  }
})();


// Nasty: modify the array in ToInteger.
(function() {
  var array = [];
  var spliced;

  for (var i = 0; i < 13; i++) {
    bad_start = { valueOf: function() { array.push(2*i); return -1; } };
    bad_count = { valueOf: function() { array.push(2*i + 1); return 1; } };
    spliced = array.splice(bad_start, bad_count);
    // According to the spec (15.4.4.12), length is calculated before
    // performing ToInteger on arguments.  However, v8 ignores elements
    // we add while converting, so we need corrective pushes.
    array.push(2*i); array.push(2*i + 1);
    if (i == 0) {
      assertEquals([], spliced);  // Length was 0, nothing to get.
      assertEquals([0, 1], array);
    } else {
      // When we start splice, array is [0 .. 2*i - 1], so we get
      // as a result [2*i], this element is removed from the array,
      // but [2 * i, 2 * i + 1] are added.
      assertEquals([2 * i - 1], spliced);
      assertEquals(2 * i, array[i]);
      assertEquals(2 * i + 1, array[i + 1]);
    }
  }
})();


// Now check the case with array of holes and some elements on prototype.
(function() {
  var len = 9;

  var at3 = "@3";
  var at7 = "@7";

  for (var i = 0; i < 7; i++) {
    var array = new Array(len);
    Array.prototype[3] = at3;
    Array.prototype[7] = at7;

    var spliced = array.splice(2, 2, 'one', undefined, 'two');

    // Second hole (at index 3) of array turns into
    // value of Array.prototype[3] while copying.
    assertEquals([, at3], spliced);
    assertEquals([, , 'one', undefined, 'two', , , at7, at7, ,], array);

    // ... but array[7] is actually a hole:
    assertTrue(delete Array.prototype[7]);
    assertEquals(undefined, array[7]);

    // and now check hasOwnProperty
    assertFalse(array.hasOwnProperty(0), "array.hasOwnProperty(0)");
    assertFalse(array.hasOwnProperty(1), "array.hasOwnProperty(1)");
    assertTrue(array.hasOwnProperty(2));
    assertTrue(array.hasOwnProperty(3));
    assertTrue(array.hasOwnProperty(4));
    assertFalse(array.hasOwnProperty(5), "array.hasOwnProperty(5)");
    assertFalse(array.hasOwnProperty(6), "array.hasOwnProperty(6)");
    assertFalse(array.hasOwnProperty(7), "array.hasOwnProperty(7)");
    assertTrue(array.hasOwnProperty(8));
    assertFalse(array.hasOwnProperty(9), "array.hasOwnProperty(9)");

    // and now check couple of indices above length.
    assertFalse(array.hasOwnProperty(10), "array.hasOwnProperty(10)");
    assertFalse(array.hasOwnProperty(15), "array.hasOwnProperty(15)");
    assertFalse(array.hasOwnProperty(31), "array.hasOwnProperty(31)");
    assertFalse(array.hasOwnProperty(63), "array.hasOwnProperty(63)");
    assertFalse(array.hasOwnProperty(2 << 32 - 1), "array.hasOwnProperty(2 << 31 - 1)");
  }
})();


// Check the behaviour when approaching maximal values for length.
(function() {
  for (var i = 0; i < 7; i++) {
    try {
      new Array((1 << 32) - 3).splice(-1, 0, 1, 2, 3, 4, 5);
      throw 'Should have thrown RangeError';
    } catch (e) {
      assertTrue(e instanceof RangeError);
    }

    // Check smi boundary
    var bigNum = (1 << 30) - 3;
    var array = new Array(bigNum);
    array.splice(-1, 0, 1, 2, 3, 4, 5, 6, 7);
    assertEquals(bigNum + 7, array.length);
  }
})();

(function() {
  for (var i = 0; i < 7; i++) {
    var a = [7, 8, 9];
    a.splice(0, 0, 1, 2, 3, 4, 5, 6);
    assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], a);
    assertFalse(a.hasOwnProperty(10), "a.hasOwnProperty(10)");
    assertEquals(undefined, a[10]);
  }
})();
