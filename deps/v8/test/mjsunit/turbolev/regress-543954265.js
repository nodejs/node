// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev-disable-builtin-reducers

function foo() {
  for (let i = 0; i < 5; i++) {
    Math.clz32(Math);
    %OptimizeOsr();
  }
}
%PrepareFunctionForOptimization(foo);
foo();
