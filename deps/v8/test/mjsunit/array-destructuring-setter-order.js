// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --array-destructure-bytecode

let log = [];

function* gen() {
  log.push("gen 1");
  yield 1;
  log.push("gen 2");
  yield 2;
}

let obj = {
  set foo(val) {
    log.push("set foo: " + val);
  }
};

let y;
[obj.foo, y] = gen();

assertEquals(["gen 1", "set foo: 1", "gen 2"], log);
assertEquals(2, y);
