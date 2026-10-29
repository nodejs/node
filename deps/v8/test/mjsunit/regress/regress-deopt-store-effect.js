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

// Test deopt after generic store with effect context.
var pro = {x: 1};
var a = {};
a.__proto__ = pro;
delete pro.x;

function g(o) {
  return 7 + (o.z = 1, 20);
};
%PrepareFunctionForOptimization(g);
g(a);
g(a);
%OptimizeFunctionOnNextCall(g);
Object.defineProperty(pro, 'z', {
  set: function(v) {
    %DeoptimizeFunction(g);
  },
  get: function() {
    return 20;
  }
});

assertEquals(27, g(a));

// Test deopt after polymorphic as monomorphic store with effect context.

var i = {z: 2, r: 1};
var j = {z: 2};
var p = {a: 10};
var pp = {a: 20, b: 1};

function bar(o, p) {
  return 7 + (o.z = 1, p.a);
};
%PrepareFunctionForOptimization(bar);
bar(i, p);
bar(i, p);
bar(j, p);
%OptimizeFunctionOnNextCall(bar);
assertEquals(27, bar(i, pp));

// Test deopt after polymorphic store with effect context.

var i = {r: 1, z: 2};
var j = {z: 2};
var p = {a: 10};
var pp = {a: 20, b: 1};

function bar1(o, p) {
  return 7 + (o.z = 1, p.a);
};
%PrepareFunctionForOptimization(bar1);
bar1(i, p);
bar1(i, p);
bar1(j, p);
%OptimizeFunctionOnNextCall(bar1);
assertEquals(27, bar1(i, pp));
