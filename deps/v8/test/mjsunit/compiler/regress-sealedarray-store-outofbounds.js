// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

const v3 = [0,"symbol"];
const v5 = 0 - 1;
const v6 = Object.seal(v3);
let v9 = 0;
function f1() {
  v6[119090556] = v5;
}
%PrepareFunctionForOptimization(f1);
f1();
%OptimizeFunctionOnNextCall(f1);
f1();
assertOptimized(f1);
assertEquals(v6.length, 2);
