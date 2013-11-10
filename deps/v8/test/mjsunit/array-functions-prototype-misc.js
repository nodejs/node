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

/**
 * @fileoverview Test splice, shift, unshift, slice and join on small
 * and large arrays.  Some of these methods are specified such that they
 * should work on other objects too, so we test that too.
 */

var LARGE = 4000000;
var VERYLARGE = 4000000000;

// Nicer for firefox 1.5.  Unless you uncomment the following two lines,
// smjs will appear to hang on this file.
//var LARGE = 40000;
//var VERYLARGE = 40000;

var fourhundredth = LARGE/400;

function PseudoArray() {
};

for (var use_real_arrays = 0; use_real_arrays <= 1; use_real_arrays++) {
  var poses = [0, 140, 20000, VERYLARGE];
  var the_prototype;
  var new_function;
  var push_function;
  var concat_function;
  var slice_function;
  var splice_function;
  var splice_function_2;
  var unshift_function;
  var unshift_function_2;
  var shift_function;
  if (use_real_arrays) {
    new_function = function(length) {
      return new Array(length);
    };
    the_prototype = Array.prototype;
    push_function = function(array, elt) {
      return array.push(elt);
    };
    concat_function = function(array, other) {
      return array.concat(other);
    };
    slice_function = function(array, start, len) {
      return array.slice(start, len);
    };
    splice_function = function(array, start, len) {
      return array.splice(start, len);
    };
    splice_function_2 = function(array, start, len, elt) {
      return array.splice(start, len, elt);
    };
    unshift_function = function(array, elt) {
      return array.unshift(elt);
    };
    unshift_function_2 = function(array, elt1, elt2) {
      return array.unshift(elt1, elt2);
    };
    shift_function = function(array) {
      return array.shift();
    };
  } else {
    // Don't run largest size on non-arrays or we'll be here for ever.
    poses.pop();
    new_function = function(length) {
      var obj = new PseudoArray();
      obj.length = length;
      return obj;
    };
    the_prototype = PseudoArray.prototype;
    push_function = function(array, elt) {
      array[array.length] = elt;
      array.length++;
    };
    concat_function = function(array, other) {
      return Array.prototype.concat.call(array, other);
    };
    slice_function = function(array, start, len) {
      return Array.prototype.slice.call(array, start, len);
    };
    splice_function = function(array, start, len) {
      return Array.prototype.splice.call(array, start, len);
    };
    splice_function_2 = function(array, start, len, elt) {
      return Array.prototype.splice.call(array, start, len, elt);
    };
    unshift_function = function(array, elt) {
      return Array.prototype.unshift.call(array, elt);
    };
    unshift_function_2 = function(array, elt1, elt2) {
      return Array.prototype.unshift.call(array, elt1, elt2);
    };
    shift_function = function(array) {
      return Array.prototype.shift.call(array);
    };
  }

  for (var pos_pos = 0; pos_pos < poses.length; pos_pos++) {
    var pos = poses[pos_pos];
    if (pos > 100) {
      var a = new_function(pos);
      assertEquals(pos, a.length);
      push_function(a, 'foo');
      assertEquals(pos + 1, a.length);
      var b = ['bar'];
      // Delete a huge number of holes.
      var c = splice_function(a, 10, pos - 20);
      assertEquals(pos - 20, c.length);
      assertEquals(21, a.length);
    }

    // Add a numeric property to the prototype of the array class.  This
    // allows us to test some borderline stuff relative to the standard.
    the_prototype["" + (pos + 1)] = 'baz';

    if (use_real_arrays) {
      // It seems quite clear from ECMAScript spec 15.4.4.5.  Just call Get on
      // every integer in the range.
      // IE, Safari get this right.
      // FF, Opera get this wrong.
      var a = ['zero', ,'two'];
      if (pos == 0) {
        assertEquals("zero,baz,two", a.join(","));
      }

      // Concat only applies to real arrays, unlike most of the other methods.
      var a = new_function(pos);
      push_function(a, "con");
      assertEquals("con", a[pos]);
      assertEquals(pos + 1, a.length);
      var b = new_function(0);
      push_function(b, "cat");
      assertEquals("cat", b[0]);
      var ab = concat_function(a, b);
      assertEquals("con", ab[pos]);
      assertEquals(pos + 2, ab.length);
      assertEquals("cat", ab[pos + 1]);
      var ba = concat_function(b, a);
      assertEquals("con", ba[pos + 1]);
      assertEquals(pos + 2, ba.length);
      assertEquals("cat", ba[0]);

      // Join with '' as separator.
      var join = a.join('');
      assertEquals("con", join);
      join = b.join('');
      assertEquals("cat", join);
      join = ab.join('');
      assertEquals("concat", join);
      join = ba.join('');
      assertEquals("catcon", join);

      var sparse = [];
      sparse[pos + 1000] = 'is ';
      sparse[pos + 271828] = 'time ';
      sparse[pos + 31415] = 'the ';
      sparse[pos + 012260199] = 'all ';
      sparse[-1] = 'foo';
      sparse[pos + 22591927] = 'good ';
      sparse[pos + 1618033] = 'for ';
      sparse[pos + 91] = ': Now ';
      sparse[pos + 86720199] = 'men.';
      sparse.hest = 'fisk';

      assertEquals("baz: Now is the time for all good men.", sparse.join(''));
    }

    a = new_function(pos);
    push_function(a, 'zero');
    push_function(a, void 0);
    push_function(a, 'two');

    // Splice works differently from join.
    // IE, Safari get this wrong.
    // FF, Opera get this right.
    // 15.4.4.12 line 24 says the object itself has to have the property...
    var zero = splice_function(a, pos, 1);
    assertEquals("undefined", typeof(a[pos]));
    assertEquals("two", a[pos+1], "pos1:" + pos);
    assertEquals(pos + 2, a.length, "a length");
    assertEquals(1, zero.length, "zero length");
    assertEquals("zero", zero[0]);

    // 15.4.4.12 line 41 says the object itself has to have the property...
    a = new_function(pos);
    push_function(a, 'zero');
    push_function(a, void 0);
    push_function(a, 'two');
    var nothing = splice_function_2(a, pos, 0, 'minus1');
    assertEquals("minus1", a[pos]);
    assertEquals("zero", a[pos+1]);
    assertEquals("undefined", typeof(a[pos+2]), "toot!");
    assertEquals("two", a[pos+3], "pos3");
    assertEquals(pos + 4, a.length);
    assertEquals(1, zero.length);
    assertEquals("zero", zero[0]);

    // 15.4.4.12 line 10 says the object itself has to have the property...
    a = new_function(pos);
    push_function(a, 'zero');
    push_function(a, void 0);
    push_function(a, 'two');
    var one = splice_function(a, pos + 1, 1);
    assertEquals("", one.join(","));
    assertEquals(pos + 2, a.length);
    assertEquals("zero", a[pos]);
    assertEquals("two", a[pos+1]);

    // Set things back to the way they were.
    the_prototype[pos + 1] = undefined;

    // Unshift.
    var a = new_function(pos);
    push_function(a, "foo");
    assertEquals("foo", a[pos]);
    assertEquals(pos + 1, a.length);
    unshift_function(a, "bar");
    assertEquals("foo", a[pos+1]);
    assertEquals(pos + 2, a.length);
    assertEquals("bar", a[0]);
    unshift_function_2(a, "baz", "boo");
    assertEquals("foo", a[pos+3]);
    assertEquals(pos + 4, a.length);
    assertEquals("baz", a[0]);
    assertEquals("boo", a[1]);
    assertEquals("bar", a[2]);

    // Shift.
    var baz = shift_function(a);
    assertEquals("baz", baz);
    assertEquals("boo", a[0]);
    assertEquals(pos + 3, a.length);
    assertEquals("foo", a[pos + 2]);

    // Slice.
    var bar = slice_function(a, 1, 0);  // don't throw an exception please.
    bar = slice_function(a, 1, 2);
    assertEquals("bar", bar[0]);
    assertEquals(1, bar.length);
    assertEquals("bar", a[1]);

  }
}

