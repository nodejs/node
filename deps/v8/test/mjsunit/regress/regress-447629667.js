// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __wrapTC(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}

function foo() {
  const __v_13 = __wrapTC(() => undefined);
  return [ __v_13];
}

const __v_8 = __wrapTC();
%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
assertEquals([undefined], foo());
