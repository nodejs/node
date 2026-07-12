// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

function __f_0( __v_2) {
    arguments[1] = arguments;
}

%PrepareFunctionForOptimization(__f_0);
__f_0();
__f_0();
%OptimizeMaglevOnNextCall(__f_0);
__f_0();
