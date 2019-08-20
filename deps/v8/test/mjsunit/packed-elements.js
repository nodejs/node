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

// Flags: --allow-natives-syntax

function test1() {
  var a = Array(8);
  assertTrue(%HasSmiOrObjectElements(a));
  assertTrue(%HasHoleyElements(a));
}

function test2() {
  var a = Array();
  assertTrue(%HasSmiOrObjectElements(a));
  assertFalse(%HasHoleyElements(a));
}

function test3() {
  var a = Array(1,2,3,4,5,6,7);
  assertTrue(%HasSmiOrObjectElements(a));
  assertFalse(%HasHoleyElements(a));
}

function test4() {
  var a = [1, 2, 3, 4];
  assertTrue(%HasSmiElements(a));
  assertFalse(%HasHoleyElements(a));
  var b = [1, 2,, 4];
  assertTrue(%HasSmiElements(b));
  assertTrue(%HasHoleyElements(b));
}

function test5() {
  var a = [1, 2, 3, 4.5];
  assertTrue(%HasDoubleElements(a));
  assertFalse(%HasHoleyElements(a));
  var b = [1,, 3.5, 4];
  assertTrue(%HasDoubleElements(b));
  assertTrue(%HasHoleyElements(b));
  var c = [1, 3.5,, 4];
  assertTrue(%HasDoubleElements(c));
  assertTrue(%HasHoleyElements(c));
}

function test6() {
  var x = new Object();
  var a = [1, 2, 3.5, x];
  assertTrue(%HasObjectElements(a));
  assertFalse(%HasHoleyElements(a));
  assertEquals(1, a[0]);
  assertEquals(2, a[1]);
  assertEquals(3.5, a[2]);
  assertEquals(x, a[3]);
  var b = [1,, 3.5, x];
  assertTrue(%HasObjectElements(b));
  assertTrue(%HasHoleyElements(b));
  assertEquals(1, b[0]);
  assertEquals(undefined, b[1]);
  assertEquals(3.5, b[2]);
  assertEquals(x, b[3]);
  var c = [1, 3.5, x,,];
  assertTrue(%HasObjectElements(c));
  assertTrue(%HasHoleyElements(c));
  assertEquals(1, c[0]);
  assertEquals(3.5, c[1]);
  assertEquals(x, c[2]);
  assertEquals(undefined, c[3]);
}

function test_with_optimization(f) {
  // Run tests in a loop to make sure that inlined Array() constructor runs out
  // of new space memory and must fall back on runtime impl.
  %PrepareFunctionForOptimization(f);
  for (i = 0; i < 25000; ++i) f();
  %OptimizeFunctionOnNextCall(f);
  for (i = 0; i < 25000; ++i) f(); // Make sure GC happens
}

test_with_optimization(test1);
test_with_optimization(test2);
test_with_optimization(test3);
test_with_optimization(test4);
test_with_optimization(test5);
test_with_optimization(test6);
