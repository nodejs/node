// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev-assert-types

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

let v1 = 1;
let i = 0;
for (let v2 = v1; v2 >= v1; v2 += v1) {
  const __v_43 = v2++;
  __v_43?.["foo"];
  %OptimizeOsr();
  v1 = v1 && v2;
  if (++i > 1000) break;
}
