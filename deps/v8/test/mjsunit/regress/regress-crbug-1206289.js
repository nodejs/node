// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-bytecode

function outer() {
  function asm_broken(a, import_obj) {
    "use asm";
    // import_obj is expected to be an object and this causes
    // asm_wasm_broken to be set to true when instantiating at runtime.
    var v = import_obj.x;
    function inner() {
    }
    return inner;
  }
  var m = asm_broken();
}

assertThrows(outer);
gc();
assertThrows(outer);
