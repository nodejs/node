// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let byte_view = new Uint8Array(8);
for(let i = 0; i < 8; ++i) byte_view[i] = i;

function bar() {
  return [undefined, byte_view[6]];
}

function foo() {
  return bar()[1];
}

%PrepareFunctionForOptimization(foo);
assertEquals(6, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(6, foo());
