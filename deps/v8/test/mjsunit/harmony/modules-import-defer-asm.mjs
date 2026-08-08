// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --allow-natives-syntax --validate-asm

// Test that passing a deferred module namespace as the foreign argument to an
// asm.js module fails on linking because deferred namespaces accesses can
// trigger side-effects, and the module falls back to regular JS execution.

import defer * as ns from "data:text/javascript,export const foo = 42;";

function AsmModule(stdlib, foreign) {
  "use asm";
  var foo = foreign.foo | 0;
  function bar() { return foo | 0; }
  return { bar: bar };
}

var m = AsmModule(this, ns);
assertFalse(%IsAsmWasmCode(AsmModule));
assertEquals(42, m.bar());
