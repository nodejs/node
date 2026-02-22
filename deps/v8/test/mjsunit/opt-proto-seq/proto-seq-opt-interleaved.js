// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --allow-natives-syntax
// Flags: --no-jitless
// Flags: --no-lazy-feedback-allocation
// Flags: --maglev --turbofan

function test_interleaved_statements() {
  function F() {}
  var x = 0;
  F.prototype.a = 1;
  x = 1; // Interruption, sequence break
  F.prototype.b = 2;
  return { F, x };
}

function assert_test_interleaved_statements(res) {
  const F = res.F;
  assertEquals(1, F.prototype.a);
  assertEquals(2, F.prototype.b);
  assertEquals(1, res.x);
}

// prettier-ignore
function run(){
assert_test_interleaved_statements(test_interleaved_statements());
%CompileBaseline(test_interleaved_statements);
assert_test_interleaved_statements(test_interleaved_statements());
%PrepareFunctionForOptimization(test_interleaved_statements);
assert_test_interleaved_statements(test_interleaved_statements());
assert_test_interleaved_statements(test_interleaved_statements());
%OptimizeMaglevOnNextCall(test_interleaved_statements);
assert_test_interleaved_statements(test_interleaved_statements());
assertOptimized(test_interleaved_statements);
assertTrue(isMaglevved(test_interleaved_statements));
assert_test_interleaved_statements(test_interleaved_statements());
%OptimizeFunctionOnNextCall(test_interleaved_statements);
assert_test_interleaved_statements(test_interleaved_statements());
assertOptimized(test_interleaved_statements);
assert_test_interleaved_statements(test_interleaved_statements());
}

run();
