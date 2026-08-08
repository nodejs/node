// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert-types --turbolev

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

let i = 1;
for (let j = i; j >= i; j += i) {
  const __v_6 = j++;
  %OptimizeOsr();
  if (__v_6 === null) ;
  i = i && j;
  if (j > 900000000) break;
}
