// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

for (let i = 100; i; (() => {    // IIFE
    --i;
    const regexp = /x[0-9][a-z]/i;
    function f() {
        %PretenureAllocationSite([{},1,arguments]);
    }
    f(0, -35066, -35066, 0, 0, 0, f, -35066, f, 0, f, 0, -35066, 0, 0, 0, -35066, f, 0,  regexp);
})()) {
}
