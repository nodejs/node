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

// Digit ranges are obeyed.

// Invalid ranges
assertThrows('Intl.NumberFormat(undefined, {minimumIntegerDigits: 0})');
assertThrows('Intl.NumberFormat(undefined, {minimumIntegerDigits: 22})');
assertThrows('Intl.NumberFormat(undefined, {minimumIntegerDigits: null})');
assertThrows('Intl.NumberFormat(undefined, {minimumIntegerDigits: Infinity})');
assertThrows('Intl.NumberFormat(undefined, {minimumIntegerDigits: -Infinity})');
assertThrows('Intl.NumberFormat(undefined, {minimumIntegerDigits: x})');

assertThrows('Intl.NumberFormat(undefined, {minimumFractionDigits: -1})');
assertThrows('Intl.NumberFormat(undefined, {maximumFractionDigits: 101})');

assertThrows('Intl.NumberFormat(undefined, {minimumSignificantDigits: 0})');
assertThrows('Intl.NumberFormat(undefined, {minimumSignificantDigits: 22})');
assertThrows('Intl.NumberFormat(undefined, {maximumSignificantDigits: 0})');
assertThrows('Intl.NumberFormat(undefined, {maximumSignificantDigits: 22})');
assertThrows('Intl.NumberFormat(undefined, ' +
    '{minimumSignificantDigits: 5, maximumSignificantDigits: 2})');

// Valid ranges
assertDoesNotThrow('Intl.NumberFormat(undefined, {minimumIntegerDigits: 1})');
assertDoesNotThrow('Intl.NumberFormat(undefined, {minimumIntegerDigits: 21})');

assertDoesNotThrow('Intl.NumberFormat(undefined, {minimumFractionDigits: 0})');
assertDoesNotThrow('Intl.NumberFormat(undefined, {minimumFractionDigits: 100})');

assertDoesNotThrow('Intl.NumberFormat(undefined, ' +
    '{minimumSignificantDigits: 1})');
assertDoesNotThrow('Intl.NumberFormat(undefined, ' +
    '{maximumSignificantDigits: 21})');
