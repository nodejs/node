// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --harmony-maths

[Math.sinh, Math.cosh, Math.tanh, Math.asinh, Math.acosh, Math.atanh].
    forEach(function(fun) {
  assertTrue(isNaN(fun(NaN)));
  assertTrue(isNaN(fun("abc")));
  assertTrue(isNaN(fun({})));
  assertEquals(fun(0), fun([]));
  assertTrue(isNaN(fun([1, 1])));
  assertEquals(fun(1.11), fun({ toString: function() { return "1.11"; } }));
  assertEquals(fun(-3.1), fun({ toString: function() { return -3.1; } }));
  assertEquals(fun(-1.1), fun({ valueOf: function() { return "-1.1"; } }));
  assertEquals(fun(3.11), fun({ valueOf: function() { return 3.11; } }));
});


function test_id(fun, rev, value) {
  assertEqualsDelta(1, rev(fun(value))/value, 1E-7);
}

[Math.PI, 2, 5, 1E-5, 0.3].forEach(function(x) {
  test_id(Math.sinh, Math.asinh, x);
  test_id(Math.sinh, Math.asinh, -x);
  test_id(Math.cosh, Math.acosh, x);
  test_id(Math.tanh, Math.atanh, x);
  test_id(Math.tanh, Math.atanh, -x);
});


[Math.sinh, Math.asinh, Math.tanh, Math.atanh].forEach(function(fun) {
  assertEquals("-Infinity", String(1/fun(-0)));
  assertEquals("Infinity", String(1/fun(0)));
});


[Math.sinh, Math.asinh].forEach(function(fun) {
  assertEquals("-Infinity", String(fun(-Infinity)));
  assertEquals("Infinity", String(fun(Infinity)));
  assertEquals("-Infinity", String(fun("-Infinity")));
  assertEquals("Infinity", String(fun("Infinity")));
});


assertEquals("Infinity", String(Math.cosh(-Infinity)));
assertEquals("Infinity", String(Math.cosh(Infinity)));
assertEquals("Infinity", String(Math.cosh("-Infinity")));
assertEquals("Infinity", String(Math.cosh("Infinity")));


assertEquals("-Infinity", String(Math.atanh(-1)));
assertEquals("Infinity", String(Math.atanh(1)));

// Math.atanh(x) is NaN for |x| > 1 and NaN
[1.000000000001, Math.PI, 10000000, 2, Infinity, NaN].forEach(function(x) {
  assertTrue(isNaN(Math.atanh(-x)));
  assertTrue(isNaN(Math.atanh(x)));
});


assertEquals(1, Math.tanh(Infinity));
assertEquals(-1, Math.tanh(-Infinity));
assertEquals(1, Math.cosh(0));
assertEquals(1, Math.cosh(-0));

assertEquals(0, Math.acosh(1));
assertEquals("Infinity", String(Math.acosh(Infinity)));

// Math.acosh(x) is NaN for x < 1
[0.99999999999, 0.2, -1000, 0, -0].forEach(function(x) {
  assertTrue(isNaN(Math.acosh(x)));
});


// Some random samples.
assertEqualsDelta(0.5210953054937, Math.sinh(0.5), 1E-12);
assertEqualsDelta(74.203210577788, Math.sinh(5), 1E-12);
assertEqualsDelta(-0.5210953054937, Math.sinh(-0.5), 1E-12);
assertEqualsDelta(-74.203210577788, Math.sinh(-5), 1E-12);

assertEqualsDelta(1.1276259652063, Math.cosh(0.5), 1E-12);
assertEqualsDelta(74.209948524787, Math.cosh(5), 1E-12);
assertEqualsDelta(1.1276259652063, Math.cosh(-0.5), 1E-12);
assertEqualsDelta(74.209948524787, Math.cosh(-5), 1E-12);

assertEqualsDelta(0.4621171572600, Math.tanh(0.5), 1E-12);
assertEqualsDelta(0.9999092042625, Math.tanh(5), 1E-12);
assertEqualsDelta(-0.4621171572600, Math.tanh(-0.5), 1E-12);
assertEqualsDelta(-0.9999092042625, Math.tanh(-5), 1E-12);

assertEqualsDelta(0.4812118250596, Math.asinh(0.5), 1E-12);
assertEqualsDelta(2.3124383412727, Math.asinh(5), 1E-12);
assertEqualsDelta(-0.4812118250596, Math.asinh(-0.5), 1E-12);
assertEqualsDelta(-2.3124383412727, Math.asinh(-5), 1E-12);

assertEqualsDelta(0.9624236501192, Math.acosh(1.5), 1E-12);
assertEqualsDelta(2.2924316695612, Math.acosh(5), 1E-12);
assertEqualsDelta(0.4435682543851, Math.acosh(1.1), 1E-12);
assertEqualsDelta(1.3169578969248, Math.acosh(2), 1E-12);

assertEqualsDelta(0.5493061443341, Math.atanh(0.5), 1E-12);
assertEqualsDelta(0.1003353477311, Math.atanh(0.1), 1E-12);
assertEqualsDelta(-0.5493061443341, Math.atanh(-0.5), 1E-12);
assertEqualsDelta(-0.1003353477311, Math.atanh(-0.1), 1E-12);

[0, 1E-50, 1E-10, 1E10, 1E50, 1E100, 1E150].forEach(function(x) {
  assertEqualsDelta(Math.asinh(x), -Math.asinh(-x), 1E-12);
});

[1-(1E-16), 0, 1E-10, 1E-50].forEach(function(x) {
  assertEqualsDelta(Math.atanh(x), -Math.atanh(-x), 1E-12);
});
