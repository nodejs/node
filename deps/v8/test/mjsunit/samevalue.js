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


// Flags: --expose-natives-as natives
// Test the SameValue internal method.

var obj1 = {x: 10, y: 11, z: "test"};
var obj2 = {x: 10, y: 11, z: "test"};

var sameValue = natives.$sameValue;

assertTrue(sameValue(0, 0));
assertTrue(sameValue(+0, +0));
assertTrue(sameValue(-0, -0));
assertTrue(sameValue(1, 1));
assertTrue(sameValue(2, 2));
assertTrue(sameValue(-1, -1));
assertTrue(sameValue(0.5, 0.5));
assertTrue(sameValue(true, true));
assertTrue(sameValue(false, false));
assertTrue(sameValue(NaN, NaN));
assertTrue(sameValue(null, null));
assertTrue(sameValue("foo", "foo"));
assertTrue(sameValue(obj1, obj1));
// Undefined values.
assertTrue(sameValue());
assertTrue(sameValue(undefined, undefined));

assertFalse(sameValue(0,1));
assertFalse(sameValue("foo", "bar"));
assertFalse(sameValue(obj1, obj2));
assertFalse(sameValue(true, false));

assertFalse(sameValue(obj1, true));
assertFalse(sameValue(obj1, "foo"));
assertFalse(sameValue(obj1, 1));
assertFalse(sameValue(obj1, undefined));
assertFalse(sameValue(obj1, NaN));

assertFalse(sameValue(undefined, true));
assertFalse(sameValue(undefined, "foo"));
assertFalse(sameValue(undefined, 1));
assertFalse(sameValue(undefined, obj1));
assertFalse(sameValue(undefined, NaN));

assertFalse(sameValue(NaN, true));
assertFalse(sameValue(NaN, "foo"));
assertFalse(sameValue(NaN, 1));
assertFalse(sameValue(NaN, obj1));
assertFalse(sameValue(NaN, undefined));

assertFalse(sameValue("foo", true));
assertFalse(sameValue("foo", 1));
assertFalse(sameValue("foo", obj1));
assertFalse(sameValue("foo", undefined));
assertFalse(sameValue("foo", NaN));

assertFalse(sameValue(true, 1));
assertFalse(sameValue(true, obj1));
assertFalse(sameValue(true, undefined));
assertFalse(sameValue(true, NaN));
assertFalse(sameValue(true, "foo"));

assertFalse(sameValue(1, true));
assertFalse(sameValue(1, obj1));
assertFalse(sameValue(1, undefined));
assertFalse(sameValue(1, NaN));
assertFalse(sameValue(1, "foo"));

// Special string cases.
assertFalse(sameValue("1", 1));
assertFalse(sameValue("true", true));
assertFalse(sameValue("false", false));
assertFalse(sameValue("undefined", undefined));
assertFalse(sameValue("NaN", NaN));

// -0 and +0 are should be different
assertFalse(sameValue(+0, -0));
assertFalse(sameValue(-0, +0));
