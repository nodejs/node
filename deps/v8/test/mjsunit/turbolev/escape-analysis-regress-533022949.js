// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

function Foo() {
  for (let i = 0; i < 5; i++) {
    %OptimizeOsr();
    const arr = [0.55, 1.55, 2.55, 3.55];
    arr[4000] -= i;
  }
}

%PrepareFunctionForOptimization(Foo);
new Foo();
