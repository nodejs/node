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

// On MacOS X 10.7.5, this test needs a stack size of at least 788 kBytes.
// Flags: --stack-size=800

// Test that we can make large object literals that work.
// Also test that we can attempt to make even larger object literals without
// crashing.
function testLiteral(size, array_in_middle) {
  print(size);

  var f;

  // Build object-literal string.
  var literal = "function f() { return ";

  for (var i = 0; i < size; i++) {
    literal += "[";
  }

  literal += array_in_middle ? " [42.2]" : "{a:42.2}";

  for (var i = 0; i < size; i++) {
    literal += "]";
  }

  literal += "; }";

  // Create the object literal.
  eval(literal);

  var x = f();

  // Check that the properties have the expected values.
  for (var i = 0; i < size; i++) {
    x = x[0];
  }

  if (array_in_middle) {
    assertEquals(42.2, x[0]), "x array in middle";
    x[0] = 41.2;
  } else {
    assertEquals(42.2, x.a, "x object in middle");
    x.a = 41.2;
  }

  var y = f();
  for (var i = 0; i < size; i++) {
    y = y[0];
  }

  if (array_in_middle) {
    assertEquals(42.2, y[0], "y array in middle");
    y[0] = 41.2;
  } else {
    assertEquals(42.2, y.a, "y object in middle");
    y.a = 41.2;
  }
}

// The sizes to test.
var sizes = [1, 2, 100, 200, 300];

// Run the test.
for (var i = 0; i < sizes.length; i++) {
  testLiteral(sizes[i], false);
  testLiteral(sizes[i], true);
}

function testLiteralAndCatch(size) {
  var big_enough = false;
  try {
    testLiteral(size, false);
  } catch (e) {
    big_enough = true;
  }
  try {
    testLiteral(size, true);
  } catch (e) {
    big_enough = true;
  }
  return big_enough;
}

// Catch stack overflows.

testLiteralAndCatch(1000) ||
testLiteralAndCatch(20000) ||
testLiteralAndCatch(200000);
