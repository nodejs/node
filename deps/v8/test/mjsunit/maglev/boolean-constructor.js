// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(o) {
  return Boolean(o);
}

%PrepareFunctionForOptimization(f);
%OptimizeMaglevOnNextCall(f);
assertEquals(false, f());
assertEquals(false, f(false));
assertEquals(true, f(true));
assertEquals(true, f(1));
assertEquals(false, f(0));
assertEquals(false, f(undefined));
assertEquals(false, f(null));
assertEquals(true, f("bla"));
assertEquals(false, f(""));
assertEquals(true, f({}));
assertEquals(true, f(Symbol('bla')));
