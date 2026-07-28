// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --maglev-verify-dominance --no-maglev-loop-peeling

function foo() {
  const o1 = {};
  o1.self = o1;

  for (let i = 0; i < 5; i++) { }

  const o2 = o1.self;
  o2.self = undefined;

  return o2;
}

%PrepareFunctionForOptimization(foo);
assertEquals({self:undefined}, foo());
assertEquals({self:undefined}, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals({self:undefined}, foo());
