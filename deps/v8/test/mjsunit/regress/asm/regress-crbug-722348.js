// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Module(global, env) {
  "use asm";
  var unused_fun = env.fun;
  function f() {}
  return { f:f }
}
assertThrows(() => Module(), TypeError);
assertFalse(%IsAsmWasmCode(Module));
