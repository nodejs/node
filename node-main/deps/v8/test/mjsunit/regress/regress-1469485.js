// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --turboshaft --turboshaft-assert-types --allow-natives-syntax
// Flags: --no-always-sparkplug --no-stress-concurrent-inlining

function f7() {
}
const v18 = %PrepareFunctionForOptimization(f7);
const v20 = %OptimizeFunctionOnNextCall(f7);
f7();
