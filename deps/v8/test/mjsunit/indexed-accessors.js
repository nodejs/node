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

// Test that getters can be defined and called with an index as a parameter.

var o = {};
o.x = 42;
o.__defineGetter__('0', function() { return o.x; });
assertEquals(o.x, o[0]);
assertEquals(o.x, o.__lookupGetter__('0')());

o.__defineSetter__('0', function(y) { o.x = y; });
assertEquals(o.x, o[0]);
assertEquals(o.x, o.__lookupGetter__('0')());
o[0] = 21;
assertEquals(21, o.x);
o.__lookupSetter__(0)(7);
assertEquals(7, o.x);

function Pair(x, y) {
  this.x = x;
  this.y = y;
};
Pair.prototype.__defineGetter__('0', function() { return this.x; });
Pair.prototype.__defineGetter__('1', function() { return this.y; });
Pair.prototype.__defineSetter__('0', function(x) { this.x = x; });
Pair.prototype.__defineSetter__('1', function(y) { this.y = y; });

var p = new Pair(2, 3);
assertEquals(2, p[0]);
assertEquals(3, p[1]);
p.x = 7;
p[1] = 8;
assertEquals(7, p[0]);
assertEquals(7, p.x);
assertEquals(8, p[1]);
assertEquals(8, p.y);


// Testing that a defined getter doesn't get lost due to inline caching.
var expected = {};
var actual = {};
for (var i = 0; i < 10; i++) {
  expected[i] = actual[i] = i;
}
function testArray() {
  for (var i = 0; i < 10; i++) {
    assertEquals(expected[i], actual[i]);
  }
}
actual[1000000] = -1;
testArray();
testArray();
actual.__defineGetter__('0', function() { return expected[0]; });
expected[0] = 42;
testArray();
expected[0] = 111;
testArray();

// Using a setter where only a getter is defined throws an exception.
var q = {};
q.__defineGetter__('0', function() { return 42; });
assertThrows('q[0] = 7');

// Using a getter where only a setter is defined returns undefined.
var q1 = {};
q1.__defineSetter__('0', function() {q1.b = 17;});
assertEquals(q1[0], undefined);
// Setter works
q1[0] = 3;
assertEquals(q1[0], undefined);
assertEquals(q1.b, 17);

// Complex case of using an undefined getter.
// From http://code.google.com/p/v8/issues/detail?id=298
// Reported by nth10sd.

a = function() {};
__defineSetter__("0", function() {});
if (a |= '') {};
assertThrows('this[a].__parent__');
assertEquals(a, 0);
assertEquals(this[a], undefined);
