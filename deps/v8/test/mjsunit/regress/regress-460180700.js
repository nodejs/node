// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags:

const v0 = [-9223372036854775807,-35542842,-4294967295,-1726845946];
function f3(a4) {
    ~(!a4[100]);
}
v0[256] *= -2.9300044286524063;
for (let v9 = 0; v9 < 25; v9++) {
    f3(v0);
}
