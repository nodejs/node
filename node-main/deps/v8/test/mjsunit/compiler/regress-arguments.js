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

// Test of arguments.

// Test passing null or undefined as receiver.
function f() { return this.foo; }

function g() { return f.apply(null, arguments); }
function h() { return f.apply(void 0, arguments); }

var foo = 42;

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();

%PrepareFunctionForOptimization(g);
for (var i = 0; i < 3; i++) assertEquals(42, g());
%OptimizeFunctionOnNextCall(g);
assertEquals(42, g());

%PrepareFunctionForOptimization(h);
for (var i = 0; i < 3; i++) assertEquals(42, h());
%OptimizeFunctionOnNextCall(h);
assertEquals(42, h());

var G1 = 21;
var G2 = 22;

function u() {
 var v = G1 + G2;
 return f.apply(v, arguments);
}

Number.prototype.foo = 42;
delete Number.prototype.foo;

%PrepareFunctionForOptimization(u);
for (var i = 0; i < 3; i++) assertEquals(void 0, u());
%OptimizeFunctionOnNextCall(u);
assertEquals(void 0, u());
