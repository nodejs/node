// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --for-of-optimization

const iterable = {
  [Symbol.iterator]: function() {
    return {next: -1073741825};
  }
};

function* testGenerator() {
  for (const item of iterable) {
    // This will throw a TypeError because .next is not a function.
    print(42);
  }
}

%PrepareFunctionForOptimization(testGenerator);
assertThrows(() => {
  testGenerator().next();
}, TypeError);
assertThrows(() => {
  testGenerator().next();
}, TypeError);
%OptimizeMaglevOnNextCall(testGenerator);
assertThrows(() => {
  testGenerator().next();
}, TypeError);
