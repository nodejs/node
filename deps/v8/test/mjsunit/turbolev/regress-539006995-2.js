// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing --no-lazy-feedback-allocation --maglev-assert --turbolev --single-threaded

for (let v0 = 0; v0 < 5; v0++) {
    for (let v2 = 0; v2 < 5; v2++) {
        function f4(a5, ...a6) {
            a5.length = v0;
            try { a6.splice( v2); } catch (e) {}
            let v8;
            try { v8 = a6.findLast(); } catch (e) {}
            v8 ?? f4;
            v8 ?? v8;
            a5.prototype = a6;
            return a6;
        }
        f4(f4).includes(v0, v2);
    }
    %OptimizeOsr();
}
