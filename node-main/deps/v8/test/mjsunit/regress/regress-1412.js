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

// Test that the apply with arguments optimization passes values
// unchanged to strict-mode functions and builtins.

// Flags: --allow-natives-syntax

function strict() {
  'use strict';
  return this;
}

function test_strict() {
  assertEquals(void 0, strict.apply(undefined, arguments));
  assertEquals(42, strict.apply(42, arguments));
  assertEquals("asdf", strict.apply("asdf", arguments));
};
%PrepareFunctionForOptimization(test_strict);
for (var i = 0; i < 10; i++) test_strict();
%OptimizeFunctionOnNextCall(test_strict);
test_strict();

function test_builtin(receiver) {
  Object.prototype.valueOf.apply(receiver, arguments);
};
%PrepareFunctionForOptimization(test_builtin);
for (var i = 0; i < 10; i++) test_builtin(this);
%OptimizeFunctionOnNextCall(test_builtin);
test_builtin(this);

var exception = false;
try {
  test_builtin(undefined);
} catch (e) {
  exception = true;
}
assertTrue(exception);
