// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function CallCollisionFirstTableThenFunction() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      g[a & 0]();
      g();
    }
    function g() {}
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertThrows(() => m.f(), TypeError);
})();

(function CallCollisionFirstFunctionThenTable() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      g();
      g[a & 0]();
    }
    function g() {}
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertThrows(() => m.f(), TypeError);
})();

(function CallCollisionFunctionAsTable() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function g() {}
    function f(a) {
      a = a | 0;
      g[a & 0]();
    }
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertThrows(() => m.f(), TypeError);
})();

(function CallCollisionImportAsTable() {
  function Module(stdlib, imports, heap) {
    "use asm";
    var g = imports.g;
    function f(a) {
      a = a | 0;
      g[a & 0]();
    }
    return { f:f };
  }
  var m = Module(this, { g:Object });
  assertFalse(%IsAsmWasmCode(Module));
  assertThrows(() => m.f(), TypeError);
})();
