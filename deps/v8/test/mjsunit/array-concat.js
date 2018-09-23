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
 * @fileoverview Test concat on small and large arrays
 */

var poses;

poses = [140, 4000000000];
while (pos = poses.shift()) {
  var a = new Array(pos);
  var array_proto = [];
  a.__proto__ = array_proto;
  assertEquals(pos, a.length);
  a.push('foo');
  assertEquals(pos + 1, a.length);
  var b = ['bar'];
  var c = a.concat(b);
  assertEquals(pos + 2, c.length);
  assertEquals("undefined", typeof(c[pos - 1]));
  assertEquals("foo", c[pos]);
  assertEquals("bar", c[pos + 1]);

  // Can we fool the system by putting a number in a string?
  var onetwofour = "124";
  a[onetwofour] = 'doo';
  assertEquals(a[124], 'doo');
  c = a.concat(b);
  assertEquals(c[124], 'doo');

  // If we put a number in the prototype, then the spec says it should be
  // copied on concat.
  array_proto["123"] = 'baz';
  assertEquals(a[123], 'baz');

  c = a.concat(b);
  assertEquals(pos + 2, c.length);
  assertEquals("baz", c[123]);
  assertEquals("undefined", typeof(c[pos - 1]));
  assertEquals("foo", c[pos]);
  assertEquals("bar", c[pos + 1]);

  // When we take the number off the prototype it disappears from a, but
  // the concat put it in c itself.
  array_proto["123"] = undefined;
  assertEquals("undefined", typeof(a[123]));
  assertEquals("baz", c[123]);

  // If the element of prototype is shadowed, the element on the instance
  // should be copied, but not the one on the prototype.
  array_proto[123] = 'baz';
  a[123] = 'xyz';
  assertEquals('xyz', a[123]);
  c = a.concat(b);
  assertEquals('xyz', c[123]);

  // Non-numeric properties on the prototype or the array shouldn't get
  // copied.
  array_proto.moe = 'joe';
  a.ben = 'jerry';
  assertEquals(a["moe"], 'joe');
  assertEquals(a["ben"], 'jerry');
  c = a.concat(b);
  // ben was not copied
  assertEquals("undefined", typeof(c.ben));

  // When we take moe off the prototype it disappears from all arrays.
  array_proto.moe = undefined;
  assertEquals("undefined", typeof(c.moe));

  // Negative indices don't get concated.
  a[-1] = 'minus1';
  assertEquals("minus1", a[-1]);
  assertEquals("undefined", typeof(a[0xffffffff]));
  c = a.concat(b);
  assertEquals("undefined", typeof(c[-1]));
  assertEquals("undefined", typeof(c[0xffffffff]));
  assertEquals(c.length, a.length + 1);
}

poses = [140, 4000000000];
while (pos = poses.shift()) {
  var a = new Array(pos);
  assertEquals(pos, a.length);
  a.push('foo');
  assertEquals(pos + 1, a.length);
  var b = ['bar'];
  var c = a.concat(b);
  assertEquals(pos + 2, c.length);
  assertEquals("undefined", typeof(c[pos - 1]));
  assertEquals("foo", c[pos]);
  assertEquals("bar", c[pos + 1]);

  // Can we fool the system by putting a number in a string?
  var onetwofour = "124";
  a[onetwofour] = 'doo';
  assertEquals(a[124], 'doo');
  c = a.concat(b);
  assertEquals(c[124], 'doo');

  // If we put a number in the prototype, then the spec says it should be
  // copied on concat.
  Array.prototype["123"] = 'baz';
  assertEquals(a[123], 'baz');

  c = a.concat(b);
  assertEquals(pos + 2, c.length);
  assertEquals("baz", c[123]);
  assertEquals("undefined", typeof(c[pos - 1]));
  assertEquals("foo", c[pos]);
  assertEquals("bar", c[pos + 1]);

  // When we take the number off the prototype it disappears from a, but
  // the concat put it in c itself.
  Array.prototype["123"] = undefined;
  assertEquals("undefined", typeof(a[123]));
  assertEquals("baz", c[123]);

  // If the element of prototype is shadowed, the element on the instance
  // should be copied, but not the one on the prototype.
  Array.prototype[123] = 'baz';
  a[123] = 'xyz';
  assertEquals('xyz', a[123]);
  c = a.concat(b);
  assertEquals('xyz', c[123]);

  // Non-numeric properties on the prototype or the array shouldn't get
  // copied.
  Array.prototype.moe = 'joe';
  a.ben = 'jerry';
  assertEquals(a["moe"], 'joe');
  assertEquals(a["ben"], 'jerry');
  c = a.concat(b);
  // ben was not copied
  assertEquals("undefined", typeof(c.ben));
  // moe was not copied, but we can see it through the prototype
  assertEquals("joe", c.moe);

  // When we take moe off the prototype it disappears from all arrays.
  Array.prototype.moe = undefined;
  assertEquals("undefined", typeof(c.moe));

  // Negative indices don't get concated.
  a[-1] = 'minus1';
  assertEquals("minus1", a[-1]);
  assertEquals("undefined", typeof(a[0xffffffff]));
  c = a.concat(b);
  assertEquals("undefined", typeof(c[-1]));
  assertEquals("undefined", typeof(c[0xffffffff]));
  assertEquals(c.length, a.length + 1);

}

a = [];
c = a.concat('Hello');
assertEquals(1, c.length);
assertEquals("Hello", c[0]);
assertEquals("Hello", c.toString());

// Check that concat preserves holes.
var holey = [void 0,'a',,'c'].concat(['d',,'f',[0,,2],void 0])
assertEquals(9, holey.length);  // hole in embedded array is ignored
for (var i = 0; i < holey.length; i++) {
  if (i == 2 || i == 5) {
    assertFalse(i in holey);
  } else {
    assertTrue(i in holey);
  }
}

// Polluted prototype from prior tests.
delete Array.prototype[123];

// Check that concat reads getters in the correct order.
var arr1 = [,2];
var arr2 = [1,3];
var r1 = [].concat(arr1, arr2);  // [,2,1,3]
assertEquals([,2,1,3], r1);

// Make first array change length of second array.
Object.defineProperty(arr1, 0, {get: function() {
      arr2.push("X");
      return undefined;
    }, configurable: true})
var r2 = [].concat(arr1, arr2);  // [undefined,2,1,3,"X"]
assertEquals([undefined,2,1,3,"X"], r2);

// Make first array change length of second array massively.
arr2.length = 2;
Object.defineProperty(arr1, 0, {get: function() {
      arr2[500000] = "X";
      return undefined;
    }, configurable: true})
var r3 = [].concat(arr1, arr2);  // [undefined,2,1,3,"X"]
var expected = [undefined,2,1,3];
expected[500000 + 2] = "X";

assertEquals(expected, r3);

var arr3 = [];
var trace = [];
var expectedTrace = []
function mkGetter(i) { return function() { trace.push(i); }; }
arr3.length = 10000;
for (var i = 0; i < 100; i++) {
  Object.defineProperty(arr3, i * i, {get: mkGetter(i)});
  expectedTrace[i] = i;
  expectedTrace[100 + i] = i;
}
var r4 = [0].concat(arr3, arr3);
assertEquals(1 + arr3.length * 2, r4.length);
assertEquals(expectedTrace, trace);
