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

// Flags: --use-osr --allow-natives-syntax --crankshaft

var o1 = {a : 10};
var o2 = { };
o2.__proto__ = o1;
var o3 = { };
o3.__proto__ = o2;

function f(n, x, b) {
  var sum = x.a;
  for (var i = 0; i < n; i++) {
    sum = 1.0 / i;
  }
  return sum;
}

f(10, o3);
f(20, o3);
f(30, o3);
%OptimizeFunctionOnNextCall(f, "concurrent");
f(100000, o3);
// At this point OSR replaces already optimized code.
// Check that it evicts old code from cache.

// This causes all code for f to be lazily deopted.
o2.a = 5;

// If OSR did not evict the old code, it will be installed in f here.
%OptimizeFunctionOnNextCall(f);
f(10, o3);

// The old code is already deoptimized, but f still points to it.
// Disassembling it will crash.
%DisassembleFunction(f);
