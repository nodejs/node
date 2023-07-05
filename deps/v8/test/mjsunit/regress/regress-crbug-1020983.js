// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-osr

function opt() {
    for (let i = 0; i < 5000; i++) {
        try {
                    try {
                        throw 1
                    } finally {
                        E
                    }
        } catch {}
    }
eval()
}
    opt()
