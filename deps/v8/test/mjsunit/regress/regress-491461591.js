// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-lazy --stress-lazy-source-positions

async function* f0(a1, a2, a3) {
    function F4(a6, a7) {
        if (!new.target) { throw 'must be called with new'; }
    }
    function F8(a10, a11, a12) {
        if (!new.target) { throw 'must be called with new'; }
        try { this(F4); } catch (e) {}
    }
    return a3;
}
