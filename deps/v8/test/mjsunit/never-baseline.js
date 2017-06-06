// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --ignition --turbo

function f(a, b) {
  %DeoptimizeNow();
  return a + b;
}

// Go through enough optimization and deoptimization cycles in order for the
// function {f} to be marked as optimization disabled.
for (var i = 0; i < 16; ++i) {
  %OptimizeFunctionOnNextCall(f);
  f(1, 2);
}

// Make the runtime profiler perceive {f} as hot again and then verify that we
// didn't trigger an unintentional baseline compilation.
for (var i = 0; i < 100000; ++i) {
  f(1, 2);
}
assertTrue(isInterpreted(f));
