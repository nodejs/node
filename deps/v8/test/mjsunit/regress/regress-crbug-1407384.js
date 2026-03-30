// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function main() {
  let v0 = 1.5;
  do {
    const v5 = BigInt.asIntN(6, 4n);
    const v6 = v5 / v5;
    const v7 = v6 / v6;
    do {
      [v7];
    } while (v0 < 0);
    --v0;
  } while (v0 < 0);
}
%PrepareFunctionForOptimization(main);
main();
%OptimizeFunctionOnNextCall(main);
main();
