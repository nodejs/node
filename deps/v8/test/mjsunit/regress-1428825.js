// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

for (let v0 = 0; v0 < 5; v0++) {
    const v1 = [];
    function f2() {
    }
    const v7 = +(v1 !== v1);
    function f8() {
        v1.length = 0;
    }
    const v12 = delete v1[1840008489];
    const v13 = v1.at(v7);
    for (let v14 = 0; v14 < 100; v14++) {
        f2(v12, v13, f8);
    }
}
