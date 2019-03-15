// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestTrailingJunkAfterExport() {
  function Module() {
    "use asm";
    function f() {}
    return {f: f}
    %kaboom;
  }
  assertThrows(() => Module(), ReferenceError);
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestExportWithSemicolon() {
  function Module() {
    "use asm";
    function f() {}
    return {f: f};
    // appreciate the semicolon
  }
  assertDoesNotThrow(() => Module());
  assertTrue(%IsAsmWasmCode(Module));
})();

(function TestExportWithoutSemicolon() {
  function Module() {
    "use asm";
    function f() {}
    return {f: f}
    // appreciate the nothingness
  }
  assertDoesNotThrow(() => Module());
  assertTrue(%IsAsmWasmCode(Module));
})();
