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

// Presents negative opportunities for dead loop removal.

function loop1() {
  while (true) return;
}

function loop2() {
  var i = 0;
  while (i++ < 10) ;
  return i;  // value of {i} escapes.
  // can only remove the loop with induction variable analysis.
}

function loop3() {
  var i = 0;
  for (; i < 10; i++) ;
  return i;  // value of {i} escapes.
  // can only remove the loop with induction variable analysis.
}

function loop4() {
  var a = 0;
  for (var i = 0; i < 10; i++) a++;
  return a;  // value of {a} escapes.
  // can only remove the loop with induction variable analysis.
}

function loop5() {
  var a = new Int32Array(4), sum = 0;
  for (var i = 0; i < a.length; i++) {
    sum += a[i];
  }
  return sum;  // {sum} escapes.
  // can only remove the loop by figuring out that all elements of {a} are 0.
}

function loop6(a) {
  for (var i = 0; i < a; i++) ;  // implicit a.valueOf().
  // can only remove the loop by guarding on the type of a.
}

function loop7(a) {
  for (var i = 0; i < 10; i++) a.toString();  // unknown side-effect on a.
  // can only remove the loop by guarding on the type of a.
}

function loop8(a) {
  for (var i = 0; i < 10; i++) a.valueOf();  // unknown side-effect on a.
  // can only remove the loop by guarding on the type of a.
}

var no_params_loops = [loop1, loop2, loop3, loop4, loop5, loop6];
var params_loops = [loop6, loop7, loop8];

for (var i = 0; i < no_params_loops.length; i++) {
  var f = no_params_loops[i];
  %PrepareFunctionForOptimization(f);
  f();
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
}

for (var i = 0; i < params_loops.length; i++) {
  var f = params_loops[i];
  %PrepareFunctionForOptimization(f);
  f(3);
  f(7);
  f(11);
  %OptimizeFunctionOnNextCall(f);
  f(9);
}
