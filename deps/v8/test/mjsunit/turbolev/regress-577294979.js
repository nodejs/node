// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0
// Flags: --turbolev-non-eager-inlining --turbolev-non-eager-loop-peeling

function get_false() {
  return false;
}

function get_fn() {
  return get_false;
}

function outer(n, limit) {
  let f = get_fn();
  let i = 0;
  while (i < n && (i < limit || get_false())) {
    i++;
  }
  return f === get_false ? i : -1;
}

%PrepareFunctionForOptimization(get_false);
%PrepareFunctionForOptimization(get_fn);
%PrepareFunctionForOptimization(outer);
assertEquals(10, outer(10, 10));
assertEquals(0, outer(10, 0));
%OptimizeFunctionOnNextCall(outer);
assertEquals(10, outer(10, 10));
assertEquals(0, outer(10, 0));
assertEquals(3, outer(10, 3));
assertEquals(0, outer(0, 10));
