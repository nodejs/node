// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

function v0() {
    for (let i= 0; i < 4004; i++) {
        const v7 = 1 >>> i;
        const v9 = 32 - i;
        const v10 = 1<< v9;
        const v11 = v7 & v10;
    }
}
v0();
v0();
