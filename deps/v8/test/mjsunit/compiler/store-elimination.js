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

// Flags: --allow-natives-syntax --store-elimination

// Test local elimination of unobservable stores.

function B(x, y) {
  this.x = x;
  this.y = y;
  return this;
}

function test_store_store() {
  var a = new B(1, 2);
  a.x = 3;  // eliminatable.
  a.x = 4;
  return a.x;
}

function test_store_load_store1() {
  var a = new B(6, 7);
  a.x = 3;  // eliminatable.
  var r = a.y;
  a.x = 4;
  return r;
}

function test_store_load_store2() {
  var a = new B(6, 8);
  a.x = 3;  // not eliminatable, unless next load is eliminated.
  var r = a.x;
  a.x = 4;
  return r;
}

function test_store_call_store() {
  var a = new B(2, 9);
  a.x = 3;  // not eliminatable.
  killall();
  a.x = 4;
  return a.y;
}

function test_store_deopt_store() {
  var a = new B(2, 1);
  a.x = 3;  // not eliminatable (implicit ValueOf following)
  var c = a + 2;
  a.x = 4;
  return a.y;
}

function killall() {
  try { } catch(e) { }
}

%NeverOptimizeFunction(killall);

function test(x, f) {
  %PrepareFunctionForOptimization(f);
  assertEquals(x, f());
  assertEquals(x, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(x, f());
}

test(4, test_store_store);
test(7, test_store_load_store1);
test(3, test_store_load_store2);
test(9, test_store_call_store);
test(1, test_store_deopt_store);
