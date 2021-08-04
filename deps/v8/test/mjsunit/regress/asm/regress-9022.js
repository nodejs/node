// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function Module(stdlib, ffi) {
  "use asm";
  var log = ffi.log;
  function boom() {
    while (1) {
      label: {
        break;
      }
      log(1);
      break;
    }
    log(2);
  }
  return { boom: boom }
}

var log_value = 0;
function log(i) {
  log_value += i;
}

Module({}, { log: log }).boom();
assertTrue(%IsAsmWasmCode(Module));
assertEquals(2, log_value);
