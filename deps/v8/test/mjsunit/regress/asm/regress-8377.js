// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(global, env, buffer) {
  "use asm";
  function test1() {
    var x = 0;
    x = -1 / 1 | 0;
    return x | 0;
  }
  function test2() {
    var x = 0;
    x = (-1 / 1) | 0;
    return x | 0;
  }
  return { test1: test1, test2: test2 };
};
let module = Module(this);
assertEquals(-1, module.test1());
assertEquals(-1, module.test2());
