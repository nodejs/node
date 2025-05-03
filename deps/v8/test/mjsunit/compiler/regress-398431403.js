// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

const v1 = new BigInt64Array();

function f2(acc, current) {
  return ArrayBuffer.isView(acc);
}
%PrepareFunctionForOptimization(f2);

function f6() {
  let arr = [{}];
  return (arr).reduceRight(f2, v1);
}
%PrepareFunctionForOptimization(f6);

f6();
f6();
%OptimizeFunctionOnNextCall(f6);
f6();
