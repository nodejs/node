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


// Flags: --expose-natives_as natives
// Test the SameValue internal method.

var obj1 = {x: 10, y: 11, z: "test"};
var obj2 = {x: 10, y: 11, z: "test"};

assertTrue(natives.SameValue(0, 0));
assertTrue(natives.SameValue(+0, +0));
assertTrue(natives.SameValue(-0, -0));
assertTrue(natives.SameValue(1, 1));
assertTrue(natives.SameValue(2, 2));
assertTrue(natives.SameValue(-1, -1));
assertTrue(natives.SameValue(0.5, 0.5));
assertTrue(natives.SameValue(true, true));
assertTrue(natives.SameValue(false, false));
assertTrue(natives.SameValue(NaN, NaN));
assertTrue(natives.SameValue(null, null));
assertTrue(natives.SameValue("foo", "foo"));
assertTrue(natives.SameValue(obj1, obj1));
// Undefined values.
assertTrue(natives.SameValue());
assertTrue(natives.SameValue(undefined, undefined));

assertFalse(natives.SameValue(0,1));
assertFalse(natives.SameValue("foo", "bar"));
assertFalse(natives.SameValue(obj1, obj2));
assertFalse(natives.SameValue(true, false));

assertFalse(natives.SameValue(obj1, true));
assertFalse(natives.SameValue(obj1, "foo"));
assertFalse(natives.SameValue(obj1, 1));
assertFalse(natives.SameValue(obj1, undefined));
assertFalse(natives.SameValue(obj1, NaN));

assertFalse(natives.SameValue(undefined, true));
assertFalse(natives.SameValue(undefined, "foo"));
assertFalse(natives.SameValue(undefined, 1));
assertFalse(natives.SameValue(undefined, obj1));
assertFalse(natives.SameValue(undefined, NaN));

assertFalse(natives.SameValue(NaN, true));
assertFalse(natives.SameValue(NaN, "foo"));
assertFalse(natives.SameValue(NaN, 1));
assertFalse(natives.SameValue(NaN, obj1));
assertFalse(natives.SameValue(NaN, undefined));

assertFalse(natives.SameValue("foo", true));
assertFalse(natives.SameValue("foo", 1));
assertFalse(natives.SameValue("foo", obj1));
assertFalse(natives.SameValue("foo", undefined));
assertFalse(natives.SameValue("foo", NaN));

assertFalse(natives.SameValue(true, 1));
assertFalse(natives.SameValue(true, obj1));
assertFalse(natives.SameValue(true, undefined));
assertFalse(natives.SameValue(true, NaN));
assertFalse(natives.SameValue(true, "foo"));

assertFalse(natives.SameValue(1, true));
assertFalse(natives.SameValue(1, obj1));
assertFalse(natives.SameValue(1, undefined));
assertFalse(natives.SameValue(1, NaN));
assertFalse(natives.SameValue(1, "foo"));

// Special string cases.
assertFalse(natives.SameValue("1", 1));
assertFalse(natives.SameValue("true", true));
assertFalse(natives.SameValue("false", false));
assertFalse(natives.SameValue("undefined", undefined));
assertFalse(natives.SameValue("NaN", NaN));

// -0 and +0 are should be different
assertFalse(natives.SameValue(+0, -0));
assertFalse(natives.SameValue(-0, +0));
