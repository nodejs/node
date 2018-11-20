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

var slow = {};
var p = {};

slow.__proto__ = p;
slow.x = 10;
slow.y = 10;
Object.defineProperty(p, "x", {writable:false, value:5});

function c(p) {
  return {__proto__: p};
}

function s(v) {
  return v.x = 1;
}

function s_strict(v) {
  "use strict";
  v.x = 1;
}

var o1 = c(slow);
var o2 = c(slow);
var o1_strict = c(slow);
var o2_strict = c(slow);
var o3 = c(slow);
var o4 = c(slow);

// Make s slow.
// Do this after using slow as prototype, since using an object as prototype
// kicks it back into fast mode.
delete slow.y;

s(o1);
s(o2);
s_strict(o1_strict);
s_strict(o2_strict);

delete slow.x;
// Directly setting x should fail.
o3.x = 20
assertEquals(5, o3.x);

// Setting x through IC should fail.
s(o4);
assertEquals(5, o4.x);
assertThrows("s_strict(o4);", TypeError);
