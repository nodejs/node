// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

async function foo(f) {
  await f();
}

%PrepareFunctionForOptimization(foo);

foo(async function() {
  throw new Error("errrrr");
}).then(() => assertUnreachable())
    .catch((e) => assertEquals("errrrr", e.message));

%OptimizeFunctionOnNextCall(foo);
