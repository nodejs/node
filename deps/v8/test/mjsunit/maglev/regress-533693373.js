// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-lazy-feedback-allocation
// Flags: --single-threaded

function* emptyGen() {
}
%PrepareFunctionForOptimization(emptyGen);

function doForOf() {
  for (let obj of emptyGen()) { }
};
%PrepareFunctionForOptimization(doForOf);

function foo() {
  for (let zero = 0; true;) {
    doForOf();
    // This will throw:
    [].forEach(zero);
  }
}
%PrepareFunctionForOptimization(foo);

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

for (let i = 0; i < 5; i++) {
  try {
    foo();
  } catch (e) {}
  %OptimizeOsr();
}
