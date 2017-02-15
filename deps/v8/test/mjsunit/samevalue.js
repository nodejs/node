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


// Flags: --expose-natives-as natives --allow-natives-syntax
// Test the SameValue and SameValueZero internal methods.

var obj1 = {x: 10, y: 11, z: "test"};
var obj2 = {x: 10, y: 11, z: "test"};

var sameValue = Object.is;
var sameValueZero = function(x, y) { return %SameValueZero(x, y); }

// Calls SameValue and SameValueZero and checks that their results match.
function sameValueBoth(a, b) {
  var result = sameValue(a, b);
  assertTrue(result === sameValueZero(a, b));
  return result;
}

// Calls SameValue and SameValueZero and checks that their results don't match.
function sameValueZeroOnly(a, b) {
  var result = sameValueZero(a, b);
  assertTrue(result && !sameValue(a, b));
  return result;
}

assertTrue(sameValueBoth(0, 0));
assertTrue(sameValueBoth(+0, +0));
assertTrue(sameValueBoth(-0, -0));
assertTrue(sameValueBoth(1, 1));
assertTrue(sameValueBoth(2, 2));
assertTrue(sameValueBoth(-1, -1));
assertTrue(sameValueBoth(0.5, 0.5));
assertTrue(sameValueBoth(true, true));
assertTrue(sameValueBoth(false, false));
assertTrue(sameValueBoth(NaN, NaN));
assertTrue(sameValueBoth(null, null));
assertTrue(sameValueBoth("foo", "foo"));
assertTrue(sameValueBoth(obj1, obj1));
// Undefined values.
assertTrue(sameValueBoth());
assertTrue(sameValueBoth(undefined, undefined));

assertFalse(sameValueBoth(0,1));
assertFalse(sameValueBoth("foo", "bar"));
assertFalse(sameValueBoth(obj1, obj2));
assertFalse(sameValueBoth(true, false));

assertFalse(sameValueBoth(obj1, true));
assertFalse(sameValueBoth(obj1, "foo"));
assertFalse(sameValueBoth(obj1, 1));
assertFalse(sameValueBoth(obj1, undefined));
assertFalse(sameValueBoth(obj1, NaN));

assertFalse(sameValueBoth(undefined, true));
assertFalse(sameValueBoth(undefined, "foo"));
assertFalse(sameValueBoth(undefined, 1));
assertFalse(sameValueBoth(undefined, obj1));
assertFalse(sameValueBoth(undefined, NaN));

assertFalse(sameValueBoth(NaN, true));
assertFalse(sameValueBoth(NaN, "foo"));
assertFalse(sameValueBoth(NaN, 1));
assertFalse(sameValueBoth(NaN, obj1));
assertFalse(sameValueBoth(NaN, undefined));

assertFalse(sameValueBoth("foo", true));
assertFalse(sameValueBoth("foo", 1));
assertFalse(sameValueBoth("foo", obj1));
assertFalse(sameValueBoth("foo", undefined));
assertFalse(sameValueBoth("foo", NaN));

assertFalse(sameValueBoth(true, 1));
assertFalse(sameValueBoth(true, obj1));
assertFalse(sameValueBoth(true, undefined));
assertFalse(sameValueBoth(true, NaN));
assertFalse(sameValueBoth(true, "foo"));

assertFalse(sameValueBoth(1, true));
assertFalse(sameValueBoth(1, obj1));
assertFalse(sameValueBoth(1, undefined));
assertFalse(sameValueBoth(1, NaN));
assertFalse(sameValueBoth(1, "foo"));

// Special string cases.
assertFalse(sameValueBoth("1", 1));
assertFalse(sameValueBoth("true", true));
assertFalse(sameValueBoth("false", false));
assertFalse(sameValueBoth("undefined", undefined));
assertFalse(sameValueBoth("NaN", NaN));

// SameValue considers -0 and +0 to be different; SameValueZero considers
// -0 and +0 to be the same.
assertTrue(sameValueZeroOnly(+0, -0));
assertTrue(sameValueZeroOnly(-0, +0));
