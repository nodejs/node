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

// Test local load elimination of redundant loads and stores.

function B(x, y) {
  this.x = x;
  this.y = y;
  return this;
}

function C() {
}

function test_load() {
  var a = new B(1, 2);
  return a.x + a.x + a.x + a.x;
}


function test_load_from_different_contexts() {
  var r = 1;
  this.f = function() {
    var fr = r;
    this.g = function(flag) {
      var gr;
      if (flag) {
        gr = r;
      } else {
        gr = r;
      }
      return gr + r + fr;
    };
  };
  this.f();
  return this.g(true);
}


function test_store_load() {
  var a = new B(1, 2);
  a.x = 4;
  var f = a.x;
  a.x = 5;
  var g = a.x;
  a.x = 6;
  var h = a.x;
  a.x = 7;
  return f + g + h + a.x;
}

function test_nonaliasing_store1() {
  var a = new B(2, 3), b = new B(3, 4);
  b.x = 4;
  var f = a.x;
  b.x = 5;
  var g = a.x;
  b.x = 6;
  var h = a.x;
  b.x = 7;
  return f + g + h + a.x;
}

function test_transitioning_store1() {
  var a = new B(2, 3);
  var f = a.x, g = a.y;
  var b = new B(3, 4);
  return a.x + a.y;
}

function test_transitioning_store2() {
  var b = new C();
  var a = new B(-1, 5);
  var f = a.x, g = a.y;
  b.x = 9;
  b.y = 11;
  return a.x + a.y;
}

var false_v = false;
function test_transitioning_store3() {
  var o = new C();
  var v = o;
  if (false_v) v = 0;
  v.x = 20;
  return o.x;
}

function killall() {
  try { } catch(e) { }
}

%NeverOptimizeFunction(killall);

function test_store_load_kill() {
  var a = new B(1, 2);
  a.x = 4;
  var f = a.x;
  a.x = 5;
  var g = a.x;
  killall();
  a.x = 6;
  var h = a.x;
  a.x = 7;
  return f + g + h + a.x;
}

function test_store_store() {
  var a = new B(6, 7);
  a.x = 7;
  a.x = 7;
  a.x = 7;
  a.x = 7;
  return a.x;
}

function test(x, f) {
  assertEquals(x, f());
  assertEquals(x, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(x, f());
}

test(4, test_load);
test(3, new test_load_from_different_contexts().g);
test(22, test_store_load);
test(8, test_nonaliasing_store1);
test(5, test_transitioning_store1);
test(4, test_transitioning_store2);
test(20, test_transitioning_store3);
test(22, test_store_load_kill);
test(7, test_store_store);
