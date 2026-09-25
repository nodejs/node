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
 * @fileoverview Assert we match ES3 and Safari.
 */

assertEquals(0, Array.prototype.length, "Array.prototype.length");
assertEquals(1, Array.length, "Array.length");
assertEquals(1, Array.prototype.concat.length, "Array.prototype.concat.length");
assertEquals(1, Array.prototype.join.length, "Array.prototype.join.length");
assertEquals(1, Array.prototype.push.length, "Array.prototype.push.length");
assertEquals(1, Array.prototype.unshift.length, "Array.prototype.unshift.length");
assertEquals(1, Boolean.length, "Boolean.length");
assertEquals(1, Error.length, "Error.length");
assertEquals(1, EvalError.length, "EvalError.length");
assertEquals(1, Function.length, "Function.length");
assertEquals(1, Function.prototype.call.length, "Function.prototype.call.length");
assertEquals(1, Number.length, "Number.length");
assertEquals(1, Number.prototype.toExponential.length, "Number.prototype.toExponential.length");
assertEquals(1, Number.prototype.toFixed.length, "Number.prototype.toFixed.length");
assertEquals(1, Number.prototype.toPrecision.length, "Number.prototype.toPrecision.length");
assertEquals(1, Object.length, "Object.length");
assertEquals(1, RangeError.length, "RangeError.length");
assertEquals(1, ReferenceError.length, "ReferenceError.length");
assertEquals(1, String.fromCharCode.length, "String.fromCharCode.length");
assertEquals(1, String.length, "String.length");
assertEquals(1, String.prototype.concat.length, "String.prototype.concat.length");
assertEquals(1, String.prototype.indexOf.length, "String.prototype.indexOf.length");
assertEquals(1, String.prototype.lastIndexOf.length, "String.prototype.lastIndexOf.length");
assertEquals(1, SyntaxError.length, "SyntaxError.length");
assertEquals(1, TypeError.length, "TypeError.length");
assertEquals(2, Array.prototype.slice.length, "Array.prototype.slice.length");
assertEquals(2, Array.prototype.splice.length, "Array.prototype.splice.length");
assertEquals(2, Date.prototype.setMonth.length, "Date.prototype.setMonth.length");
assertEquals(2, Date.prototype.setSeconds.length, "Date.prototype.setSeconds.length");
assertEquals(2, Date.prototype.setUTCMonth.length, "Date.prototype.setUTCMonth.length");
assertEquals(2, Date.prototype.setUTCSeconds.length, "Date.prototype.setUTCSeconds.length");
assertEquals(2, Function.prototype.apply.length, "Function.prototype.apply.length");
assertEquals(2, Math.max.length, "Math.max.length");
assertEquals(2, Math.min.length, "Math.min.length");
assertEquals(2, RegExp.length, "RegExp.length");
assertEquals(2, String.prototype.slice.length, "String.prototype.slice.length");
assertEquals(2, String.prototype.split.length, "String.prototype.split.length");
assertEquals(2, String.prototype.substr.length, "String.prototype.substr.length");
assertEquals(2, String.prototype.substring.length, "String.prototype.substring.length");
assertEquals(3, Date.prototype.setFullYear.length, "Date.prototype.setFullYear.length");
assertEquals(3, Date.prototype.setMinutes.length, "Date.prototype.setMinutes.length");
assertEquals(3, Date.prototype.setUTCFullYear.length, "Date.prototype.setUTCFullYear.length");
assertEquals(3, Date.prototype.setUTCMinutes.length, "Date.prototype.setUTCMinutes.length");
assertEquals(4, Date.prototype.setHours.length, "Date.prototype.setHours.length");
assertEquals(4, Date.prototype.setUTCHours.length, "Date.prototype.setUTCHours.length");
assertEquals(7, Date.UTC.length, "Date.UTC.length");
assertEquals(7, Date.length, "Date.length");
