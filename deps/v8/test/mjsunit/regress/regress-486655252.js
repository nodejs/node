// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-lazy-source-positions --stress-lazy

class C1 {
    static #valueOf(a3) {
        for (let i6 = (() => {
                C1.#valueOf("CST", C1, this);
                return 0;
            })();
            8;
            ) {
        }
    }
}
