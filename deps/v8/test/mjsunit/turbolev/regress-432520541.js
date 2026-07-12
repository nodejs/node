// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev-non-eager-inlining --max-maglev-inlined-bytecode-size-small=0

function bar(x) {
  if (!x) throw new "assertion failure";
}
const N = 1000;
function foo() {
  class C {
    static hello() {
      return 10;
    }
  };
  bar(C.hello() === 10);
}

for (let i = 0; i < N; i++) {
  foo();
}
