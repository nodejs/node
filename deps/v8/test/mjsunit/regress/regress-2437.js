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

