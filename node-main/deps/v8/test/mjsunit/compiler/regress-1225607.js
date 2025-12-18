// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=1024

const v2 = {};
const v4 = {a:42};
function v8() {
    const v11 = v4.g;
}
function v13() {
    v4.g = v2;
}
const v22 = v13();
function v26() {
}
for (let v46 = 0; v46 < 100; v46++) {
    const v53 = v8();
}
v4.g = 42;
