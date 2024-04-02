// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt
{
  let a = 0n;
  a = 3n;

  function TestAdd() {
    let sum = 0n;

    for (let i = 0; i < 3; ++i) {
      sum = a + sum;
    }

    return sum;
  }

  %PrepareFunctionForOptimization(TestAdd);
  TestAdd();
  TestAdd();
  %OptimizeFunctionOnNextCall(TestAdd);
  TestAdd();
  TestAdd();
}
