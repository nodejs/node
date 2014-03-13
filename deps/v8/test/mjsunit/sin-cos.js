// Copyright 2011 the V8 project authors. All rights reserved.
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

// Test Math.sin and Math.cos.

// Flags: --allow-natives-syntax

assertEquals("-Infinity", String(1/Math.sin(-0)));
assertEquals(1, Math.cos(-0));
assertEquals("-Infinity", String(1/Math.tan(-0)));

// Assert that minus zero does not cause deopt.
function no_deopt_on_minus_zero(x) {
  return Math.sin(x) + Math.cos(x) + Math.tan(x);
}

no_deopt_on_minus_zero(1);
no_deopt_on_minus_zero(1);
%OptimizeFunctionOnNextCall(no_deopt_on_minus_zero);
no_deopt_on_minus_zero(-0);
assertOptimized(no_deopt_on_minus_zero);


function sinTest() {
  assertEquals(0, Math.sin(0));
  assertEquals(1, Math.sin(Math.PI / 2));
}

function cosTest() {
  assertEquals(1, Math.cos(0));
  assertEquals(-1, Math.cos(Math.PI));
}

sinTest();
cosTest();

// By accident, the slow case for sine and cosine were both sine at
// some point.  This is a regression test for that issue.
var x = Math.pow(2, 30);
assertTrue(Math.sin(x) != Math.cos(x));

// Ensure that sine and log are not the same.
x = 0.5;
assertTrue(Math.sin(x) != Math.log(x));

// Test against approximation by series.
var factorial = [1];
var accuracy = 50;
for (var i = 1; i < accuracy; i++) {
  factorial[i] = factorial[i-1] * i;
}

// We sum up in the reverse order for higher precision, as we expect the terms
// to grow smaller for x reasonably close to 0.
function precision_sum(array) {
  var result = 0;
  while (array.length > 0) {
    result += array.pop();
  }
  return result;
}

function sin(x) {
  var sign = 1;
  var x2 = x*x;
  var terms = [];
  for (var i = 1; i < accuracy; i += 2) {
    terms.push(sign * x / factorial[i]);
    x *= x2;
    sign *= -1;
  }
  return precision_sum(terms);
}

function cos(x) {
  var sign = -1;
  var x2 = x*x;
  x = x2;
  var terms = [1];
  for (var i = 2; i < accuracy; i += 2) {
    terms.push(sign * x / factorial[i]);
    x *= x2;
    sign *= -1;
  }
  return precision_sum(terms);
}

function abs_error(fun, ref, x) {
  return Math.abs(ref(x) - fun(x));
}

var test_inputs = [];
for (var i = -10000; i < 10000; i += 177) test_inputs.push(i/1257);
var epsilon = 0.0000001;

test_inputs.push(0);
test_inputs.push(0 + epsilon);
test_inputs.push(0 - epsilon);
test_inputs.push(Math.PI/2);
test_inputs.push(Math.PI/2 + epsilon);
test_inputs.push(Math.PI/2 - epsilon);
test_inputs.push(Math.PI);
test_inputs.push(Math.PI + epsilon);
test_inputs.push(Math.PI - epsilon);
test_inputs.push(- 2*Math.PI);
test_inputs.push(- 2*Math.PI + epsilon);
test_inputs.push(- 2*Math.PI - epsilon);

var squares = [];
for (var i = 0; i < test_inputs.length; i++) {
  var x = test_inputs[i];
  var err_sin = abs_error(Math.sin, sin, x);
  var err_cos = abs_error(Math.cos, cos, x)
  assertEqualsDelta(0, err_sin, 1E-13);
  assertEqualsDelta(0, err_cos, 1E-13);
  squares.push(err_sin*err_sin + err_cos*err_cos);
}

// Sum squares up by adding them pairwise, to avoid losing precision.
while (squares.length > 1) {
  var reduced = [];
  if (squares.length % 2 == 1) reduced.push(squares.pop());
  // Remaining number of elements is even.
  while(squares.length > 1) reduced.push(squares.pop() + squares.pop());
  squares = reduced;
}

var err_rms = Math.sqrt(squares[0] / test_inputs.length / 2);
assertEqualsDelta(0, err_rms, 1E-14);

assertEquals(-1, Math.cos({ valueOf: function() { return Math.PI; } }));
assertEquals(0, Math.sin("0x00000"));
assertEquals(1, Math.cos("0x00000"));
assertTrue(isNaN(Math.sin(Infinity)));
assertTrue(isNaN(Math.cos("-Infinity")));
assertEquals("Infinity", String(Math.tan(Math.PI/2)));
assertEquals("-Infinity", String(Math.tan(-Math.PI/2)));
assertEquals("-Infinity", String(1/Math.sin("-0")));

// Assert that the remainder after division by pi is reasonably precise.
function assertError(expected, x, epsilon) {
  assertTrue(Math.abs(x - expected) < epsilon);
}

assertEqualsDelta(0.9367521275331447,  Math.cos(1e06),  1e-15);
assertEqualsDelta(0.8731196226768560,  Math.cos(1e10),  1e-08);
assertEqualsDelta(0.9367521275331447,  Math.cos(-1e06), 1e-15);
assertEqualsDelta(0.8731196226768560,  Math.cos(-1e10), 1e-08);
assertEqualsDelta(-0.3499935021712929, Math.sin(1e06),  1e-15);
assertEqualsDelta(-0.4875060250875106, Math.sin(1e10),  1e-08);
assertEqualsDelta(0.3499935021712929,  Math.sin(-1e06), 1e-15);
assertEqualsDelta(0.4875060250875106,  Math.sin(-1e10), 1e-08);
assertEqualsDelta(0.7796880066069787,  Math.sin(1e16),  1e-05);
assertEqualsDelta(-0.6261681981330861, Math.cos(1e16),  1e-05);

// Assert that remainder calculation terminates.
for (var i = -1024; i < 1024; i++) {
  assertFalse(isNaN(Math.sin(Math.pow(2, i))));
}

assertFalse(isNaN(Math.cos(1.57079632679489700)));
assertFalse(isNaN(Math.cos(-1e-100)));
assertFalse(isNaN(Math.cos(-1e-323)));
