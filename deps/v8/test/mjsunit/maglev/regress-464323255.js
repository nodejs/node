// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* foo(flag) {
  if (flag) yield val;
  let val = 1;
}

for (let i = 0; i < 500; ++i) {
  for (let a of foo()) {}
}

function bar() {
  for (let b of foo(true)) {
    if (b) { assertUnreachable(); }
  }
}

assertThrows(() => bar());
