// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax --noenable-lazy-source-positions

function f1() {
    (true ? typeof~0 === "bigint" : 0) && 0;
}
%PrepareFunctionForOptimization(f1);
%OptimizeMaglevOnNextCall(f1);
f1();
