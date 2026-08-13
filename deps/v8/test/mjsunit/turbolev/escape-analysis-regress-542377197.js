// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

Object.prototype.__defineGetter__(0, () => 42);

function foo(x) {
  return ![, x][0];
}

%PrepareFunctionForOptimization(foo);
assertEquals(false, foo("abc"));

%OptimizeFunctionOnNextCall(foo);
assertEquals(false, foo("abc"));
