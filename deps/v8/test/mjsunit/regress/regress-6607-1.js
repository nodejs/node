// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function get(a, i) {
  return a[i];
}

get([1,,3], 0);
get([1,,3], 2);
%OptimizeFunctionOnNextCall(get);
get([1,,3], 0);
assertOptimized(get);

// This unrelated change to the Array.prototype should be fine.
Array.prototype.unrelated = 1;
assertOptimized(get);
