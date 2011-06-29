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
 * This test uses assert{True,False}(... == ...) instead of
 * assertEquals(..., ...) to not rely on the details of the
 * implementation of assertEquals.
 */

function testEqual(a, b) {
  assertTrue(a == b);
  assertTrue(b == a);
  assertFalse(a != b);
  assertFalse(b != a);
}

function testNotEqual(a, b) {
  assertFalse(a == b);
  assertFalse(b == a);
  assertTrue(a != b);
  assertTrue(b != a);
}

// Object where ToPrimitive returns value.
function Wrapper(value) {
  this.value = value;
  this.valueOf = function () { return this.value; };
}

// Object where ToPrimitive returns value by failover to toString when
// valueOf isn't a function.
function Wrapper2(value) {
  this.value = value;
  this.valueOf = null;
  this.toString = function () { return this.value; };
}


// Compare values of same type.

// Numbers are equal if same, unless NaN, which isn't equal to anything, and
// +/-0 being equal.

testNotEqual(NaN, NaN);
testNotEqual(NaN, 0);
testNotEqual(NaN, Infinity);

testEqual(Number.MAX_VALUE, Number.MAX_VALUE);
testEqual(Number.MIN_VALUE, Number.MIN_VALUE);
testEqual(Infinity, Infinity);
testEqual(-Infinity, -Infinity);

testEqual(0, 0);
testEqual(0, -0);
testEqual(-0, -0);

testNotEqual(0.9, 1);
testNotEqual(0.999999, 1);
testNotEqual(0.9999999999, 1);
testNotEqual(0.9999999999999, 1);

// Strings are equal if containing the same code points.

testEqual('hello', 'hello');
testEqual('hello', 'hel' + 'lo');
testEqual('', '');
testEqual('\u0020\x20', '  ');  // Escapes are not part of the value.

// Booleans are equal if they are the same.

testEqual(true, true);
testEqual(false, false);
testNotEqual(true, false);

// Null and undefined are equal to themselves.

testEqual(null, null);
testEqual(undefined, undefined);

// Objects are equal if they are the same object only.

testEqual(Math, Math);
testEqual(Object.prototype, Object.prototype);


(function () {
  var x = new Wrapper(null);
  var y = x, z = x;
   testEqual(y, x);
})();

(function () {
  var x = new Boolean(true);
  var y = x, z = x;
   testEqual(y, x);
})();

(function () {
  var x = new Boolean(false);
  var y = x, z = x;
   testEqual(y, x);
})();

// Test comparing values of different types.

// Null and undefined are equal to each-other, and to nothing else.
testEqual(null, undefined);
testEqual(undefined, null);

testNotEqual(null, new Wrapper(null));
testNotEqual(null, 0);
testNotEqual(null, false);
testNotEqual(null, "");
testNotEqual(null, new Object());
testNotEqual(undefined, new Wrapper(undefined));
testNotEqual(undefined, 0);
testNotEqual(undefined, false);
testNotEqual(undefined, "");
testNotEqual(undefined, new Object());

// Numbers compared to Strings will convert the string to a number using
// the internal ToNumber conversion.

testEqual(1, '1');
testEqual(255, '0xff');
testEqual(0, '\r');  // ToNumber ignores tailing and trailing whitespace.
testEqual(1e19, '1e19');
testEqual(Infinity, "Infinity");

// Booleans compared to anything else will be converted to numbers.
testEqual(false, 0);
testEqual(true, 1);
testEqual(false, "0");  // String also converted to number.
testEqual(true, "1");

// Objects compared to Number or String (or Boolean, since that's converted
// to Number too) is converted to primitive using ToPrimitive with NO HINT.
// Having no hint means Date gets a string hint, and everything else gets
// a number hint.

testEqual(new Boolean(true), true);
testEqual(new Boolean(true), 1);  // First to primtive boolean, then to number.
testEqual(new Boolean(false), false);
testEqual(new Boolean(false), 0);

testEqual(new Wrapper(true), true);
testEqual(new Wrapper(true), 1);
testEqual(new Wrapper(false), false);
testEqual(new Wrapper(false), 0);

testEqual(new Wrapper2(true), true);
testEqual(new Wrapper2(true), 1);
testEqual(new Wrapper2(false), false);
testEqual(new Wrapper2(false), 0);

testEqual(new Number(1), true);
testEqual(new Number(1), 1);
testEqual(new Number(0), false);
testEqual(new Number(0), 0);

// Date objects convert to string, not number (and the string does not
// convert to the number).
testEqual(new Date(42), String(new Date(42)));
testNotEqual(new Date(42), Number(new Date(42)));
var dnow = new Date();
testEqual(dnow, dnow);
testEqual(dnow, String(dnow));
testNotEqual(dnow, Number(dnow));

// Doesn't just call toString, but uses ToPrimitive which tries toString first
// and valueOf second.
dnow.toString = null;
testEqual(dnow, Number(dnow));
dnow.valueOf = function () { return "42"; };
testEqual(dnow, 42);
dnow.toString = function () { return "1"; };
testEqual(dnow, true);


// Objects compared to other objects, or to null and undefined, are not
// converted to primitive.
testNotEqual(new Wrapper(null), new Wrapper(null));
testNotEqual(new Boolean(true), new Boolean(true));
testNotEqual(new Boolean(false), new Boolean(false));
testNotEqual(new String("a"), new String("a"));
testNotEqual(new Number(42), new Number(42));
testNotEqual(new Date(42), new Date(42));
testNotEqual(new Array(42), new Array(42));
testNotEqual(new Object(), new Object());

// Object that can't be converted to primitive.
var badObject = {
  valueOf: null,
  toString: function() {
    return this;  // Not primitive.
  }
};

testEqual(badObject, badObject);
testNotEqual(badObject, {});
testNotEqual(badObject, null);
testNotEqual(badObject, undefined);
// Forcing conversion will throw.
function testBadConversion(value) {
  assertThrows(function() { return badObject == value; });
  assertThrows(function() { return badObject != value; });
  assertThrows(function() { return value == badObject; });
  assertThrows(function() { return value != badObject; });
}
testBadConversion(0);
testBadConversion("string");
testBadConversion(true);
