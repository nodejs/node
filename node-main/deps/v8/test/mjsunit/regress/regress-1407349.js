// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan

function main() {
  for (let v1 = 0; v1 < 4002; v1++) {
    const v3 = [-160421.17589718767];
    v3.constructor = v1;
    try {
      const v4 = (-9223372036854775807)();
    } catch(v5) {
    }
  }
}
main();
