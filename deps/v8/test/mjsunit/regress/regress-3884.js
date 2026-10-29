// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function f(x) {
  // TurboFan will hoist the CompareIC for x === 'some_string' and spill it.
  if (x === 'some_other_string_1' || x === 'some_string') {
    gc();
  }
  if (x === 'some_other_string_2' || x === 'some_string') {
    gc();
  }
  // TurboFan will hoist the CompareIC for x === 1.4 and spill it.
  if (x === 1.7 || x === 1.4) {
    gc();
  }
  if (x === 1.9 || x === 1.4) {
    gc();
  }
};
%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);

f('some_other_string_1');
f(1.7);
