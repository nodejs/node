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

// Flags: --allow-natives-syntax --load-elimination

// Test global load elimination of redundant loads and stores.

var X = true;  // For forcing branches.
X = false;
X = true;
X = false;

function B(x, y) {
  this.x = x;
  this.y = y;
  return this;
}

function test_load() {
  var a = new B(1, 2);
  var f = a.x + a.x;
  if (false) ;
  return f + a.x + a.x;
}

function test_load2() {
  var a = new B(1, 2);
  var f = a.x + a.x;
  if (true) ;
  return f + a.x + a.x;
}

function test_store_load() {
  var a = new B(1, 2);
  a.x = 4;
  var b = X ? a.x : a.x;
  return b + a.x;
}

function test_store_load2() {
  var a = new B(1, 2);
  var c = 6;
  if (X) a.x = c;
  else a.x = c;
  return a.x + a.x;
}

function test_nonaliasing_store1() {
  var a = new B(2, 3), b = new B(3, 4);
  if (X) ;
  b.x = 4;
  if (X) ;
  var f = a.x;
  if (X) ;
  b.x = 5;
  if (X) ;
  var g = a.x;
  if (X) ;
  b.x = 6;
  if (X) ;
  var h = a.x;
  if (X) ;
  b.x = 7;
  if (X) ;
  return f + g + h + a.x;
}

function test_loop(x) {
  var a = new B(2, 3);
  var v = a.x;
  var total = v;
  var i = 0;
  while (i++ < 10) {
    total = a.x;
    a.y = 4;
  }
  return total;
}

function test_loop2(x) {
  var a = new B(2, 3);
  var v = a.x;
  var total = v;
  var i = 0;
  while (i++ < 10) {
    total = a.x;  // a.x not affected by loop
    a.y = 4;

    var j = 0;
    while (j++ < 10) {
      total = a.x;  // a.x not affected by loop
      a.y = 5;
    }

    total = a.x;
    a.y = 6;

    j = 0;
    while (j++ < 10) {
      total = a.x;  // a.x not affected by loop
      a.y = 7;
    }
  }
  return total;
}

function killall() {
  try { } catch(e) { }
}

%NeverOptimizeFunction(killall);

function test_store_load_kill() {
  var a = new B(1, 2);
  if (X) ;
  a.x = 4;
  if (X) ;
  var f = a.x;
  if (X) ;
  a.x = 5;
  if (X) ;
  var g = a.x;
  if (X) ;
  killall();
  if (X) ;
  a.x = 6;
  if (X) ;
  var h = a.x;
  if (X) ;
  a.x = 7;
  if (X) ;
  return f + g + h + a.x;
}

function test_store_store() {
  var a = new B(6, 7);
  if (X) ;
  a.x = 7;
  if (X) ;
  a.x = 7;
  if (X) ;
  a.x = 7;
  if (X) ;
  a.x = 7;
  if (X) ;
  return a.x;
}

function test(x, f) {
  X = true;
  assertEquals(x, f());
  assertEquals(x, f());
  X = false;
  assertEquals(x, f());
  assertEquals(x, f());
  X = true;
  %OptimizeFunctionOnNextCall(f);
  assertEquals(x, f());
  assertEquals(x, f());
  X = false;
  assertEquals(x, f());
  assertEquals(x, f());
}

test(4, test_load);
test(8, test_store_load);
test(12, test_store_load2);
test(8, test_nonaliasing_store1);
test(22, test_store_load_kill);
test(7, test_store_store);
test(2, test_loop);
test(2, test_loop2);
