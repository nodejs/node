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

// Flags: --allow-natives-syntax

// Test deopt with count operation on parameter.
var max_smi = 1073741823;
var o = {x:0};

function inc1(x) { x++; o.x = x; }
inc1(max_smi);
assertEquals(max_smi + 1, o.x);

inc1(1.1);
assertEquals(2.1, o.x);


// Test deopt with count operation on named property.
function inc2(p) { p.x++ }

%PrepareFunctionForOptimization(inc2);

o.x = "42";
inc2(o);
assertEquals(43, o.x);

var s = max_smi - 10;
o.x = s;
for(var i = 0; i < 20; i++) {
  inc2(o);
  if (i == 4) {
    %OptimizeFunctionOnNextCall(inc2);
  }
}
assertEquals(max_smi + 10, o.x);


// Test deopt with count operation on keyed property.
function inc3(a, b) { a[b]++; }

%PrepareFunctionForOptimization(inc3);

o = ["42"];
inc3(o, 0);
assertEquals(43, o[0]);

var s = max_smi - 10;
o[0] = s;
for(var i = 0; i < 20; i++) {
  inc3(o, 0);
  if (i == 4) {
    %OptimizeFunctionOnNextCall(inc3);
  }
}
assertEquals(max_smi + 10, o[0]);

inc3(o,"0");

assertEquals(max_smi + 11, o[0]);

// Test bailout when accessing a non-existing array element.
o[0] = 0;
for(var i = 0; i < 5; i++) {
  inc3(o, 0);
}
%PrepareFunctionForOptimization(inc3);
%OptimizeFunctionOnNextCall(inc3);
inc3(o, 0);
inc3(o, 1);

// Test bailout with count operation in a value context.
function inc4(x,y) { return (x++) + y; }
%PrepareFunctionForOptimization(inc4);
for (var i = 0; i < 5; ++i) assertEquals(3, inc4(2, 1));
%OptimizeFunctionOnNextCall(inc4);
inc4(2, 1);
assertEquals(3.1, inc4(2, 1.1));

function inc5(x,y) { return (++x) + y; }
%PrepareFunctionForOptimization(inc5);
for (var i = 0; i < 5; ++i) assertEquals(4, inc5(2, 1));
%OptimizeFunctionOnNextCall(inc5);
assertEquals(4, inc5(2, 1));
assertEquals(4.1, inc5(2, 1.1));
assertEquals(4.1, inc5(2.1, 1));

function inc6(o,y) { return (o.x++) + y; }
%PrepareFunctionForOptimization(inc6);
o = {x:0};
for (var i = 0; i < 5; ++i) {
  o.x = 42;
  assertEquals(43, inc6(o, 1));
}
%OptimizeFunctionOnNextCall(inc6);
o.x = 42;
assertEquals(43, inc6(o, 1));
o.x = 42;
assertEquals(43.1, inc6(o, 1.1));
o.x = 42.1;
assertEquals(43.1, inc6(o, 1));

function inc7(o,y) { return (++o.x) + y; }
%PrepareFunctionForOptimization(inc7);
o = {x:0};
for (var i = 0; i < 5; ++i) {
  o.x = 42;
  assertEquals(44, inc7(o, 1));
}
%OptimizeFunctionOnNextCall(inc7);
o.x = 42;
assertEquals(44, inc7(o, 1));
o.x = 42;
assertEquals(44.1, inc7(o, 1.1));
o.x = 42.1;
assertEquals(44.1, inc7(o, 1));

function inc8(o,y) { return (o[0]++) + y; }
%PrepareFunctionForOptimization(inc8);
var q = [0];
for (var i = 0; i < 5; ++i) {
  q[0] = 42;
  assertEquals(43, inc8(q, 1));
}
%OptimizeFunctionOnNextCall(inc8);
q[0] = 42;
assertEquals(43, inc8(q, 1));
q[0] = 42;
assertEquals(43.1, inc8(q, 1.1));
q[0] = 42.1;
assertEquals(43.1, inc8(q, 1));

function inc9(o,y) { return (++o[0]) + y; }
%PrepareFunctionForOptimization(inc9);
q = [0];
for (var i = 0; i < 5; ++i) {
  q[0] = 42;
  assertEquals(44, inc9(q, 1));
}
%OptimizeFunctionOnNextCall(inc9);
q[0] = 42;
assertEquals(44, inc9(q, 1));
q[0] = 42;
assertEquals(44.1, inc9(q, 1.1));
q[0] = 42.1;
assertEquals(44.1, inc9(q, 1));

// Test deopt because of a failed map check.
function inc10(p) { return p.x++ }
%PrepareFunctionForOptimization(inc10);
var g1 = {x:0};
var g2 = {y:0, x:42}
for (var i = 0; i < 5; ++i) {
  g1.x = 42;
  assertEquals(42, inc10(g1));
  assertEquals(43, g1.x);
}
%OptimizeFunctionOnNextCall(inc10);
g1.x = 42;
assertEquals(42, inc10(g1));
assertEquals(43, g1.x);
assertEquals(42, inc10(g2));
assertEquals(43, g2.x);

// Test deoptimization with postfix operation in a value context.
function inc11(a) { return a[this.x++]; }
var g3 = {x:null, f:inc11};
var g4 = [42];
assertEquals(42, g3.f(g4));
