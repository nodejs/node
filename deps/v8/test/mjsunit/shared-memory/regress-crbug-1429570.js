// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

const t0 = this.SharedStructType(['a']);
const v6 = t0();
function f10(a11) {
  a11[1] = 0.1;
}
f10(Array());
f10(v6);
f10(v6);
assertEquals(undefined, v6[1]);
