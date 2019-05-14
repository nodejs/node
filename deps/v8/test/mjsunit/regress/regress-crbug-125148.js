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

function ToDictionaryMode(x) {
  %OptimizeObjectForAddingMultipleProperties(x, 100);
}

var A, B, C;

// The initial bug report was about calling a know function...
A = {};
Object.defineProperty(A, "foo", { value: function() { assertUnreachable(); }});

B = Object.create(A);
Object.defineProperty(B, "foo", { value: function() { return 111; }});

C = Object.create(B);

function bar(x) { return x.foo(); }

assertEquals(111, bar(C));
assertEquals(111, bar(C));
ToDictionaryMode(B);
%OptimizeFunctionOnNextCall(bar);
assertEquals(111, bar(C));

// Although this was not in the initial bug report: The same for getters...
A = {};
Object.defineProperty(A, "baz", { get: function() { assertUnreachable(); }});

B = Object.create(A);
Object.defineProperty(B, "baz", { get: function() { return 111; }});

C = Object.create(B);

function boo(x) { return x.baz; }

assertEquals(111, boo(C));
assertEquals(111, boo(C));
ToDictionaryMode(B);
%OptimizeFunctionOnNextCall(boo);
assertEquals(111, boo(C));

// And once more for setters...
A = {};
Object.defineProperty(A, "huh", { set: function(x) { assertUnreachable(); }});

B = Object.create(A);
var setterValue;
Object.defineProperty(B, "huh", { set: function(x) { setterValue = x; }});

C = Object.create(B);

function fuu(x) {
  setterValue = 222;
  x.huh = 111;
  return setterValue;
}

assertEquals(111, fuu(C));
assertEquals(111, fuu(C));
ToDictionaryMode(B);
%OptimizeFunctionOnNextCall(fuu);
assertEquals(111, fuu(C));
