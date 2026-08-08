// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-lazy --stress-lazy-source-positions

function* f2(a3, a4, a5) {
    function F6() {
        if (!new.target) { throw 'must be called with new'; }
        with (4096) {
            try {
                this.getTimeZones(691, a4);
            } catch(e9) {
            }
        }
    }
    return a5;
}
