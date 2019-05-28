// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function opt(g) {
    for (var X = 0; X < 1; X++) {
        (new(function() {
            this.y
        })).x;
        (g || (g && (((g || -N)(g && 0))))).y = 0
    }
    (function() { g })
}
opt({});
%OptimizeFunctionOnNextCall(opt);
opt({});
