// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --validate-asm

// Compilation limits for WASM are not enforced for asm->wasm.
%SetWasmCompileControls(0, false);

function AsmModule() {
  "use asm";
  function xxx() { return 43; }
  function yyy() { return 43; }
  function zzz() { return 43; }
  function main() { return 43; }

  return {main: main};
}

assertEquals(43, AsmModule(
    undefined, undefined, new ArrayBuffer(1024)).main());
assertTrue(%IsAsmWasmCode(AsmModule));
