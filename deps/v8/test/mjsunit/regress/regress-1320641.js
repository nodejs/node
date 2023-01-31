// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(){
  const xs = new Uint16Array(3775336418);
  return xs[-981886074];
}
%PrepareFunctionForOptimization(foo);
foo();

assertEquals(undefined, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo());
