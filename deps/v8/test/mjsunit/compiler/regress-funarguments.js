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

// Test function.arguments.

function A() {}
function B() {}

function fee(x, y) {
  if (x == 1) return fee["arg" + "uments"];
  if (x == 2) return gee["arg" + "uments"];
  return 42;
}

function gee(x) { return this.f(2 - x, "f"); }

function foo(x, y) {
  if (x == 0) return foo["arg" + "uments"];
  if (x == 1) return goo["arg" + "uments"];
  return 42;
}

function goo(x) { return this.f(x, "f"); }

A.prototype.f = fee;
A.prototype.g = gee;

B.prototype.f = foo;
B.prototype.g = goo;

var o = new A();

function hej(x) {
  if (x == 0) return o.g(x, "h");
  if (x == 1) return o.g(x, "h");
  return o.g(x, "z");
}

function opt_g() {
  %PrepareFunctionForOptimization(o.g);
  for (var k=0; k<2; k++) {
    for (var i=0; i<5; i++) o.g(i, "g");
  }
  %OptimizeFunctionOnNextCall(o.g);
  o.g(0, "g");
}

function opt_hej() {
  %PrepareFunctionForOptimization(hej);
  for (var k=0; k<2; k++) {
    for (var j=0; j<5; j++) hej(j);
  }
  %OptimizeFunctionOnNextCall(hej);
  hej(0)
}

opt_g();
opt_hej();
assertArrayEquals([0, "g"], o.g(0, "g"));
assertArrayEquals([1, "f"], o.g(1, "g"));
assertArrayEquals([0, "h"], hej(0));
assertArrayEquals([1, "f"], hej(1));

o = new B();

opt_g();
opt_hej();
assertArrayEquals([0, "f"], o.g(0, "g"));
assertArrayEquals([1, "g"], o.g(1, "g"));
assertArrayEquals([0, "f"], hej(0));
assertArrayEquals([1, "h"], hej(1));
