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

function argc0() {
  return arguments.length;
}

function argc1(i) {
  return arguments.length;
}

function argc2(i, j) {
  return arguments.length;
}

assertEquals(0, argc0());
assertEquals(1, argc0(1));
assertEquals(2, argc0(1, 2));
assertEquals(3, argc0(1, 2, 3));
assertEquals(0, argc1());
assertEquals(1, argc1(1));
assertEquals(2, argc1(1, 2));
assertEquals(3, argc1(1, 2, 3));
assertEquals(0, argc2());
assertEquals(1, argc2(1));
assertEquals(2, argc2(1, 2));
assertEquals(3, argc2(1, 2, 3));



var index;

function argv0() {
  return arguments[index];
}

function argv1(i) {
  return arguments[index];
}

function argv2(i, j) {
  return arguments[index];
}

index = 0;
assertEquals(7, argv0(7));
assertEquals(7, argv0(7, 8));
assertEquals(7, argv0(7, 8, 9));
assertEquals(7, argv1(7));
assertEquals(7, argv1(7, 8));
assertEquals(7, argv1(7, 8, 9));
assertEquals(7, argv2(7));
assertEquals(7, argv2(7, 8));
assertEquals(7, argv2(7, 8, 9));

index = 1;
assertEquals(8, argv0(7, 8));
assertEquals(8, argv0(7, 8));
assertEquals(8, argv1(7, 8, 9));
assertEquals(8, argv1(7, 8, 9));
assertEquals(8, argv2(7, 8, 9));
assertEquals(8, argv2(7, 8, 9));

index = 2;
assertEquals(9, argv0(7, 8, 9));
assertEquals(9, argv1(7, 8, 9));
assertEquals(9, argv2(7, 8, 9));


// Test that calling a lazily compiled function with
// an unexpected number of arguments works.
function f(a) { return arguments.length; };
assertEquals(3, f(1, 2, 3));
