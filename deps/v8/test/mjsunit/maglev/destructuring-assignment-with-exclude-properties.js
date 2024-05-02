// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

function foo(x) {
  const { a, b, ...rest } = {a:1, b:1, c:1, d:1};
  return rest
}

%PrepareFunctionForOptimization(foo);
assertEquals({c:1, d:1}, foo());
assertEquals({c:1, d:1}, foo());
%OptimizeMaglevOnNextCall(foo);
assertEquals({c:1, d:1}, foo());
assertTrue(isMaglevved(foo));
