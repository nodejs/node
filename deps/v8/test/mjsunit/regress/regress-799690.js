// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=500

function asm() {
  "use asm";
  function f(a) {
    a = a | 0;
    while (1) return 1;
    return 0;
  }
  return { f: f};
}
const mod = asm();
function call_f() {
  mod.f();
  call_f();
}
assertThrows(call_f, RangeError);
