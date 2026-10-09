// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

for (let i = 0; i < 1600; i++) eval("({k" + i + ": 1})");
function g() {
  return 42;
}
let o = {zz: g()};
assertEquals(42, o.zz);
