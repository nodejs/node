// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --block-concurrent-recompilation --noalways-opt

global = 1;

function boom(value) {
  return global;
}

%PrepareFunctionForOptimization(boom);
assertEquals(1, boom());
assertEquals(1, boom());
%OptimizeFunctionOnNextCall(boom, "concurrent");
assertEquals(1, boom());

this.__defineGetter__("global", () => 42);

%UnblockConcurrentRecompilation();

// boom should be deoptimized because the global property cell has changed.
assertUnoptimized(boom, "sync");

assertEquals(42, boom());
