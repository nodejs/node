// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --optimize-maglev-optimizes-to-turbofan --turbolev
// Flags: --allow-natives-syntax --no-use-ic --maglev

function foo() {
  let x = { pi: Math.PI };
}
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
