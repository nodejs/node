// Copyright 2012 the V8 project authors. All rights reserved.
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

// Number.prototype methods on non-Numbers.

assertThrows(function() { Number.prototype.toExponential.call({}) },
             TypeError);

assertThrows(function() { Number.prototype.toPrecision.call({}) },
             TypeError);

assertThrows(function() { Number.prototype.toFixed.call({}) },
             TypeError);

assertThrows(function() { Number.prototype.toString.call({}) },
             TypeError);

assertThrows(function() { Number.prototype.toLocaleString.call({}) },
             TypeError);

assertThrows(function() { Number.prototype.ValueOf.call({}) },
             TypeError);


// Call on Number objects with custom valueOf method.

var x_obj = new Number(1);
x_obj.valueOf = function() { assertUnreachable(); };

assertEquals("1.00e+0",
             Number.prototype.toExponential.call(x_obj, 2));

assertEquals("1.0",
             Number.prototype.toPrecision.call(x_obj, 2));

assertEquals("1.00",
             Number.prototype.toFixed.call(x_obj, 2));

// Call on primitive numbers.
assertEquals("1.00e+0",
             Number.prototype.toExponential.call(1, 2));

assertEquals("1.0",
             Number.prototype.toPrecision.call(1, 2));

assertEquals("1.00",
             Number.prototype.toFixed.call(1, 2));


// toExponential and toPrecision does following steps in order
// 1) convert the argument using ToInteger
// 2) check for non-finite receiver, on which it returns,
// 3) check argument range and throw exception if out of range.
// Note that the the last two steps are reversed for toFixed.
// Luckily, the receiver is expected to be a number or number
// wrapper, so that getting its value is not observable.

var f_flag = false;
var f_obj = { valueOf: function() { f_flag = true; return 1000; } };

assertEquals("NaN",
             Number.prototype.toExponential.call(NaN, f_obj));
assertTrue(f_flag);

f_flag = false;
assertEquals("Infinity",
             Number.prototype.toExponential.call(1/0, f_obj));
assertTrue(f_flag);

f_flag = false;
assertEquals("-Infinity",
             Number.prototype.toExponential.call(-1/0, f_obj));
assertTrue(f_flag);

f_flag = false;
assertEquals("NaN",
             Number.prototype.toPrecision.call(NaN, f_obj));
assertTrue(f_flag);

f_flag = false;
assertEquals("Infinity",
             Number.prototype.toPrecision.call(1/0, f_obj));
assertTrue(f_flag);

f_flag = false;
assertEquals("-Infinity",
             Number.prototype.toPrecision.call(-1/0, f_obj));
assertTrue(f_flag);

// The odd man out: toFixed.

f_flag = false;
assertThrows(function() { Number.prototype.toFixed.call(NaN, f_obj) },
             RangeError);
assertTrue(f_flag);

f_flag = false;
assertThrows(function() { Number.prototype.toFixed.call(1/0, f_obj) },
             RangeError);
assertTrue(f_flag);

f_flag = false;
assertThrows(function() { Number.prototype.toFixed.call(-1/0, f_obj) },
             RangeError);
assertTrue(f_flag);
