// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Module() {
  "use asm";
  function f() {}
  return f
}
eval("(" + Module.toString().replace(/;/, String.fromCharCode(8233)) + ")();");
assertFalse(%IsAsmWasmCode(Module));  // Valid asm.js, but we reject Unicode.
