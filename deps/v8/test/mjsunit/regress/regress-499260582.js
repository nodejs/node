// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing --single-threaded

function f9(a10, a11) {
    'use strict';
    for (let v12 = 0; v12 < 5; v12++) {
        const v15 = ({ maxByteLength: 1073741824 }).maxByteLength;
        %OptimizeOsr();
        function F16(a18, a19, a20) {
            if (!new.target) { throw 'must be called with new'; }
        }
        try { F16.call(v15); } catch (e) {}
    }
    return f9;
}
%PrepareFunctionForOptimization(f9);
f9(f9, f9);
