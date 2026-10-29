// Copyright 2011 the V8 project authors. All rights reserved.
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

// Test deoptimization after inlined call.

function bar(a, b) {try { return a; } finally { } }

function test_context() {
  function foo(x) { return 42; }
  var s, t;
  for (var i = 0x7fff0000; i < 0x80000000; i++) {
    bar(t = foo(i) ? bar(42 + i - i) : bar(0), s = i + t);
  }
  return s;
}
assertEquals(0x7fffffff + 42, test_context());


function value_context() {
  function foo(x) { return 42; }
  var s, t;
  for (var i = 0x7fff0000; i < 0x80000000; i++) {
    bar(t = foo(i), s = i + t);
  }
  return s;
}
assertEquals(0x7fffffff + 42, value_context());


function effect_context() {
  function foo(x) { return 42; }
  var s, t;
  for (var i = 0x7fff0000; i < 0x80000000; i++) {
    bar(foo(i), s = i + 42);
  }
  return s;
}
assertEquals(0x7fffffff + 42, effect_context());
