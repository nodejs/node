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

// Summary of the spec: lastIndex is reset to 0 if
// - a regexp fails to match, regardless of global or non-global.
// - a global regexp is used in a function that returns multiple results,
//   such as String.prototype.replace or String.prototype.match, since it
//   repeats the regexp until it fails to match.
// Otherwise lastIndex is only set when a global regexp matches, to the index
// after the match.

// Test Regexp.prototype.exec
r = /a/;
r.lastIndex = 1;
r.exec("zzzz");
assertEquals(0, r.lastIndex);

// Test Regexp.prototype.test
r = /a/;
r.lastIndex = 1;
r.test("zzzz");
assertEquals(0, r.lastIndex);

// Test String.prototype.match
r = /a/;
r.lastIndex = 1;
"zzzz".match(r);
assertEquals(0, r.lastIndex);

// Test String.prototype.replace with atomic regexp and empty string.
r = /a/;
r.lastIndex = 1;
"zzzz".replace(r, "");
assertEquals(0, r.lastIndex);

// Test String.prototype.replace with non-atomic regexp and empty string.
r = /\d/;
r.lastIndex = 1;
"zzzz".replace(r, "");
assertEquals(0, r.lastIndex);

// Test String.prototype.replace with atomic regexp and non-empty string.
r = /a/;
r.lastIndex = 1;
"zzzz".replace(r, "a");
assertEquals(0, r.lastIndex);

// Test String.prototype.replace with non-atomic regexp and non-empty string.
r = /\d/;
r.lastIndex = 1;
"zzzz".replace(r, "a");
assertEquals(0, r.lastIndex);

// Test String.prototype.replace with replacement function
r = /a/;
r.lastIndex = 1;
"zzzz".replace(r, function() { return ""; });
assertEquals(0, r.lastIndex);

// Regexp functions that returns multiple results:
// A global regexp always resets lastIndex regardless of whether it matches.
r = /a/g;
r.lastIndex = -1;
"0123abcd".replace(r, "x");
assertEquals(0, r.lastIndex);

r.lastIndex = -1;
"01234567".replace(r, "x");
assertEquals(0, r.lastIndex);

r.lastIndex = -1;
"0123abcd".match(r);
assertEquals(0, r.lastIndex);

r.lastIndex = -1;
"01234567".match(r);
assertEquals(0, r.lastIndex);

// A non-global regexp resets lastIndex iff it does not match.
r = /a/;
r.lastIndex = -1;
"0123abcd".replace(r, "x");
assertEquals(-1, r.lastIndex);

r.lastIndex = -1;
"01234567".replace(r, "x");
assertEquals(0, r.lastIndex);

r.lastIndex = -1;
"0123abcd".match(r);
assertEquals(-1, r.lastIndex);

r.lastIndex = -1;
"01234567".match(r);
assertEquals(0, r.lastIndex);

// Also test RegExp.prototype.exec and RegExp.prototype.test
r = /a/g;
r.lastIndex = 1;
r.exec("01234567");
assertEquals(0, r.lastIndex);

r.lastIndex = 1;
r.exec("0123abcd");
assertEquals(5, r.lastIndex);

r = /a/;
r.lastIndex = 1;
r.exec("01234567");
assertEquals(0, r.lastIndex);

r.lastIndex = 1;
r.exec("0123abcd");
assertEquals(1, r.lastIndex);

r = /a/g;
r.lastIndex = 1;
r.test("01234567");
assertEquals(0, r.lastIndex);

r.lastIndex = 1;
r.test("0123abcd");
assertEquals(5, r.lastIndex);

r = /a/;
r.lastIndex = 1;
r.test("01234567");
assertEquals(0, r.lastIndex);

r.lastIndex = 1;
r.test("0123abcd");
assertEquals(1, r.lastIndex);
