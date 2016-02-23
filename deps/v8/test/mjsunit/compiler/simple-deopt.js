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

function f(x) {
  return ~x;
}

f(42);
assertEquals(~12, f(12.45));
assertEquals(~42, f(42.87));


var a = 1, b = 2, c = 4, d = 8;
function g() {
  return a | (b | (c | d));
}

g();
c = "16";
assertEquals(1 | 2 | 16 | 8, g());


// Test deopt when global function changes.
function h() {
  return g();
}
assertEquals(1 | 2 | 16 | 8, h());
g = function() { return 42; };
assertEquals(42, h());


// Test deopt when map changes.
var obj = {};
obj.g = g;
function k(o) {
  return o.g();
}
for (var i = 0; i < 5; i++) k(obj);
%OptimizeFunctionOnNextCall(k);
k(obj);
assertEquals(42, k(obj));
assertEquals(87, k({g: function() { return 87; }}));


// Test deopt with assignments to parameters.
function p(x,y) {
  x = 42;
  y = 1;
  y = y << "0";
  return x | y;
}
assertEquals(43, p(0,0));


// Test deopt with literals on the expression stack.
function LiteralToStack(x) {
  return 'lit[' + (x + ']');
}

assertEquals('lit[-87]', LiteralToStack(-87));
assertEquals('lit[0]', LiteralToStack(0));
assertEquals('lit[42]', LiteralToStack(42));


// Test deopt before call.
var str = "abc";
var r;
function CallCharAt(n) { return str.charAt(n); }
for (var i = 0; i < 5; i++) {
  r = CallCharAt(0);
}
%OptimizeFunctionOnNextCall(CallCharAt);
r = CallCharAt(0);
assertEquals("a", r);


// Test of deopt in presence of spilling.
function add4(a,b,c,d) {
  return a+b+c+d;
}
assertEquals(0x40000003, add4(1,1,2,0x3fffffff));
