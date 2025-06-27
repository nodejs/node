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

// Test that we can create object literals of various sizes.

// Flags: --allow-natives-syntax

function testLiteral(size) {

  // Build object-literal string.
  var literal = "var o = { ";

  for (var i = 0; i < size; i++) {
    if (i > 0) literal += ",";
    literal += 'a' + i + ':' + i;
  }
  literal += "}";

  // Create the object literal.
  eval(literal);

  // Check that the properties have the expected values.
  for (var i = 0; i < size; i++) {
    assertEquals(i, o['a' + i]);
  }
}
function testElementLiteral(size) {

  // Build object-literal string.
  var literal = "var o = { ";

  for (var i = 0; i < size; i++) {
    if (i > 0) literal += ",";
    literal += i + ':\'' + i + '\'';
  }
  literal += "}";

  // Create the object literal.
  eval(literal);

  // Check that the properties have the expected values.
  for (var i = 0; i < size; i++) {
    assertEquals(i + '', o[i]);
  }
}

// The sizes to test.
var sizes = [0, 1, 2, 100, 200, 400, 1000];

// Run the test.
for (var i = 0; i < sizes.length; i++) {
  testLiteral(sizes[i]);
  testElementLiteral(sizes[i]);
}


// Large Object dictionary mode literal.


function TestSlowLiteralOptimized() {
  function f() {
    return {__proto__: null, bar: 'barValue'};
  };
  %PrepareFunctionForOptimization(f);
  let obj = f();
  assertFalse(%HasFastProperties(obj));
  assertEquals(Object.getPrototypeOf(obj), null);
  assertEquals(["bar"], Object.keys(obj));
  assertEquals("barValue", obj.bar);
  obj.bar = "barValue2";
  assertEquals("barValue2", obj.bar);

  %OptimizeFunctionOnNextCall(f);
  obj = f();
  assertFalse(%HasFastProperties(obj));
  assertEquals(Object.getPrototypeOf(obj), null);
  assertEquals(["bar"], Object.keys(obj));
  assertEquals("barValue", obj.bar);
  obj.bar = "barValue2";
  assertEquals("barValue2", obj.bar);
};
TestSlowLiteralOptimized();
TestSlowLiteralOptimized();
