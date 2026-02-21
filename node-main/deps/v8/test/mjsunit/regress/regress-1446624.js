// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

for (let v3 = 0; v3 < 25; v3++) {
    function f4() {
    }
    const v9 = Array(2);
    try {
        v9.reduceRight(f4);
    } catch(e11) {
    }
}
