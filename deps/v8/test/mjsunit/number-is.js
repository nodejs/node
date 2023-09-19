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

// Test number predicates that Harmony adds to the Number constructor:
// isFinite(), isNaN(), isInteger(), isSafeInteger().

assertTrue(Number.isFinite(0));
assertTrue(Number.isFinite(Number.MIN_VALUE));
assertTrue(Number.isFinite(Number.MAX_VALUE));
assertTrue(Number.isFinite(Number.MIN_SAFE_INTEGER));
assertTrue(Number.isFinite(Number.MIN_SAFE_INTEGER - 13));
assertTrue(Number.isFinite(Number.MAX_SAFE_INTEGER));
assertTrue(Number.isFinite(Number.MAX_SAFE_INTEGER + 23));
assertFalse(Number.isFinite(Number.NaN));
assertFalse(Number.isFinite(Number.POSITIVE_INFINITY));
assertFalse(Number.isFinite(Number.NEGATIVE_INFINITY));
assertFalse(Number.isFinite(new Number(0)));
assertFalse(Number.isFinite(1/0));
assertFalse(Number.isFinite(-1/0));
assertFalse(Number.isFinite({}));
assertFalse(Number.isFinite([]));
assertFalse(Number.isFinite("s"));
assertFalse(Number.isFinite(null));
assertFalse(Number.isFinite(undefined));

assertFalse(Number.isNaN(0));
assertFalse(Number.isNaN(Number.MIN_VALUE));
assertFalse(Number.isNaN(Number.MAX_VALUE));
assertFalse(Number.isNaN(Number.MIN_SAFE_INTEGER - 13));
assertFalse(Number.isNaN(Number.MAX_SAFE_INTEGER + 23));
assertTrue(Number.isNaN(Number.NaN));
assertFalse(Number.isNaN(Number.POSITIVE_INFINITY));
assertFalse(Number.isNaN(Number.NEGATIVE_INFINITY));
assertFalse(Number.isNaN(Number.EPSILON));
assertFalse(Number.isNaN(new Number(0)));
assertFalse(Number.isNaN(1/0));
assertFalse(Number.isNaN(-1/0));
assertFalse(Number.isNaN({}));
assertFalse(Number.isNaN([]));
assertFalse(Number.isNaN("s"));
assertFalse(Number.isNaN(null));
assertFalse(Number.isNaN(undefined));

assertFalse(Number.isInteger({}));
assertFalse(Number.isInteger([]));
assertFalse(Number.isInteger("s"));
assertFalse(Number.isInteger(null));
assertFalse(Number.isInteger(undefined));
assertFalse(Number.isInteger(new Number(2)));
assertTrue(Number.isInteger(0));
assertFalse(Number.isInteger(Number.MIN_VALUE));
assertTrue(Number.isInteger(Number.MAX_VALUE));
assertTrue(Number.isInteger(Number.MIN_SAFE_INTEGER));
assertTrue(Number.isInteger(Number.MIN_SAFE_INTEGER - 13));
assertTrue(Number.isInteger(Number.MAX_SAFE_INTEGER));
assertTrue(Number.isInteger(Number.MAX_SAFE_INTEGER + 23));
assertFalse(Number.isInteger(Number.NaN));
assertFalse(Number.isInteger(Number.POSITIVE_INFINITY));
assertFalse(Number.isInteger(Number.NEGATIVE_INFINITY));
assertFalse(Number.isInteger(1/0));
assertFalse(Number.isInteger(-1/0));
assertFalse(Number.isInteger(Number.EPSILON));

assertFalse(Number.isSafeInteger({}));
assertFalse(Number.isSafeInteger([]));
assertFalse(Number.isSafeInteger("s"));
assertFalse(Number.isSafeInteger(null));
assertFalse(Number.isSafeInteger(undefined));
assertFalse(Number.isSafeInteger(new Number(2)));
assertTrue(Number.isSafeInteger(0));
assertTrue(Number.isSafeInteger(Number.MIN_SAFE_INTEGER));
assertFalse(Number.isSafeInteger(Number.MIN_SAFE_INTEGER - 13));
assertTrue(Number.isSafeInteger(Number.MIN_SAFE_INTEGER + 13));
assertTrue(Number.isSafeInteger(Number.MAX_SAFE_INTEGER));
assertFalse(Number.isSafeInteger(Number.MAX_SAFE_INTEGER + 23));
assertTrue(Number.isSafeInteger(Number.MAX_SAFE_INTEGER - 23));
assertFalse(Number.isSafeInteger(Number.MIN_VALUE));
assertFalse(Number.isSafeInteger(Number.MAX_VALUE));
assertFalse(Number.isSafeInteger(Number.NaN));
assertFalse(Number.isSafeInteger(Number.POSITIVE_INFINITY));
assertFalse(Number.isSafeInteger(Number.NEGATIVE_INFINITY));
assertFalse(Number.isSafeInteger(1/0));
assertFalse(Number.isSafeInteger(-1/0));
assertFalse(Number.isSafeInteger(Number.EPSILON));

var near_upper = Math.pow(2, 52);
assertTrue(Number.isSafeInteger(near_upper));
assertFalse(Number.isSafeInteger(2 * near_upper));
assertTrue(Number.isSafeInteger(2 * near_upper - 1));
assertTrue(Number.isSafeInteger(2 * near_upper - 2));
assertFalse(Number.isSafeInteger(2 * near_upper + 1));
assertFalse(Number.isSafeInteger(2 * near_upper + 2));
assertFalse(Number.isSafeInteger(2 * near_upper + 7));

var near_lower = -near_upper;
assertTrue(Number.isSafeInteger(near_lower));
assertFalse(Number.isSafeInteger(2 * near_lower));
assertTrue(Number.isSafeInteger(2 * near_lower + 1));
assertTrue(Number.isSafeInteger(2 * near_lower + 2));
assertFalse(Number.isSafeInteger(2 * near_lower - 1));
assertFalse(Number.isSafeInteger(2 * near_lower - 2));
assertFalse(Number.isSafeInteger(2 * near_lower - 7));
