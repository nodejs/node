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

function s(v) {
  v.x = 1;
}

function s_strict(v) {
  "use strict";
  v.x = 1;
}

function c(p) {
  return {__proto__: p};
}

var p = {};

var o1 = c(p);
var o2 = c(p);
var o3 = c(p);
var o4 = c(p);

// Make p go slow.
// Do this after using p as prototype, since using an object as prototype kicks
// it back into fast mode.
p.y = 1;
delete p.y;
p.x = 5;

// Initialize the store IC.
s(o1);
s(o2);
s_strict(o1);
s_strict(o2);

// Make x non-writable.
Object.defineProperty(p, "x", { writable: false });

// Verify that direct setting fails.
o3.x = 20;
assertEquals(5, o3.x);

// Verify that setting through the IC fails.
s(o4);
assertEquals(5, o4.x);
assertThrows("s_strict(o4);", TypeError);
