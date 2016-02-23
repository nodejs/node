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

// Flags: --allow-natives-syntax

// Presents opportunities for dead loop removal.

function loop1() {
  while (false) ;  // doesn't even loop.
}

function loop2() {
  var i = 0;
  while (i++ < 10) ;  // nothing in the body.
}

function loop3() {
  for (var i = 0; i < 10; i++) ;  // nothing in the body.
}

function loop4() {
  var a = 0;
  for (var i = 0; i < 10; i++) a++;  // {a} is dead after the loop.
}

function loop5() {
  var a = new Int32Array(4), sum = 0;
  for (var i = 0; i < a.length; i++) {
    // Involves only reads on typed arrays, and {i} doesn't overflow.
    sum += a[i];
  }
}

function loop6() {
  var a = new Array(4), sum = 0;
  for (var i = 0; i < a.length; i++) {
    // Involves only in-bounds read on the array {a}.
    // Have to prove that {a} doesn't have getters...?
    sum += a[i];
  }
}

function loop7() {
  for (var i = 0; i < 10; i++) {
    new Object();  // Have to prove the allocation doesn't escape.
  }
}

function loop8() {
  for (var i = 0; i < 10; i++) {
    var x = {};  // Have to prove the allocation doesn't escape.
  }
}

var loops = [loop1, loop2, loop3, loop4, loop5, loop6, loop7, loop8];

for (var i = 0; i < loops.length; i++) {
  var f = loops[i];
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
}
