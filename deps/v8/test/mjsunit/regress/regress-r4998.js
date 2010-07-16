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

// Test for a broken fast-smi-loop that does not save the incremented value
// of the loop index.  If this test fails, it loops forever, and times out.

// Flags: --nofull-compiler

// Calling foo() spills the virtual frame.
function foo() {
  return;
}

function bar() {
  var x1 = 3;
  var x2 = 3;
  var x3 = 3;
  var x4 = 3;
  var x5 = 3;
  var x6 = 3;
  var x7 = 3;
  var x8 = 3;
  var x9 = 3;
  var x10 = 3;
  var x11 = 3;
  var x12 = 3;
  var x13 = 3;

  foo();

  x1 = 257;
  x2 = 258;
  x3 = 259;
  x4 = 260;
  x5 = 261;
  x6 = 262;
  x7 = 263;
  x8 = 264;
  x9 = 265;
  x10 = 266;
  x11 = 267;
  x12 = 268;
  x13 = 269;

  // The loop variable x7 is initialized to 3,
  // and then MakeMergeable is called on the virtual frame.
  // MakeMergeable has forced the loop variable x7 to be spilled,
  // so it is marked as synced
  // The back edge then merges its virtual frame, which incorrectly
  // claims that x7 is synced, and does not save the modified
  // value.
  for (x7 = 3; x7 < 10; ++x7) {
    foo();
  }
}

bar();

function aliasing() {
  var x = 3;
  var j;
  for (j = 7; j < 11; ++j) {
    x = j;
  }

  assertEquals(10, x);
  assertEquals(11, j);
}

aliasing();
