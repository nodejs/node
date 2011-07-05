// Copyright 2010 the V8 project authors. All rights reserved.
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

// Test that installing a getter on the global object instead of a
// normal property works.

var x = 0;

function getX() { return x; }

for (var i = 0; i < 10; i++) {
  assertEquals(i < 5 ? 0 : 42, getX());
  if (i == 4) __defineGetter__("x", function() { return 42; });
}


// Test that installing a setter on the global object instead of a
// normal property works.

var y = 0;
var setter_y;

function setY(value) { y = value; }

for (var i = 0; i < 10; i++) {
  setY(i);
  assertEquals(i < 5 ? i : 2 * i, y);
  if (i == 4) {
    __defineSetter__("y", function(value) { setter_y = 2 * value; });
    __defineGetter__("y", function() { return setter_y; });
  }
}


// Test that replacing a getter with a normal property works as
// expected.

__defineGetter__("z", function() { return 42; });

function getZ() { return z; }

for (var i = 0; i < 10; i++) {
  assertEquals(i < 5 ? 42 : 0, getZ());
  if (i == 4) {
    delete z;
    var z = 0;
  }
}
