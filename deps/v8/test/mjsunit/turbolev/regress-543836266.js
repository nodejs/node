// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation --turbolev
// Flags: --turbolev-future --maglev-disable-builtin-reducers
// Flags: --no-concurrent-osr --no-maglev-osr

function wrap(f) {
  return f();
}

%OptimizeOsr();
for (let i = 0; i < 5; i++) {
  function inner() {}
  wrap(() => Math.round(inner));
}
const dead = 1;
