// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --no-lazy-feedback-allocation

function f0(a1) {
    function f2(a3) {
        switch (a3 | -6) {
            case 4:
            case 5:
                return 3;
        }
        return a3;
    }
    return f2(-2147483648);
}
%PrepareFunctionForOptimization(f0);
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
