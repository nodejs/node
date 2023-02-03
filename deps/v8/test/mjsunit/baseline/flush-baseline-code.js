// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code --allow-natives-syntax
// Flags: --baseline-batch-compilation-threshold=0 --sparkplug
// Flags: --no-always-sparkplug --lazy-feedback-allocation
// Flags: --flush-baseline-code --flush-bytecode --no-turbofan --no-maglev
// Flags: --no-stress-concurrent-inlining
// Flags: --no-concurrent-sparkplug

function HasBaselineCode(f) {
  let opt_status = %GetOptimizationStatus(f);
  return (opt_status & V8OptimizationStatus.kBaseline) !== 0;
}

function HasByteCode(f) {
  let opt_status = %GetOptimizationStatus(f);
  return (opt_status & V8OptimizationStatus.kInterpreted) !== 0;
}

var x = {b:20, c:30};
function f() {
  return x.b + 10;
}

// Test bytecode gets flushed
f();
assertTrue(HasByteCode(f));
gc();
assertFalse(HasByteCode(f));

// Test baseline code and bytecode gets flushed
for (i = 1; i < 50; i++) {
  f();
}
assertTrue(HasBaselineCode(f));
gc();
assertFalse(HasBaselineCode(f));
assertFalse(HasByteCode(f));

// Check bytecode isn't flushed if it's held strongly from somewhere but
// baseline code is flushed.
function f1(should_recurse) {
  if (should_recurse) {
    assertTrue(HasByteCode(f1));
    for (i = 1; i < 50; i++) {
      f1(false);
    }
    assertTrue(HasBaselineCode(f1));
    gc();
    // TODO(jgruber, v8:12161): No longer true since we now always tier up to
    // available Sparkplug code as early as possible. By the time we reach this
    // assert, SP code is being executed and is thus alive.
    // assertFalse(HasBaselineCode(f1));
    // Also, the active tier is Sparkplug and not Ignition.
    // assertTrue(ActiveTierIsIgnition(f1));
  }
  return x.b + 10;
}

f1(false);
// Recurse first time so we have bytecode array on the stack that keeps
// bytecode alive.
f1(true);

// Flush bytecode
gc();
assertFalse(HasBaselineCode(f1));
assertFalse(HasByteCode(f1));

// Check baseline code and bytecode aren't flushed if baseline code is on
// stack.
function f2(should_recurse) {
  if (should_recurse) {
    assertTrue(HasBaselineCode(f2));
    f2(false);
    gc();
    assertTrue(HasBaselineCode(f2));
  }
  return x.b + 10;
}

for (i = 1; i < 50; i++) {
  f2(false);
}
assertTrue(HasBaselineCode(f2));
// Recurse with baseline code on stack
f2(true);
