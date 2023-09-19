// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stack-size=100 --ignore-unhandled-promises

let i = 0;
function f() {
  i++;
  if (i > 10) {
    %PrepareFunctionForOptimization(f);
    %OptimizeFunctionOnNextCall(f);
  }

  new Promise(f);
  return f.x;
}
f();
