// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function Module(stdlib, env) {
  "use asm";
  var x = env.bar|0;
  return { foo: function(y) { return eval(1); } };
}
Module(this, {bar:0});
assertFalse(%IsAsmWasmCode(Module));
