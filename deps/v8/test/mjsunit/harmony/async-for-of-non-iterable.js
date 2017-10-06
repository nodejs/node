// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-async-iteration
var done = false;

async function f() {
    try {
        for await ([a] of {}) {
            UNREACHABLE();
        }
        UNREACHABLE();
    } catch (e) {
        assertEquals(e.message, "{} is not async iterable");
        done = true;
    }
}

f();
assertTrue(done);
