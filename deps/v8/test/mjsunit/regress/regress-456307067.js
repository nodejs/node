// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function opt_me() {
    let k = 6;
    for (let i=0; i<100; i++) { // Maglev OSR Optimization trigger
        switch (k) {
            case 2:
                k = 60860;
            case 3:
                break;
            case 4:
                k = g;
        }
    }

    // The declaration is hoisted to the Temporal Dead Zone,
    // and the value of g is the_hole before execution reaches this point.
    const g = undefined;
}
opt_me();
