// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function bar(a) {
  let ret;
  for (let i = 0; i < 2; i++) ret = a[i];
  return [ret];
}

const arr = [0.0, , 14.14];

%PrepareFunctionForOptimization(bar);
bar(arr);
%OptimizeFunctionOnNextCall(bar);

const result = bar(arr);
const withoutHoles = result.filter(x => true);
assertEquals(1, withoutHoles.length);
assertEquals(undefined, withoutHoles[0]);
