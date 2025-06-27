// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:  --allow-natives-syntax --stress-compaction

// To reliably reproduce the crash use --verify-heap

function __f_2(o) {
  return o.field.b.x;
}

%PrepareFunctionForOptimization(__f_2);

try {
  %OptimizeFunctionOnNextCall(__f_2);
  __v_1 = __f_2();
} catch (e) { }

function __f_3() { __f_3(/./.test()); };

try {
  __f_3();
} catch (e) { }
