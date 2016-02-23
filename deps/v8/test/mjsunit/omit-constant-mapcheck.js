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

var g1 = { a:1 }

function load() {
  return g1.a;
}

assertEquals(1, load());
assertEquals(1, load());
%OptimizeFunctionOnNextCall(load);
assertEquals(1, load());
delete g1.a;
assertEquals(undefined, load());

var g2 = { a:2 }

function load2() {
  return g2.a;
}

assertEquals(2, load2());
assertEquals(2, load2());
%OptimizeFunctionOnNextCall(load2);
assertEquals(2, load2());
g2.b = 10;
g2.a = 5;
assertEquals(5, load2());

var g3 = { a:2, b:9, c:1 }

function store(v) {
  g3.a = v;
  return g3.a;
}

assertEquals(5, store(5));
assertEquals(8, store(8));
%OptimizeFunctionOnNextCall(store);
assertEquals(10, store(10));
delete g3.c;
store(7);
assertEquals({a:7, b:9}, g3);
