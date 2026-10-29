// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --stack-size=103

let ticks = 0;

function v0() {
  try { v1(); } catch {}
  // This triggers the deopt that may overflow the stack.
  try { undefined[null] = null; } catch {}
}

function v1() {
  while (!v0()) {
    // Trigger OSR early to get a crashing case asap.
    if (ticks == 5) %OptimizeOsr();
    // With the bug fixed, there's no easy way to trigger termination. Instead,
    // run until we reach a certain number of ticks. The crash triggers locally
    // at tick 7562, thus running until 20k ticks to be somewhat safe.
    if (ticks >= 20000) exit(0);
    ticks++;
  }
}

%PrepareFunctionForOptimization(v0);
%PrepareFunctionForOptimization(v1);

v0();
