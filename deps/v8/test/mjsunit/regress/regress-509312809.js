// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const v2 = new Uint8Array(4294967296);
for (let v3 = 0; v3 < 25; v3++) {
    v2["p" + v3] = v3;
}

// Test load
v2["4294967295"];

// Test store
function store(arr) {
    arr["4294967295"] = 42;
}
store(v2);
store(v2);