// Lets see if performance is reasonable.

var a = new Array(LARGE + 10);
for (var i = 0; i < a.length; i += 1000) {
  a[i] = i;
}

// Take something near the end of the array.
for (var i = 0; i < 10; i++) {
  var top = a.splice(LARGE, 5);
  assertEquals(5, top.length);
  assertEquals(LARGE, top[0]);
  assertEquals("undefined", typeof(top[1]));
  assertEquals(LARGE + 5, a.length);
  a.splice(LARGE, 0, LARGE);
  a.length = LARGE + 10;
}

var a = new Array(LARGE + 10);
for (var i = 0; i < a.length; i += fourhundredth) {
  a[i] = i;
}

// Take something near the middle of the array.
for (var i = 0; i < 10; i++) {
  var top = a.splice(LARGE >> 1, 5);
  assertEquals(5, top.length);
  assertEquals(LARGE >> 1, top[0]);
  assertEquals("undefined", typeof(top[1]));
  assertEquals(LARGE + 5, a.length);
  a.splice(LARGE >> 1, 0, LARGE >> 1, void 0, void 0, void 0, void 0);
}


// Test http://b/issue?id=1202711
arr = [0];
arr.length = 2;
Array.prototype[1] = 1;
assertEquals(1, arr.pop());
assertEquals(0, arr.pop());
Array.prototype[1] = undefined;

// Test http://code.google.com/p/chromium/issues/detail?id=21860
Array.prototype.push.apply([], [1].splice(0, -(-1 % 5)));
