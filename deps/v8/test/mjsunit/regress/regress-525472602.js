// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let o = new Array(1073741825);
o[0] = 1; // Force dictionary elements initially
let sep = {
  toString() {
    o.length = 0;
    o.push(1); // HOLEY_SMI_ELEMENTS
    return ",";
  }
};
assertThrows(() => o.join(sep), RangeError);
