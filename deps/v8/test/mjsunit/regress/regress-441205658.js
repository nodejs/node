// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --turboshaft-typed-optimizations

const v2 = Array(7);
for (let v4 = 0; v4 < 50; v4++) {
    function f5() {
        return "cKWCI" >= v2.at(v4);
    }
    f5();
}
const v9 = {};
